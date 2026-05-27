import pandas as pd
import numpy as np
import matplotlib.pyplot as plt

# Load data
data = pd.read_csv('DATA\Data_MaskMeterial(polypropylene)_11.11.2025\sensor_data_3ok.csv', encoding='latin1')
print(data.head())

# Extract and prepare data
y1 = data['Humidity (%)'].values
raw_y2 = data['Paper Sensor Value'].values

# Reverse y2
y2 = -raw_y2

# Smooth reversed y2 by averaging every 10 data points
smoothed_y2 = []
window_size = 10
for i in range(0, len(y2), window_size):
    chunk = y2[i:i + window_size]
    if len(chunk) == window_size:
        avg = sum(chunk) / window_size
        smoothed_y2.extend([avg] * window_size)

# Match lengths
min_len = min(len(y1), len(smoothed_y2))
y1 = y1[:min_len]
smoothed_y2 = smoothed_y2[:min_len]
x = np.arange(min_len)

# Start plotting
fig, ax1 = plt.subplots(figsize=(12, 6))

# Plot raw y1 (Humidity)
#--------------------------------------------------------------------------------#
#ax1.plot(x, y1, 'o-', label="DHT11", color='blue')
p1 = np.polyfit(x, y1, 2)
y1_fit = np.polyval(p1, x)
ax1.plot(x, y1_fit, '-', label="Fit: DHT11", color='navy', linewidth=2)

ax1.set_xlabel('Time (sec.)', fontsize=20, fontweight='bold')
ax1.set_ylabel('Humidity (DHT11)', fontsize=20, fontweight='bold', color='blue')
ax1.tick_params(axis='y', labelcolor='blue')
ax1.grid(True)

# Plot smoothed y2
ax2 = ax1.twinx()
#--------------------------------------------------------------------------------#
#ax2.plot(x, smoothed_y2, 'o-', label="Smoothed Paper Sensor", color='#FF7F50')
p2 = np.polyfit(x, smoothed_y2, 2)
y2_fit = np.polyval(p2, x)
ax2.plot(x, y2_fit, '-', label="Fit: Paper Sensor", color='darkorange', linewidth=2)

ax2.set_ylabel('Sensor Value', fontsize=20, fontweight='bold', color='#FF7F50')
ax2.tick_params(axis='y', labelcolor='#FF7F50')

# Title and show
plt.title('Sensor Data with Averaging and Best-Fit Curves', fontsize=20, fontweight='bold')
plt.tight_layout()
plt.show()
