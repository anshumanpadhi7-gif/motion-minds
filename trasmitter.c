/*
 *  NODE 1  --  MPU9250 sensor  +  ESP-NOW transmitter
 *  Board: ESP32 (DevKit V1 / WROOM-32)
 *
 *  I2C   : SDA = GPIO21   SCL = GPIO22      (the ESP32 defaults)
 *  Radio : ESP-NOW, broadcast. No router, no password, no MAC lookup, no wires.
 *  USB serial @115200 stays free for debug.
 *
 *           ESP32 DevKit V1                  MPU9250
 *           +-------------+
 *     3V3 --|             |-- GPIO21 --------- SDA
 *     GND --|    ESP32    |-- GPIO22 --------- SCL
 *           |             |-- 3V3 ----------- VCC
 *           +-------------+-- GND ----------- GND, AD0
 *
 *  Note: ESP-NOW on ESP32 uses <esp_now.h> and needs an esp_now_peer_info_t,
 *  unlike the ESP8266's simpler esp_now_add_peer(mac, role, channel, ...).
 */

#include <Wire.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <esp_idf_version.h>

#ifndef ESP_ARDUINO_VERSION_MAJOR
#define ESP_ARDUINO_VERSION_MAJOR 2      // older cores predate this macro
#endif

// ---------------------------------------------------------------------------
// The ESP-NOW callback signatures have changed TWICE, and NOT in step with the
// Arduino core version. Guard on the IDF version, which is what actually
// defines the typedefs:
//
//   IDF >= 5.5   (Arduino-ESP32 3.3+)   send: wifi_tx_info_t   recv: esp_now_recv_info_t
//   IDF >= 5.0   (Arduino-ESP32 3.0+)   send: uint8_t *        recv: esp_now_recv_info_t
//   IDF  4.4     (Arduino-ESP32 2.x)    send: uint8_t *        recv: uint8_t *
// ---------------------------------------------------------------------------
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 5, 0)
  #define SEND_CB_ARGS  const wifi_tx_info_t *tx_info, esp_now_send_status_t status
#else
  #define SEND_CB_ARGS  const uint8_t *mac_addr, esp_now_send_status_t status
#endif

#define MPU_ADDR   0x68          // AD0 -> GND.  Use 0x69 if AD0 -> 3V3
#define SDA_PIN    21
#define SCL_PIN    22

// ---- direction codes on the air ----------------------------------------
const uint8_t DIR_NONE  = 'N';
const uint8_t DIR_POS_X = 'A';   // tilted right
const uint8_t DIR_NEG_X = 'B';   // tilted left
const uint8_t DIR_POS_Y = 'C';   // tilted forward
const uint8_t DIR_NEG_Y = 'D';   // tilted backward

// ---- tilt thresholds, in DEGREES away from the flat 0-degree rest position
const float TH_ON_DEG  = 45.0f;  // tilt past this  -> an LED lights
const float TH_OFF_DEG = 35.0f;  // come back inside this -> all LEDs off
const float EMA        = 0.25f;  // low-pass filter, 0.05 = smooth .. 0.5 = twitchy

// Any tilt from TH_ON_DEG up to a full 90 degrees (board on edge) lights the
// LED for that direction.
//   threshold  30 deg = 0.50 g   45 deg = 0.71 g   60 deg = 0.87 g   90 deg = 1.00 g

// ---- MOUNTING AXIS MAPPING ---------------------------------------------
// Which physical tilt counts as "left" depends on how the module is rotated
// on your board, and that is not something the code can know by itself.
//
// Two knobs cover all 8 ways it can be mounted. Run direction_mapper.ino on
// this board once; it measures each tilt and prints exactly what to put here.
const bool  SWAP_XY  = false;    // true if left/right tilt lights the
                                 // forward/backward LED (axes interchanged)
const float INVERT_X =  1.0f;    // -1 if left and right come out swapped
const float INVERT_Y =  1.0f;    // -1 if forward and backward come out swapped

uint8_t current = DIR_NONE;
uint8_t pending = DIR_NONE;
uint8_t pendingCount = 0;
float   fx = 0, fy = 0, fz = 0;
unsigned long lastSend = 0;

// A real tilt persists; a hand-shake jolt does not. Require the new direction
// to win 3 times in a row (3 x 50 ms = 150 ms) before we believe it. 1 = off.
const uint8_t CONFIRM = 3;

// Broadcast - every ESP-NOW device on this channel hears us.
uint8_t bcAddr[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

volatile uint32_t sendFails = 0;

// We only care about the status here, never the destination MAC, so the two
// possible first parameters are both simply ignored.
void onSent(SEND_CB_ARGS) {
  if (status != ESP_NOW_SEND_SUCCESS) sendFails++;
}

// ------------------------------------------------------------------------
void mpuWrite(uint8_t reg, uint8_t val) {
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(reg);
  Wire.write(val);
  Wire.endTransmission();
}

bool mpuInit() {
  Wire.begin(SDA_PIN, SCL_PIN);
  delay(100);

  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x75);                       // WHO_AM_I
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom((uint8_t)MPU_ADDR, (uint8_t)1) != 1) return false;
  uint8_t who = Wire.read();
  Serial.printf("WHO_AM_I = 0x%02X  (0x71/0x73 = MPU9250/9255, 0x70 = MPU6500 clone)\n", who);

  mpuWrite(0x6B, 0x01);   // PWR_MGMT_1  : wake up, clock = PLL (gyro X)
  delay(50);
  mpuWrite(0x1C, 0x00);   // ACCEL_CONFIG: +-2g   -> 16384 LSB per g
  mpuWrite(0x1A, 0x03);   // CONFIG      : DLPF ~41 Hz, kills vibration noise
  delay(50);
  return true;
}

bool mpuReadAccel(float &ax, float &ay, float &az) {
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x3B);                       // ACCEL_XOUT_H
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom((uint8_t)MPU_ADDR, (uint8_t)6) != 6) return false;

  uint16_t raw[3];
  for (int i = 0; i < 3; i++) {
    uint16_t hi = Wire.read();
    uint16_t lo = Wire.read();
    raw[i] = (hi << 8) | lo;
  }
  ax = (int16_t)raw[0] / 16384.0f;
  ay = (int16_t)raw[1] / 16384.0f;
  az = (int16_t)raw[2] / 16384.0f;
  return true;
}

// ---- MOUNTING ----------------------------------------------------------
// The module is mounted FLAT, so at rest it reads (0, 0, +1 g) and no axis
// remapping is needed - do NOT add one, or you will rotate the axes twice.
//
// If you ever mount it on its side, gravity moves onto another axis and
// tiltDegrees() then reports ~84 degrees while the board sits perfectly
// still, permanently lighting a direction. In that case rotate the readings
// so the rest position lands back on +Z:
//      float nx = ay, ny = az, nz = ax;   ax = nx; ay = ny; az = nz;
// (A proper rotation, determinant +1, so handedness is preserved.)

// How far the board is tipped away from flat, in degrees.
// 0 = lying flat.  90 = standing on edge (or upside down = 180).
// Uses all three axes, so a DIAGONAL tilt measures the same as a straight one.
float tiltDegrees(float ax, float ay, float az) {
  return atan2f(sqrtf(ax * ax + ay * ay), fabsf(az)) * RAD_TO_DEG;
}

// HOW FAR it is tipped decides WHETHER an LED lights.
// WHICH axis dominates decides WHICH LED lights.
uint8_t decide(float ax, float ay, float az, uint8_t prev) {
  float th = (prev == DIR_NONE) ? TH_ON_DEG : TH_OFF_DEG;

  // Back at (or near) the 0-degree rest position -> every LED off.
  // Note the remap below does not affect this: it is a rotation, so
  // sqrt(ax^2 + ay^2) is the same either way.
  if (tiltDegrees(ax, ay, az) < th) return DIR_NONE;

  // Apply the mounting correction, then classify.
  float x = (SWAP_XY ? ay : ax) * INVERT_X;
  float y = (SWAP_XY ? ax : ay) * INVERT_Y;

  if (fabsf(x) >= fabsf(y))
    return (x > 0) ? DIR_POS_X : DIR_NEG_X;      // +X = right, -X = left
  return (y > 0) ? DIR_POS_Y : DIR_NEG_Y;        // +Y = forward, -Y = backward
}

// ------------------------------------------------------------------------
void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println("\n[Node 1] MPU9250 sensor + ESP-NOW transmitter (ESP32)");

  // ---- radio ----------------------------------------------------------
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();                     // we never join a network
  esp_wifi_set_ps(WIFI_PS_NONE);         // ESP32: no power save = steady latency

  Serial.print("Node 1 MAC: ");
  Serial.println(WiFi.macAddress());

  if (esp_now_init() != ESP_OK) {
    Serial.println("!! ESP-NOW init failed");
    while (true) delay(1000);
  }

  esp_now_peer_info_t peer = {};
  memcpy(peer.peer_addr, bcAddr, 6);
  peer.channel = 0;                      // 0 = whatever channel we are on
  peer.encrypt = false;
  peer.ifidx   = WIFI_IF_STA;
  if (esp_now_add_peer(&peer) != ESP_OK) {
    Serial.println("!! esp_now_add_peer failed");
    while (true) delay(1000);
  }
  esp_now_register_send_cb(onSent);
  Serial.println("ESP-NOW up, broadcasting");

  // ---- sensor ---------------------------------------------------------
  if (!mpuInit()) {
    Serial.println("!! MPU9250 not answering - check SDA/SCL/3V3/GND and AD0 strap");
    while (true) delay(1000);
  }
  Serial.println("MPU9250 ready. Tilt the board...");
}

void loop() {
  float ax, ay, az;

  // ---- read + filter at full speed -----------------------------------
  if (mpuReadAccel(ax, ay, az)) {
    fx += EMA * (ax - fx);
    fy += EMA * (ay - fy);
    fz += EMA * (az - fz);
  }

  // ---- decide + transmit 20x per second -------------------------------
  if (millis() - lastSend >= 50) {
    lastSend = millis();

    uint8_t want = decide(fx, fy, fz, current);

    if (want == current) {
      pendingCount = 0;                       // no change, nothing to confirm
    } else if (want == pending && ++pendingCount >= CONFIRM) {
      current = want;                         // agreed long enough - commit it
      pendingCount = 0;
    } else if (want != pending) {
      pending = want;
      pendingCount = 1;
    }

    esp_now_send(bcAddr, &current, 1);        // one byte over the air

    Serial.printf("tilt=%5.1f deg  ax=%+.2f ay=%+.2f az=%+.2f  ->  %c  (fails=%lu)\n",
                  tiltDegrees(fx, fy, fz), fx, fy, fz, current,
                  (unsigned long)sendFails);
  }
}