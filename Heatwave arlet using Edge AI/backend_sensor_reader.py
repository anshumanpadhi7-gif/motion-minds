backend/sensor_reader.py.

import json
import time
import serial


def read_sensor_stream(port, baudrate=115200):
    """
    Reads newline-delimited JSON from a USB serial connection.
    The firmware must emit temperature_c and humidity_pct fields.
    """
    while True:
        try:
            with serial.Serial(port, baudrate, timeout=2) as device:
                print(f"Connected to sensor on {port}")

                while True:
                    line = device.readline().decode(
                        "utf-8", errors="replace"
                    ).strip()

                    if not line:
                        continue

                    try:
                        data = json.loads(line)
                    except json.JSONDecodeError:
                        continue

                    temp = data.get("temperature_c")
                    humidity = data.get("humidity_pct")

                    if temp is None or humidity is None:
                        continue

                    yield {
                        "temperature_c": float(temp),
                        "humidity_pct": float(humidity)
                    }

        except (serial.SerialException, OSError) as error:
            print(f"Sensor connection error: {error}")
            time.sleep(3)