// ******** R16 ********

#include <BluetoothSerial.h>
#include <Wire.h>
#include <Adafruit_BMP280.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <ESP32Servo.h>

// ----------------------------------------------------- Pin Config
#define sensorPin    36
#define Humidifyer   23
#define AIR_SDA_PIN  26
#define AIR_SCL_PIN  25
#define DIS_SDA_PIN  21
#define DIS_SCL_PIN  22
#define BLDC_PIN     18

// ----------------------------------------------------- Display Config
#define SCREEN_WIDTH  128
#define SCREEN_HEIGHT  64

// --------------------------------------------------------- Ramp Config
const int RAMP_STEP  = 10;   // µs per step
const int RAMP_DELAY = 20;   // ms between steps

// --------------------------------------------------------- System Mode
enum SystemMode { MODE_NONE, MODE_AUTO, MODE_MANUAL };
SystemMode systemMode = MODE_NONE;

// --------------------------------------------------------- ESC Global State
int currentSpeed  = 1000;
int targetSpeed   = 1074;     // stoping PWM 1074 / 1075 theke start
int manualSpeedLVL = 0;       // last manual input (0–10), default

// -------------------------------------------- I2C Buses
TwoWire I2C_BMP  = TwoWire(0);
TwoWire I2C_OLED = TwoWire(1);

// ----------------------------------------------- Objects setup
BluetoothSerial SerialBT;
Adafruit_BMP280 bmp(&I2C_BMP);
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &I2C_OLED, -1);
Servo esc;

// ─────────────────────────────────────────── Forward Declarations
void rampToSpeed(int target);
void sendModePrompt();
void parseBTInput();
void runAutoMode(int sensorValue);
void runManualMode();
void updateOLED(int sensorValue, float temperature, float pressure, int speedLVL, const char* h_status, const char* modeLabel);
void sendBTTelemetry(int sensorValue, float temperature, float pressure, int speedLVL, const char* h_status);


// ------------------------------------------------------------------------------------------------------ ********SETUP******
void setup()
{
  Serial.begin(115200);

  // ----------------------------------- Bluetooth Name
  SerialBT.begin("BreathEZ");

  // ------------------------------------- Humidifier
  pinMode(Humidifyer, OUTPUT);
  digitalWrite(Humidifyer, LOW);

  // ---------------------------------------------------- Air Pressure Sensor
  I2C_BMP.begin(AIR_SDA_PIN, AIR_SCL_PIN, 100000);
  if (!bmp.begin(0x76)) 
  {
    Serial.println("!!ERROR!! Air Preasure Sensor not found!");
    SerialBT.println("!!ERROR!! Air Preasure Sensor not found!");
    while (1);
  }

  // ------------------------------------------------------------- OLED Display
  I2C_OLED.begin(DIS_SDA_PIN, DIS_SCL_PIN, 100000);
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) 
  {
    Serial.println("!!ERROR!! OLED not found!");
    Serial.println("!!ERROR!! OLED not found!");
    while (1);
  }
  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(WHITE);
  display.setCursor(15, 25);
  display.println("BreathEZ");
  display.display();

  //-------------------------------------------- ESC / BLDC Arm Sequence
  esc.attach(BLDC_PIN);
  Serial.println("Arming ESC...");
  SerialBT.println("Arming ESC...");
  esc.writeMicroseconds(1000);
  delay(3000);

  Serial.println("System Initialized — Waiting for Bluetooth...");
  SerialBT.println("System Initialized");
}


//------------------------------------------------------------------------------------------------------ ******LOOP*****
void loop()
{
  if (systemMode == MODE_NONE)
  {
    if (SerialBT.connected())
    {
      // Prompt once when connection is detected
      static bool promptSent = false;
      if (!promptSent) {
        delay(500);         
        sendModePrompt();
        promptSent = true;
      }
      parseBTInput(); // keep lisening to BT sigbnal

      // ---------------------------------- Mode Selector on OLED
      display.clearDisplay();
      display.setTextSize(2);
      display.setTextColor(WHITE);
      display.setCursor(15, 0);
      display.println("BreathEZ");
      display.setTextSize(1);
      display.setTextColor(WHITE);
      display.setCursor(5, 28);
      display.println("Select Mode:");
      display.setCursor(5, 40);
      display.println(" A: Auto Mode");
      display.setCursor(5, 52);
      display.println(" M: Manual Mode");
      display.display();
    }
    else
    {
      static bool promptSent = false;
      promptSent = false;

      display.clearDisplay();
      display.setTextSize(1);
      display.setTextColor(WHITE);
      display.setCursor(10, 25);
      display.println("Waiting for BT...");
      display.display();
    }
    return;   // Do NOT proceed to sensor/motor logic until mode is set
  }

  // -------------------------------------------------------- BT is connected & mode is set — check for new commands every loop
  parseBTInput();

  // -------------------------------------------------------- Sensor Value Read
  int   sensorValue = analogRead(sensorPin); // Paper
  float temperature = bmp.readTemperature(); //Air
  float pressure    = bmp.readPressure() / 100.0f;  //Air  ~hPa
  // float altitude = bmp.readAltitude(1013.25);  //Air   

  // -------------------------------------------------- Sensor Check
  if (sensorValue <= 0 || sensorValue > 4095) 
  {
    Serial.println("!!ERROR!! Sensor not connected!");
    SerialBT.println("!!ERROR!! Sensor not connected!");
    delay(500);
    return;
  }

  // ─────────────────────────────────────── Humidifier Control
  const char* H_status;
  if (sensorValue < 1000) // ************************************************CALIBRATION**************
  {           
    digitalWrite(Humidifyer, HIGH);
    H_status = "ON";
  } 
  else 
  {
    digitalWrite(Humidifyer, LOW);
    H_status = "OFF";
  }

  // ------------------------------------------------------------- Fan / BLDC Control 
  int SpeedLVL = 0;

  if (systemMode == MODE_AUTO)
  {
    // AUTO MODE 
    int BLDC_Speed = map(sensorValue, 300, 3100, 1074, 2000); // ********************** CALIBRATION *************
    targetSpeed = constrain(BLDC_Speed, 1074, 2000);
    SpeedLVL = map(targetSpeed, 1000, 2000, 0, 10);
    rampToSpeed(targetSpeed);
  }
  else if (systemMode == MODE_MANUAL)
  {
    // Manual: last user input drives speed (already stored in targetSpeed)
    SpeedLVL = manualSpeedLVL;
    // rampToSpeed is called inside parseBTInput when a new value arrives
    // but we re-apply here to hold the last commanded speed
    //esc.writeMicroseconds(targetSpeed); // ????????????????????????????????????????????????????????????
    rampToSpeed(targetSpeed);
  }

  // ----------------------------------------------------------------------- Mode label for display
  const char* modeLabel = (systemMode == MODE_AUTO) ? "AUTO" : "MANUAL";

  // ------------------------------------------------------------------- Serial Print
  Serial.print("MODE: ");  
  Serial.print(modeLabel);  
  Serial.print(", Paper Sensor: ");    
  Serial.print(sensorValue);
  Serial.print(", Temp: ");   
  Serial.print(temperature);  
  Serial.print(" C");
  Serial.print(", Pressure: ");   
  Serial.print(pressure);     
  Serial.print(" hPa");
  Serial.print(", Speed LVL: "); 
  Serial.print(SpeedLVL);
  Serial.print(", PWM: ");    
  Serial.print(targetSpeed);
  Serial.print(", Humid status: ");  
  Serial.println(H_status);

  // ----------------------------------------------------------------------- Bluetooth Telemetry
  sendBTTelemetry(sensorValue, temperature, pressure, SpeedLVL, H_status);

  // ------------------------------------------------------------------------------- OLED Update
  updateOLED(sensorValue, temperature, pressure, SpeedLVL, H_status, modeLabel);

  delay(100);
}

// ------------------------------------------------------------------------- *****BLUETOOTH INPUT PARSER******

void parseBTInput()
{
  if (!SerialBT.available()) return;

  String input = ""; // Read full incoming string until newline or buffer empty
  while (SerialBT.available()) 
  {
    char c = (char)SerialBT.read();
    if (c == '\n' || c == '\r') break;
    input += c;
    delay(2);
  }
  input.trim();

  if (input.length() == 0) return;

  String upper = input;
  upper.toUpperCase();

  // ─---------------------------------------------------------------─ Mode Switch Commands
  if (upper == "A" || upper == "AUTO")
  {
    systemMode = MODE_AUTO;
    SerialBT.println("------------------------------");
    SerialBT.println("  Mode: AUTO  ");
    SerialBT.println("------------------------------");
    Serial.println("----------Mode Switched to AUTO-------------");
    targetSpeed = 1074;
    rampToSpeed(targetSpeed);
    return;
  }

  if (upper == "M" || upper == "MANUAL")
  {
    systemMode = MODE_MANUAL;
    SerialBT.println("------------------------------");
    SerialBT.println("  Mode: MANUAL  ");
    SerialBT.println("  Send speed level (0-10).");
    SerialBT.println("------------------------------");
    Serial.println("------------Mode Switched to MANUAL-----------");
    targetSpeed = 1074;
    rampToSpeed(targetSpeed);
    return;
  }

  if (upper == "R" || upper == "RESET")
  {
    systemMode = MODE_NONE;
    Serial.println("MODE: RESET");
    SerialBT.println("Reseting...");
    targetSpeed = 1074;
    rampToSpeed(targetSpeed);
    return;
  }

  if (systemMode == MODE_MANUAL)  // --------------- Manual Speed Input, only valid in MANUAL mode
  {
    bool isNumber = true;
    for (int i = 0; i < (int)input.length(); i++)
    {
      if (!isDigit(input[i])) 
      { 
        isNumber = false; 
        break; 
      }
    }

    if (isNumber)
    {
      int inputSpeed = input.toInt();
      inputSpeed     = constrain(inputSpeed, 0, 10);
      targetSpeed    = map(inputSpeed, 0, 10, 1074, 1800);
      manualSpeedLVL = inputSpeed;

      rampToSpeed(targetSpeed);

      SerialBT.print("MODE: MANUAL, Speed set to level ");
      SerialBT.print(inputSpeed);
      SerialBT.print("/10, PWM: ");
      SerialBT.print(targetSpeed);
      SerialBT.println(" µs");

      Serial.print("MODE: MANUAL, Speed LVL: "); 
      Serial.print(inputSpeed);
      Serial.print(", PWM: ");             
      Serial.print(targetSpeed);
      Serial.println(" µs");
    }
    else
    {
      SerialBT.println("!!Unknown command!! Send 0-10 for speed, or 'A' for Auto.");
    }
  }
  else if (systemMode == MODE_AUTO)
  {
    SerialBT.println("In AUTO mode. Send 'M' to switch to Manual.");
  }
  else
  {
    SerialBT.println("Please select mode: Send 'A' (Auto) or 'M' (Manual).");
  }
}

//  -------------------------------------------------------------------*********MODE PROMPT***********
void sendModePrompt()
{
  SerialBT.println("==============================");
  SerialBT.println("   Welcome to BreathEZ v1.0   ");
  SerialBT.println("==============================");
  SerialBT.println("Please select operating mode:");
  SerialBT.println("  A:  Auto Mode");
  SerialBT.println("  M:  Manual Mode");
  SerialBT.println("        *Control Fan speed 0 to 10*");
  SerialBT.println("  R:  Reset");
  SerialBT.println("==============================");
  SerialBT.println("Send 'A' or 'M' to begin.");
  SerialBT.println("==============================");
}



// ----------------------------------------------------------------- *************BLUETOOTH TELEMETRY************
void sendBTTelemetry(int sensorValue, float temperature, float pressure, int speedLVL, const char* h_status)
{
  const char* modeStr = (systemMode == MODE_AUTO) ? "AUTO" : "MANUAL";

  SerialBT.print("Mode: ");         
  SerialBT.print(modeStr);
  SerialBT.print(", Paper Sensor: ");    
  SerialBT.print(sensorValue);
  SerialBT.print(", Temp: ");      
  SerialBT.print(temperature);  
  SerialBT.print("C");
  SerialBT.print(", Pressure: ");  
  SerialBT.print(pressure);     
  SerialBT.print("hPa");
  SerialBT.print(", Speed LVL: ");  
  SerialBT.print(speedLVL);     
  SerialBT.print("/10");
  SerialBT.print(", PWM: ");       
  SerialBT.print(targetSpeed);
  SerialBT.print(", Humid Status: ");
  SerialBT.println(h_status);
}


// ------------------------------------------------------------------------ *********OLED DISPLAY**********
void updateOLED(int sensorValue, float temperature, float pressure, int speedLVL, const char* h_status, const char* modeLabel)
{
  display.clearDisplay();
  display.setTextColor(WHITE);

  // ── Header row
  display.setTextSize(1);
  display.setCursor(0, 2);
  display.print("BreathEZ");
  display.setCursor(80, 2);
  display.print(modeLabel);

  // ── Divider line (manual pixel draw)
  display.drawLine(0, 12, 127, 10, WHITE);

  // ── Data rows
  display.setCursor(0, 18);
  display.print("Pump Speed: ");
  display.print(speedLVL);
  display.print("/10");


  display.setCursor(0, 30);
  display.print("Temp:  ");
  display.print(temperature, 1);
  display.print(" C");

  display.setCursor(0, 42);
  display.print("Press: ");
  display.print(pressure, 1);
  display.print(" hPa");

  display.setCursor(0, 54);
  display.print("Humidifier Status: ");
  display.print(h_status);

  display.display();
}


// --------------------------------------------------------------------- *******ESC RAMP FUNCTION*******
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

  esc.writeMicroseconds(target);  // land exactly on target
  currentSpeed = target;
  Serial.print("!!ESC!! Target reached: "); 
  Serial.println(target);
}
