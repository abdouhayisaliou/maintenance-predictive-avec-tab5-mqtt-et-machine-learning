import json
import csv
import os
from datetime import datetime

import pandas as pd
import joblib
import paho.mqtt.client as mqtt


BROKER = "test.mosquitto.org"
PORT = 1883

TOPIC_DATA = "factory/cnc/data"
TOPIC_PREDICTION = "factory/cnc/prediction"

MODEL_FILE = "cnc_predictive_model.pkl"

model = joblib.load(MODEL_FILE)


def calculate_health_score(data):
    temp = float(data.get("spindle_temp", 0))
    vibration = float(data.get("vibration", 0))
    pressure = float(data.get("hydraulic_pressure", 0))
    current = float(data.get("motor_current", 0))
    coolant = float(data.get("coolant_flow", 10))

    health_score = 100
    health_score -= max(0, temp - 55) * 0.7
    health_score -= vibration * 18
    health_score -= max(0, pressure - 100) * 0.25
    health_score -= max(0, current - 5) * 4
    health_score -= max(0, 8 - coolant) * 6

    return round(max(0, min(100, health_score)))


def predict_state(data):
    temp = float(data.get("spindle_temp", 0))
    vibration = float(data.get("vibration", 0))
    pressure = float(data.get("hydraulic_pressure", 0))
    current = float(data.get("motor_current", 0))
    coolant = float(data.get("coolant_flow", 10))

    sample = pd.DataFrame([{
        "temp": temp,
        "vibration": vibration,
        "pressure": pressure,
        "current": current,
        "coolant": coolant
    }])

    prediction = model.predict(sample)[0]
    health_score = calculate_health_score(data)

    return {
        "machine": data.get("machine", "CNC_MAZAK_SIM"),
        "prediction": prediction,
        "confidence": 95,
        "health_score": health_score,
        "reason": get_reason(prediction),
        "timestamp": datetime.utcnow().isoformat()
    }


def get_reason(prediction):
    if prediction == "NORMAL":
        return "Fonctionnement stable"
    if prediction == "FATIGUE":
        return "Signes de fatigue machine"
    if prediction == "DEGRADE":
        return "Dérive de fonctionnement détectée"
    if prediction == "CRITIQUE":
        return "Risque d'arrêt machine détecté"
    return "État inconnu"


def save_to_csv(data, prediction):
    filename = "machine_history.csv"
    file_exists = os.path.isfile(filename)

    with open(filename, "a", newline="", encoding="utf-8") as csvfile:
        writer = csv.writer(csvfile)

        if not file_exists:
            writer.writerow([
                "timestamp",
                "machine",
                "temp",
                "vibration",
                "pressure",
                "current",
                "coolant",
                "health_score",
                "prediction"
            ])

        writer.writerow([
            prediction["timestamp"],
            data.get("machine"),
            data.get("spindle_temp"),
            data.get("vibration"),
            data.get("hydraulic_pressure"),
            data.get("motor_current"),
            data.get("coolant_flow"),
            prediction["health_score"],
            prediction["prediction"]
        ])


def on_connect(client, userdata, flags, rc):
    print("Connecté au broker MQTT avec code :", rc)

    if rc == 0:
        client.subscribe(TOPIC_DATA)
        print("En écoute sur :", TOPIC_DATA)
    else:
        print("Erreur de connexion MQTT")


def on_message(client, userdata, msg):
    try:
        raw_payload = msg.payload.decode("utf-8").strip()

        if not raw_payload:
            return

        try:
            data = json.loads(raw_payload)
        except json.JSONDecodeError:
            return

        prediction = predict_state(data)

        save_to_csv(data, prediction)

        client.publish(
            TOPIC_PREDICTION,
            json.dumps(prediction)
        )

        print("Données reçues :", data)
        print("Prédiction ML envoyée :", prediction)
        print("-" * 60)

    except Exception as e:
        print("Erreur :", e)


client = mqtt.Client()

client.on_connect = on_connect
client.on_message = on_message

print("Chargement du modèle :", MODEL_FILE)
print("Connexion au broker MQTT...")

client.connect(BROKER, PORT, 60)
client.loop_forever()