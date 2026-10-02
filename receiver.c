/*
 *  NODE 2  --  ESP-NOW receiver  +  4 LEDs
 *  Board: ESP32 (DevKit V1 / WROOM-32)
 *
 *  Radio : ESP-NOW. Listens for one byte from Node 1.
 *  LEDs  : GPIO21, GPIO23, GPIO19, GPIO18  ->  220R  ->  LED anode,
 *          LED cathode -> GND.   No wires to Node 1 at all.
 *
 *           ESP32 DevKit V1
 *           +-------------+
 *  GPIO21 --|             |   right     tilt
 *  GPIO23 --|    ESP32    |   left      tilt
 *  GPIO19 --|             |   forward   tilt
 *  GPIO18 --|             |   backward  tilt
 *     GND --|             |-- all four LED cathodes
 *           +-------------+
 *
 *  This board only breaks out: 2, 4, 5, 18, 19, 21, 22, 23. Which to avoid:
 *
 *    GPIO2 - carries the ONBOARD LED. Driving it lights both your external LED
 *            and the module's own, and there is no way to separate them. It is
 *            also a boot-strapping pin. Never use it for anything.
 *    GPIO5 - boot-strapping pin.
 *    GPIO4 - usable, but it appears in the strapping group on some ESP32
 *            datasheets, so it is the last resort if you need a fifth pin.
 *
 *  21/22 are also the ESP32's default I2C pins. That does not matter here -
 *  Node 2 has no I2C devices, and Node 1 is a separate board.
 */

#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <esp_idf_version.h>

#ifndef ESP_ARDUINO_VERSION_MAJOR
#define ESP_ARDUINO_VERSION_MAJOR 2      // older cores predate this macro
#endif

// The ESP-NOW receive callback signature changed with ESP-IDF 5.0
// (Arduino-ESP32 3.x). Guard on the IDF version, not the core version.
// We never look at the sender's MAC, so the two forms differ only in a first
// parameter that we ignore either way.
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
  #define RECV_CB_ARGS  const esp_now_recv_info_t *info, const uint8_t *data, int len
#else
  #define RECV_CB_ARGS  const uint8_t *mac, const uint8_t *data, int len
#endif

#define LED_XP   21   // tilt right  (was GPIO2 - that pin carries the onboard LED)
#define LED_XN   23   // tilt left   (GPIO22 delivered nothing - see note above)
#define LED_YP   19  // tilt forward
#define LED_YN   18   // tilt backward

// Written from the ESP-NOW callback (runs in the WiFi task), read in loop(),
// so these must be volatile and are copied into locals before use.
volatile uint8_t dir = 'N';
volatile unsigned long lastRx = 0;

// ------------------------------------------------------------------------
// One function, valid on every core version because RECV_CB_ARGS expands to
// whichever signature this IDF defines.
// ------------------------------------------------------------------------
void onRecv(RECV_CB_ARGS) {
  if (len < 1) return;
  uint8_t c = data[0];
  if (c == 'A' || c == 'B' || c == 'C' || c == 'D' || c == 'N') {
    dir = c;
    lastRx = millis();
  }
}

void show(uint8_t d) {
  digitalWrite(LED_XP, d == 'A');
  digitalWrite(LED_XN, d == 'B');
  digitalWrite(LED_YP, d == 'C');
  digitalWrite(LED_YN, d == 'D');
}

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.printf("\n[Node 2] ESP-NOW receiver + 4 LEDs (ESP32, core %d.x)\n",
                ESP_ARDUINO_VERSION_MAJOR);

  pinMode(LED_XP, OUTPUT);
  pinMode(LED_XN, OUTPUT);
  pinMode(LED_YP, OUTPUT);
  pinMode(LED_YN, OUTPUT);
  show('N');                             // all off

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();                     // we never join a network
  esp_wifi_set_ps(WIFI_PS_NONE);         // ESP32: no power save = steady latency

  Serial.print("Node 2 MAC: ");
  Serial.println(WiFi.macAddress());     // needed only if you switch to unicast

  if (esp_now_init() != ESP_OK) {
    Serial.println("!! ESP-NOW init failed");
    while (true) delay(1000);
  }

  // Not strictly needed just to RECEIVE a broadcast, but harmless and it
  // makes the intent explicit.
  esp_now_peer_info_t peer = {};
  uint8_t bcAddr[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
  memcpy(peer.peer_addr, bcAddr, 6);
  peer.channel = 0;
  peer.encrypt = false;
  peer.ifidx   = WIFI_IF_STA;
  esp_now_add_peer(&peer);

  esp_now_register_recv_cb(onRecv);

  Serial.println("Listening...");
}

void loop() {
  uint8_t d = dir;                          // volatile -> local

  // Node 1 transmits every 50 ms.  Silence for 500 ms = link is dead -> all off.
  if (millis() - lastRx > 500) d = 'N';

  show(d);

  // Mirror the state on the USB serial monitor for debugging
  static uint8_t last = 0xFF;
  if (d != last) {
    last = d;
    Serial.printf("dir = %c\n", d);
  }
}