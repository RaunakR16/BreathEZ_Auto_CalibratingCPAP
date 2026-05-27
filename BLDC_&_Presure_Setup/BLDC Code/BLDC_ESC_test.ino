#include <ESP32Servo.h>

Servo esc;
int currentSpeed = 1000;
int targetSpeed = 1000;

const int RAMP_STEP = 5;      // µs per step (lower = smoother)
const int RAMP_DELAY = 20;    // ms between steps (lower = faster ramp)

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

  esc.writeMicroseconds(target);  // ensure we land exactly on target
  currentSpeed = target;
}

void setup() {
  esc.attach(18);
  Serial.begin(115200);
  Serial.println("Arming ESC...");
  esc.writeMicroseconds(1000);
  delay(3000);
  Serial.println("Enter Speed 0 to 10:");
}

void loop() {
  if (Serial.available()) {
    int inputSpeed = Serial.parseInt();
    while (Serial.available()) Serial.read();  // flush buffer

    inputSpeed = constrain(inputSpeed, 0, 10);
    targetSpeed = map(inputSpeed, 0, 10, 1035, 1260);

    Serial.print("Ramping to speed: ");
    Serial.print(inputSpeed);
    Serial.print(" | ");
    Serial.println(targetSpeed);


    rampToSpeed(targetSpeed);

    Serial.println("Target reached.");
    Serial.println("Enter Speed 0 to 10:");
  }
}
