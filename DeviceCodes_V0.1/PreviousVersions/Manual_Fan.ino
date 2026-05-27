#include <ESP32Servo.h>
#include <BluetoothSerial.h>

BluetoothSerial BT;

Servo esc;
int currentSpeed = 1000;
int targetSpeed = 1000;
const int RAMP_STEP = 5;
const int RAMP_DELAY = 20;

void rampToSpeed(int target) {
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
  esc.writeMicroseconds(target);
  currentSpeed = target;
}

void setup() {
  Serial.begin(115200);         // USB debug
  BT.begin("ESP32_Motor");      // Bluetooth device name — change as you like

  esc.attach(18);
  Serial.println("Arming ESC...");
  BT.println("Arming ESC...");

  esc.writeMicroseconds(1000);
  delay(3000);

  Serial.println("Bluetooth ready. Enter Speed 0 to 10:");
  BT.println("Connected! Enter Speed 0 to 10:");
}

void loop() {
  if (BT.available()) 
  {
    int inputSpeed = BT.parseInt();
    while (BT.available()) BT.read();  // flush buffer

    inputSpeed = constrain(inputSpeed, 0, 10);
    targetSpeed = map(inputSpeed, 0, 10, 1074, 1800);

    Serial.print("Ramping to: ");  
    Serial.print(inputSpeed);
    Serial.print(" | ");           
    Serial.println(targetSpeed);
    
    BT.print("Ramping to: "); 
    BT.print(inputSpeed);
    BT.print(" | ");                
    BT.println(targetSpeed);

    rampToSpeed(targetSpeed);

    Serial.println("Target reached.");
    BT.println("Target reached. Enter Speed 0 to 10:");
  }
}
