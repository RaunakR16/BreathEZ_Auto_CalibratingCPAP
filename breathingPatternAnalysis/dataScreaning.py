import pandas as pd
import matplotlib.pyplot as plt

# -------- LOAD CSV --------
file_path = r"C:\Users\rauna\Downloads\4th_YearProject-1\CODES\breathingPatternAnalysis\DATA\sensor_log1.csv"

df = pd.read_csv(file_path)

# -------- CONVERT TIME --------
df['Timestamp'] = pd.to_datetime(df['Timestamp'])

# -------- PLOT --------
plt.figure(figsize=(16,8))

# Raw ADC
plt.plot(df['Timestamp'], df['Raw_Value'], label='Raw ADC')

# Filtered ADC
plt.plot(df['Timestamp'], df['Filtered_Value'], label='Filtered ADC')

# -------- GRAPH SETTINGS --------
plt.xlabel("Time")
plt.ylabel("Sensor ADC Value")
# plt.title("Raw vs Filtered ADC Data")

plt.legend()
plt.grid(True)

# Rotate time labels
plt.xticks(rotation=45)

plt.tight_layout()

# -------- SHOW --------
plt.show()