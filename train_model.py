import pandas as pd
import joblib

from sklearn.model_selection import train_test_split
from sklearn.ensemble import RandomForestClassifier
from sklearn.metrics import classification_report, confusion_matrix, accuracy_score


CSV_FILE = "machine_history.csv"
MODEL_FILE = "cnc_predictive_model.pkl"


df = pd.read_csv(CSV_FILE)

print("Colonnes disponibles :")
print(df.columns)

print("\nRépartition des classes :")
print(df["prediction"].value_counts())


features = [
    "temp",
    "vibration",
    "pressure",
    "current",
    "coolant"
]

target = "prediction"

df = df.dropna(subset=features + [target])

X = df[features]
y = df[target]

X_train, X_test, y_train, y_test = train_test_split(
    X,
    y,
    test_size=0.25,
    random_state=42,
    stratify=y
)

model = RandomForestClassifier(
    n_estimators=150,
    random_state=42,
    class_weight="balanced"
)

model.fit(X_train, y_train)

y_pred = model.predict(X_test)

print("\n===== RÉSULTATS =====")
print("Accuracy :", accuracy_score(y_test, y_pred))

print("\n===== RAPPORT DE CLASSIFICATION =====")
print(classification_report(y_test, y_pred))

print("\n===== MATRICE DE CONFUSION =====")
print(confusion_matrix(y_test, y_pred))

joblib.dump(model, MODEL_FILE)

print(f"\nModèle sauvegardé dans : {MODEL_FILE}")