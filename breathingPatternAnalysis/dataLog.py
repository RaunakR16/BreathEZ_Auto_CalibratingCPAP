import serial
import csv
from datetime import datetime

# -------- SERIAL CONFIG --------
PORT = 'COM7'
BAUD = 115200

# -------- OPEN SERIAL --------
ser = serial.Serial(PORT, BAUD, timeout=1)

# -------- CSV FILE --------
filename = r"C:\Users\rauna\Downloads\4th_YearProject-1\CODES\breathingPatternAnalysis\DATA\sensor_log9.csv"

with open(filename, mode='w', newline='') as file:

    writer = csv.writer(file)

    # CSV Header
    writer.writerow(["Timestamp", "Raw_Value", "Filtered_Value"])

    print("Logging started... Press Ctrl+C to stop.")

    try:
        while True:

            line = ser.readline().decode('utf-8').strip()

            if line:
                try:
                    raw, filtered = line.split(',')

                    timestamp = datetime.now().strftime('%H:%M:%S.%f')

                    writer.writerow([timestamp, raw, filtered])

                    # Force save instantly
                    file.flush()

                    print(f"{timestamp} | Raw: {raw} | Filtered: {filtered}")

                except Exception as e:
                    print("Invalid data:", line)
                    print(e)

    except KeyboardInterrupt:
        print("\nLogging stopped.")

ser.close()