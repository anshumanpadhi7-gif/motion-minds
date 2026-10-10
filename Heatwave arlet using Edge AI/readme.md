README.md.

# HeatSafe Edge

### Localized Heat Monitoring and Edge-AI Research Prototype

HeatSafe Edge is a proposed environmental monitoring system designed to measure local air temperature and relative humidity, maintain a record of readings, display trends, and support future heat-condition prediction on an edge device.

## Problem

General weather information may not represent the conditions at a particular rooftop, bus stop, workplace, classroom, or campus location. Localized environmental measurements can help institutions better understand their exposure to heat.

## Proposed solution

HeatSafe Edge combines:

- Arduino UNO Q and environmental sensors
- Local measurement processing and data storage
- A web dashboard for readings and trends
- A temperature-prediction baseline
- Planned Edge-AI model evaluation
- Planned local alerts and expanded heat assessment

## Current repository features

- SHT31 Arduino sensor-reading firmware
- Flask API for readings and history
- CSV logging
- Responsive dashboard
- Temperature-history chart
- Persistence forecast baseline
- Serial-reader module for USB bench testing

## Technology stack

- Hardware: Arduino UNO Q
- Firmware: Arduino C/C++
- Backend: Python and Flask
- Frontend: HTML, CSS, JavaScript
- Storage: CSV for the initial prototype
- Sensor interface: I2C
- Prediction: persistence baseline

## Repository structure

```text
heatsafe-edge/
├── backend/
│   ├── app.py
│   ├── predictor.py
│   └── sensor_reader.py
├── firmware/
│   └── heatsafe_sensor.ino
├── frontend/
│   ├── index.html
│   ├── style.css
│   └── app.js
├── data/
├── requirements.txt
├── .gitignore
└── README.md
```

## Run locally

Requires Python 3.10 or newer.

1. Install dependencies:

   ```bash
   python -m pip install -r requirements.txt
   ```

2. Start the application:

   ```bash
   python backend/app.py
   ```

3. Open:

   http://127.0.0.1:5000

4. Send a test reading from another terminal:

   ```bash
   curl -X POST http://127.0.0.1:5000/api/readings \
     -H "Content-Type: application/json" \
     -d '{"temperature_c":31.4,"humidity_pct":64}'
   ```

   This command inserts a test reading. It does not prove the hardware is connected.

## API endpoints

| Endpoint | Purpose |
|---|---|
| GET /api/health | Backend status |
| GET /api/latest | Latest reading |
| GET /api/history | Saved reading history |
| GET /api/prediction | Persistence baseline |
| POST /api/readings | Submit a validated reading |

## Hardware

The firmware expects an SHT31-compatible I2C sensor. Confirm the sensor address and wiring for the actual module before powering it.

## Current limitations

- The USB serial reader is not yet wired into the Flask application's startup.
- The current prediction is a persistence baseline, not a trained AI model.
- Full WBGT measurement requires suitable measurement inputs and validation.
- The dashboard does not yet implement a validated heat-risk classification.
- A public deployment needs security controls and production hosting.

## Validation plan

1. Confirm real sensor readings.
2. Compare measurements with a reference instrument.
3. Integrate the UNO Q microcontroller and Linux application.
4. Collect representative timestamped data.
5. Compare prediction models on unseen time periods.
6. Test local operation without external internet.

## Safety note

This is a research prototype, not a medical diagnostic system or a certified occupational heat-safety instrument.

## References

- Arduino UNO Q documentation: https://docs.arduino.cc/hardware/uno-q/
- Arduino UNO Q user manual: https://docs.arduino.cc/tutorials/uno-q/user-manual/
- Flask documentation: https://flask.palletsprojects.com/
- ISO 7243: https://www.iso.org/standard/67188.html
-