import serial
import csv
import time

# -----------------------------------------------------------------------Change COM port and baud rate
ser = serial.Serial('COM5', 9600, timeout=1) 

# -----------------------------------------------------------------------Change CSV file Name
with open('sensor_data.csv', mode='a', newline='') as file:
    writer = csv.writer(file)

    if file.tell() == 0:
        writer.writerow(['Temperature (°C)', 'Humidity (%)', 'Sensor Value'])
    print("Started saving data...")

    try:
        while True:
            line = ser.readline().decode('utf-8').strip()
            print(f"Temperature (°C)', 'Humidity (%)', 'Sensor Value: {line}")  # Debugging raw data

            if line:
                data = line.split(',')

                if len(data) == 3:  
                    temperature = data[0]
                    humidity = data[1]
                    PaperSensorValue = data[2]

                    writer.writerow([temperature, humidity, PaperSensorValue])
                    file.flush() 
                    print(f"{temperature}C, {humidity}%, {PaperSensorValue}")
                else:
                    print("Incomplete data received, skipping entry.")


                time.sleep(1)
    except KeyboardInterrupt:
        print("Data saving stopped.")
    except Exception as e:
        print(f"Error: {e}")
    finally:

        ser.close()
