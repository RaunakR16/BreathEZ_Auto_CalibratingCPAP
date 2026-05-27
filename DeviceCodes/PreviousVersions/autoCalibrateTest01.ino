#include <BluetoothSerial.h>
#include <Wire.h>
#include <Adafruit_BMP280.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <ESP32Servo.h>

// --------------------------------------------- Pin Config
#define sensorPin 36
#define Humidifyer 23
#define AIR_SDA_PIN 26
#define AIR_SCL_PIN 25
#define DIS_SDA_PIN 21
#define DIS_SCL_PIN 22
#define BLDC_PIN 18

//------------------------------------------------- Veriables
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

int currentSpeed = 1000;
int targetSpeed = 1000;

const int RAMP_STEP = 10;      // µs per step (lower = smoother)
const int RAMP_DELAY = 20;    // ms between steps (lower = faster ramp)

// --------------------------------------------- I2C Buses
TwoWire I2C_BMP = TwoWire(0);
TwoWire I2C_OLED = TwoWire(1);

// --------------------------------------------- System Objects
BluetoothSerial SerialBT;
Adafruit_BMP280 bmp(&I2C_BMP);
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &I2C_OLED, -1);
Servo esc;

// ------------------------------------------------------------------------------------------------------ SETUP
void setup() 
{
  Serial.begin(115200);

  // ------------------------------------ Bluetooth Name
  SerialBT.begin("BreathEZ"); 

  // ------------------------------------ Humidifier
  pinMode(Humidifyer, OUTPUT);  
  digitalWrite(Humidifyer, LOW);

  // --------------------------------------------- Air Preasure BMP280
  I2C_BMP.begin(AIR_SDA_PIN, AIR_SCL_PIN, 100000);

  if (!bmp.begin(0x76))  
  {  
    Serial.println("BMP280 not found!");
    while (1);
  }

  // -------------------------------------------------------- OLED Display
  I2C_OLED.begin(DIS_SDA_PIN, DIS_SCL_PIN, 100000);

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) 
  {
    Serial.println("OLED not found");
    while (1);
  }

  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(WHITE);

  display.setCursor(15, 25);
  display.println("BreathEZ");
  display.display();

  Serial.println("System Initialized");
  Serial.println("Bluetooth Ready");

  // -------------------------------------------------------- ESC
  esc.attach(BLDC_PIN);
  Serial.println("Arming ESC...");
  esc.writeMicroseconds(1000);
  delay(3000);
}

//------------------------------------------------------------------------------------------------------ LOOP
void loop() 
{
  display.clearDisplay();

  // -------------------------------------------------------- Air Sensor Value Read
  int sensorValue = analogRead(sensorPin);
  float temperature = bmp.readTemperature();
  float pressure = bmp.readPressure() / 100.0; // hPa
  float altitude = bmp.readAltitude(1013.25);

  // ------------------------------------------------------ Sensor check
  if (isnan(sensorPin)) 
  {
    Serial.println("!!ERROR!!, Sensor is not connected");
    SerialBT.println("!!ERROR!!, Sensor is not connected");
    return;
  }
  // ------------------------------------------------------- Humidifyer Controll
  const char* H_status;

  if(sensorValue < 1000) // ***************CALIBRATION*****************
  {
    digitalWrite(Humidifyer, HIGH);
    // Serial.println("Humidifyer ON");
    // SerialBT.println("Humidifyer ON");
    H_status = "ON";
  }
  else 
  {
    digitalWrite(Humidifyer, LOW);
    // Serial.println("Humidifyer OFF");
    // SerialBT.println("Humidifyer OFF");
    H_status = "OFF";
  }

  // ------------------------------------------------------------ BLDC ESC ********************

  int BLDC_Speed = map(sensorValue, 200, 3100, 1074, 2000);
  int targetSpeed = constrain(BLDC_Speed, 1074, 2000);
  int SpeedLVL = map(targetSpeed, 1000, 1900, 0, 10);
  rampToSpeed(targetSpeed);
   
  // --------------------------------------------------------- Serial Print
  Serial.print("PaperSensor: ");
  Serial.print(sensorValue);
  Serial.print(", ");
  
  Serial.print("Temp: ");
  Serial.print(temperature);
  Serial.print(" °C");
  Serial.print(", ");

  Serial.print("Pressure: ");
  Serial.print(pressure);
  Serial.print(" hPa");
  Serial.print(", ");

  // Serial.print("Altitude: ");
  // Serial.print(altitude);
  // Serial.print(" m");
  // Serial.print(", ");

  Serial.print("Pump speed lvl: ");
  Serial.print(SpeedLVL);
  Serial.print(", ");
 
  Serial.print("Pump speed: ");
  Serial.println(targetSpeed);

  //----------------------------------------------------------- Bluetooth Print
  SerialBT.print("PaperSensor: ");
  SerialBT.print(sensorValue);
  SerialBT.print(", ");
  
  SerialBT.print("Temp: ");
  SerialBT.print(temperature);
  SerialBT.print(" °C");
  SerialBT.print(", ");

  SerialBT.print("Pressure: ");
  SerialBT.print(pressure);
  SerialBT.print(" hPa");
  SerialBT.print(", ");

  // SerialBT.print("Altitude: ");
  // SerialBT.print(altitude);
  // SerialBT.print(" m");
  // SerialBT.print(", ");

  SerialBT.print("Pump speed lvl: ");
  SerialBT.print(SpeedLVL);
  SerialBT.print(", ");

  SerialBT.print("Pump speed: ");
  SerialBT.print(targetSpeed);
  SerialBT.print(", ");

  SerialBT.print("Humidifyer Status: ");
  SerialBT.println(H_status);

  // --------------------------------------------------------- OLED Display
  display.setTextSize(1.5);
  display.setCursor(25, 0);
  display.println("CPAP Monitor");

  // display.setTextSize(1);
  // display.setCursor(0, 18);
  // display.print("Resp: ");
  // display.print(sensorValue);
  // display.println("breaths/min");

  // display.setCursor(0, 30);
  // display.print("Temp: ");
  // display.print(temperature);
  // display.println(" C");

  // display.setCursor(0, 42);
  // display.print("Pressure:");
  // display.print(pressure);
  // display.println(" hPa");

  // display.setCursor(0, 54);
  // display.print("Humidifyer Status:");
  // display.println(status);

  //************************************************************************************

  display.setTextSize(1);

  display.setCursor(0, 18);
  display.print("Pump Speed: ");
  display.print(SpeedLVL);

  display.setCursor(0, 30);
  display.print("Temp: ");
  display.print(temperature);
  display.println(" C");

  display.setCursor(0, 42);
  display.print("Pressure:");
  display.print(pressure);
  display.println(" hPa");

  display.setCursor(0, 54);
  display.print("Humidifyer Status:");
  display.println(H_status);

  display.display();
  delay(100); 
}

// ------------------------------------------------------------------------------- BLDC Pump Ramp Function
void rampToSpeed(int target) 
{
  target = constrain(target, 1000, 2000);

  if (currentSpeed < target) {
    for (int s = currentSpeed; s <= target; s += RAMP_STEP) {
      esc.writeMicroseconds(s);
      delay(RAMP_DELAY);
    }
  } 
  else if (currentSpeed > target) {
    for (int s = currentSpeed; s >= target; s -= RAMP_STEP) {
      esc.writeMicroseconds(s);
      delay(RAMP_DELAY);
    }
  }

  esc.writeMicroseconds(target);  // ensure we land exactly on target
  currentSpeed = target;
  Serial.println("Target reached.");
}
