backend/app.py. 

from flask import Flask, jsonify, request, send_from_directory
from datetime import datetime, timezone
from collections import deque
from threading import Lock
import csv
import os

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
FRONTEND = os.path.join(ROOT, "frontend")
DATA_DIR = os.path.join(ROOT, "data")
CSV_PATH = os.path.join(DATA_DIR, "readings.csv")

os.makedirs(DATA_DIR, exist_ok=True)

app = Flask(__name__)
lock = Lock()
history = deque(maxlen=1000)
latest = None


def save_reading(temp, humidity, source="sensor"):
    global latest

    if not (-40 <= temp <= 85):
        raise ValueError("Temperature is outside configured limits")
    if not (0 <= humidity <= 100):
        raise ValueError("Humidity must be between 0 and 100")

    reading = {
        "timestamp": datetime.now(timezone.utc).isoformat(),
        "temperature_c": round(float(temp), 2),
        "humidity_pct": round(float(humidity), 2),
        "source": source
    }

    with lock:
        latest = reading
        history.append(reading)

        new_file = not os.path.exists(CSV_PATH)
        with open(CSV_PATH, "a", newline="", encoding="utf-8") as file:
            writer = csv.DictWriter(
                file,
                fieldnames=[
                    "timestamp", "temperature_c",
                    "humidity_pct", "source"
                ]
            )
            if new_file:
                writer.writeheader()
            writer.writerow(reading)

    return reading


def read_saved_history():
    if not os.path.exists(CSV_PATH):
        return []

    with open(CSV_PATH, newline="", encoding="utf-8") as file:
        return list(csv.DictReader(file))[-1000:]


@app.get("/")
def home():
    return send_from_directory(FRONTEND, "index.html")


@app.get("/<path:filename>")
def frontend_file(filename):
    return send_from_directory(FRONTEND, filename)


@app.get("/api/latest")
def api_latest():
    with lock:
        current = dict(latest) if latest else None

    return jsonify({
        "reading": current,
        "data_available": current is not None
    })


@app.get("/api/history")
def api_history():
    rows = read_saved_history()
    return jsonify({"readings": rows})


@app.get("/api/prediction")
def api_prediction():
    with lock:
        rows = list(history)

    if not rows:
        rows = [
            {
                "temperature_c": float(r["temperature_c"])
            }
            for r in read_saved_history()
        ]

    if not rows:
        return jsonify({
            "available": False,
            "message": "Waiting for sensor data"
        })

    latest_temp = float(rows[-1]["temperature_c"])

    return jsonify({
        "available": True,
        "horizon_minutes": 15,
        "predicted_temperature_c": round(latest_temp, 2),
        "model": "persistence_baseline",
        "is_trained_ai": False
    })


@app.post("/api/readings")
def api_add_reading():
    # For local integration/testing. Do not expose publicly without
    # authentication and suitable request validation.
    data = request.get_json(silent=True) or {}

    try:
        temp = float(data["temperature_c"])
        humidity = float(data["humidity_pct"])
        reading = save_reading(temp, humidity, source="api")
    except (KeyError, TypeError, ValueError) as error:
        return jsonify({"error": str(error)}), 400

    return jsonify({"saved": True, "reading": reading}), 201


@app.get("/api/health")
def api_health():
    with lock:
        has_data = latest is not None

    return jsonify({
        "application": "HeatSafe Edge",
        "backend": "running",
        "data_available": has_data
    })


if __name__ == "__main__":
    # Local development only; not a production deployment server.
    app.run(host="127.0.0.1", port=5000, debug=False)