import serial
import csv
import time

# ------------------------------------------------------------------------ Change COM port and baud rate
ser = serial.Serial('COM5', 9600, timeout=1)

# ----------------------------------------------------------------------- Change output CSV file name
with open('Test01.csv', mode='a', newline='') as file:
    writer = csv.writer(file)

    # Write header if file is empty
    if file.tell() == 0:
        writer.writerow(['T1_TankTemp', 'H1_TankHumidity', 'T2_SensorTemp', 'H2_SensorHumidity', 'PaperSensorValue'])

    print("Started saving data...")

    try:
        while True:
            line = ser.readline().decode('utf-8').strip()

            if line:
                print(f"Raw Data: {line}")  # Debugging help

                # Split using comma AND pipe separators
                # Expected Arduino format:
                # T1 H1 | T2 H2 | Paper
                parts = line.replace("|", ",").replace("  ", " ").replace(" ", "").split(",")

                # Expecting 5 values
                if len(parts) == 5:
                    T1 = parts[0]
                    H1 = parts[1]
                    T2 = parts[2]
                    H2 = parts[3]
                    Paper = parts[4]

                    writer.writerow([T1, H1, T2, H2, Paper])
                    file.flush()

                    print(f"T1={T1}°C, H1={H1}%, T2={T2}°C, H2={H2}%, Paper={Paper}")

                else:
                    print("⚠ Incomplete or mismatched data format. Skipping entry.")

            time.sleep(0.5)

    except KeyboardInterrupt:
        print("Data saving stopped.")

    except Exception as e:
        print(f"Error: {e}")

    finally:
        ser.close()
