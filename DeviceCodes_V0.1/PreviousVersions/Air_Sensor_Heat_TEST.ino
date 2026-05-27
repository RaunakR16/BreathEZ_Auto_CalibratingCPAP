
# define SENSOR 36

#define RELAY 4

#define DT 25
#define SCK 26

long readHX710B() 
{
  long value = 0;

  if (digitalRead(DT) == HIGH) 
  {
  Serial.println("HX710B not ready");
  return 0;
  } // wait for data ready

  for (int i = 0; i < 24; i++) {
    digitalWrite(SCK, HIGH);
    value = value << 1;
    digitalWrite(SCK, LOW);

    if (digitalRead(DT)) {
      value++;
    }
  }

  // extra clock pulse
  digitalWrite(SCK, HIGH);
  digitalWrite(SCK, LOW);

  // sign extend
  if (value & 0x800000) {
    value |= 0xFF000000;
  }

  return value;
}

void setup() 
{
  Serial.begin(115200);
  pinMode(RELAY, OUTPUT);
  pinMode(SCK, OUTPUT);
  pinMode(DT, INPUT);
}

void loop() 
{

  int sensorValue = analogRead(SENSOR);
  Serial.print(sensorValue);
  Serial.print(", ");

  // digitalWrite(RELAY, HIGH);   
  long reading = readHX710B();
  Serial.println(reading);
  delay(500);
      
  // digitalWrite(RELAY, LOW);
  // delay(1000);  
}

