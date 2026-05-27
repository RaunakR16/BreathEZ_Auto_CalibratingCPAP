#include "DHT.h"

//Pin Numbers
#define DHTPIN1 A0 
#define DHTPIN2 A1
#define RELAY 7  
#define PaperSensorPin A2   

#define DHTTYPE DHT11  

float PaperSensorValue = 0;
bool fan1On = false;

DHT dht1(DHTPIN1, DHTTYPE);
DHT dht2(DHTPIN2, DHTTYPE);


void setup() 
{
  Serial.begin(9600);
  dht1.begin();
  dht2.begin();
  pinMode(RELAY, OUTPUT);
}

void loop() 
{
  PaperSensorValue = analogRead(PaperSensorPin);
  float h1 = dht1.readHumidity();
  float t1 = dht1.readTemperature();
  
  float h2 = dht2.readHumidity();
  float t2 = dht2.readTemperature();

  if (isnan(h1) || isnan(t1) || isnan(h2) || isnan(t2)) 
  {
    Serial.println("Failed to read from DHT sensor!");
    return;
  }

  // Fan control logic
  if (h1 >= 90 && !fan1On) 
  {
    digitalWrite(RELAY, LOW);  // Turn on Fan 1 (LOW for active low relay)
    fan1On = true;
  } 
  else if (h1 < 50 && fan1On) 
  {
    digitalWrite(RELAY, HIGH); // Turn off Fan 1 (HIGH for active low relay)
    fan1On = false;
  }

Serial.print(t1); 
Serial.print(",");
Serial.print(h1); 
Serial.print(",");
Serial.print(t2); 
Serial.print(",");
Serial.print(h2); Serial.print(",");
Serial.println(PaperSensorValue);


  delay(500);
}
