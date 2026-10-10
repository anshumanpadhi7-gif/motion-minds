backend/predictor.py. 

def predict_temperature(history):
    """
    Baseline forecast for the next 15 minutes.
    This is NOT a trained AI model.

    history: list of readings with temperature_c values.
    """
    valid = [
        item["temperature_c"]
        for item in history
        if isinstance(item.get("temperature_c"), (int, float))
    ]

    if not valid:
        return None

    return {
        "horizon_minutes": 15,
        "predicted_temperature_c": round(valid[-1], 2),
        "model": "persistence_baseline",
        "is_trained_ai": False
    }