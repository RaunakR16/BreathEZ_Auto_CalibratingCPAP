#include <dht11.h>
#include <SoftwareSerial.h> 

// Pin Numbers
#define DHT11PIN A0   
#define RELAY1PIN 7   

#define PaperSensorPin A3 

dht11 DHT11;


bool fan1On = false; // Keeps track of Fan 1 state.  Start with fan off.
int PaperSensorValue = 0;

void setup()
{

  Serial.begin(9600);         
  pinMode(RELAY1PIN, OUTPUT);
  digitalWrite(RELAY1PIN, HIGH); // Initialize fan to OFF (HIGH for active low)
}

void loop()
{
  float h = (float)DHT11.humidity;
  float t = (float)DHT11.temperature;
  PaperSensorValue = analogRead(PaperSensorPin);

  // Fan control logic
  if (h >= 90 && !fan1On) 
  {
    digitalWrite(RELAY1PIN, LOW);  // Turn on Fan 1 (LOW for active low relay)
    fan1On = true;
  } 
  else if (h < 50 && fan1On) 
  {
    digitalWrite(RELAY1PIN, HIGH); // Turn off Fan 1 (HIGH for active low relay)
    fan1On = false;
  }

  Serial.print(temperature, 2);
  Serial.print(",");
  Serial.print(humidity, 2);
  Serial.print(",");
  Serial.println(PaperSensorValue);

  delay(1000);
}
