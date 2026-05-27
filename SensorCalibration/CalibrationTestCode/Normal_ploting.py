import pandas as pd
import matplotlib.pyplot as plt

data = pd.read_csv('DATA\odbhut_data_day_2\sensor_data.csv', encoding='latin1')
print(data.head())

# Rest of your code remains the same
y1 = data['Humidity (%)'].values
y2 = data['Sensor Value'].values

fig, ax1 = plt.subplots(figsize=(12, 6))

ax1.plot(y1, 'o-', label="DHT 11", color='blue')
ax1.set_xlabel('Time (sec.)', fontsize=20, fontweight='bold')
ax1.set_ylabel('Humidity (DHT11)', fontsize=20, fontweight='bold', color='blue')
ax1.tick_params(axis='y', labelcolor='blue')
ax1.grid(True)

ax2 = ax1.twinx()
ax2.plot(y2, 'o-', label="Paper-based sensor", color='#FF7F50')
ax2.set_ylabel('Serial value (Paper-based sensor)', fontsize=20, fontweight='bold', color='#FF7F50')
ax2.tick_params(axis='y', labelcolor='#FF7F50')

plt.title('Raw data obtained from the sensors', fontsize=20, fontweight='bold')

plt.show()