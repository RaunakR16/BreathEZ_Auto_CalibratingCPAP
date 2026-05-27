#include <ESP32Servo.h>

Servo esc;
int currentSpeed = 1000;

void setup() {
  esc.attach(18);  // GPIO18

  Serial.begin(115200);
  Serial.println("Arming ESC...");

  // Send minimum throttle
  esc.writeMicroseconds(1000);
  delay(3000);  // wait for ESC to arm
  Serial.print("Enter Speed 0 to 1000");
}

void loop()
{
  if (Serial.available())
  {
    int inputSpeed = Serial.parseInt();

    // Flush leftover characters (newline, carriage return, etc.)
    while (Serial.available()) Serial.read();

    inputSpeed = constrain(inputSpeed, 0, 1000);
    int speedValue = map(inputSpeed, 0, 1000, 1000, 2000);
    esc.writeMicroseconds(speedValue);

    currentSpeed = speedValue;
    Serial.print("Speed set to: ");
    Serial.println(inputSpeed);
  }
  // No else needed — ESC holds last pulse automatically
}
