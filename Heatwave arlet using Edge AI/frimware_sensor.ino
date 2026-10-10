firmware/heatsafe_sensor.ino.


#include <Wire.h>
#include <Adafruit_SHT31.h>
#include <math.h>

Adafruit_SHT31 sensor = Adafruit_SHT31();

const unsigned long READ_INTERVAL_MS = 1000;
unsigned long lastRead = 0;

void setup() {
  Serial.begin(115200);
  delay(1500);

  Wire.begin();

  if (!sensor.begin(0x44)) {
    Serial.println("{\"error\":\"SHT31 not found at 0x44\"}");
    while (true) {
      delay(1000);
    }
  }

  Serial.println("{\"status\":\"sensor_ready\"}");
}

void loop() {
  unsigned long now = millis();

  if (now - lastRead < READ_INTERVAL_MS) {
    return;
  }

  lastRead = now;

  float temp = sensor.readTemperature();
  float humidity = sensor.readHumidity();

  bool valid =
      !isnan(temp) &&
      !isnan(humidity) &&
      temp >= -40.0 &&
      temp <= 125.0 &&
      humidity >= 0.0 &&
      humidity <= 100.0;

  if (!valid) {
    Serial.println("{\"error\":\"invalid_reading\"}");
    return;
  }

  Serial.print("{\"temperature_c\":");
  Serial.print(temp, 2);
  Serial.print(",\"humidity_pct\":");
  Serial.print(humidity, 2);
  Serial.println("}");
}