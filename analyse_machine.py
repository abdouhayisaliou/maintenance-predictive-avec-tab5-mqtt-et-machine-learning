import pandas as pd
import matplotlib.pyplot as plt

df = pd.read_csv("machine_history.csv")

print("\n===== STATISTIQUES =====\n")

print(df.describe())

print("\n===== ETATS =====\n")

print(df["prediction"].value_counts())

plt.figure(figsize=(12,5))

plt.plot(df["health_score"])

plt.title("Indice de santé machine")

plt.ylabel("Santé (%)")

plt.xlabel("Mesures")

plt.grid(True)

plt.show()