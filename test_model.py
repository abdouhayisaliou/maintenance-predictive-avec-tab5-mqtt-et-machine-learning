import joblib
import pandas as pd

model = joblib.load("cnc_predictive_model.pkl")

sample = pd.DataFrame([{
    "temp": 85,
    "vibration": 0.95,
    "pressure": 145,
    "current": 7.5,
    "coolant": 5
}])

prediction = model.predict(sample)

print("Prédiction :", prediction[0])