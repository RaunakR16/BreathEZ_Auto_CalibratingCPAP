#define SENSOR 36
#define RELAY 4

// ---------- Timing ----------
unsigned long previousSensorMillis = 0;
unsigned long previousRelayMillis = 0;

const unsigned long sensorInterval = 100;   // 100 ms
const unsigned long relayOnTime = 1000;     // 1 sec
const unsigned long relayOffTime = 15000;    // 15 sec
const unsigned long relayHigh = 50;

// ---------- Relay ----------
bool relayState = false;

// ---------- EMA Filter ----------
float filteredValue = 0;

// Smoothing factor
// Lower = smoother but slower response
// Higher = faster response but less smooth
float alpha = 0.1;

void setup()
{
  Serial.begin(115200);

  pinMode(RELAY, OUTPUT);
  digitalWrite(RELAY, LOW);

  // Initialize filter
  filteredValue = analogRead(SENSOR);
}

void loop()
{
  unsigned long currentMillis = millis();

  // ---------- SENSOR READING ----------
  if (currentMillis - previousSensorMillis >= sensorInterval)
  {
    previousSensorMillis = currentMillis;

    int rawValue = analogRead(SENSOR);

    // EMA FILTER
    filteredValue = (alpha * rawValue) + ((1 - alpha) * filteredValue);

    // Serial.print("Raw: ");
    Serial.print(rawValue);

    // Serial.print("  Filtered: ");
    Serial.print(", ");
    Serial.println(filteredValue);
  }

  // ---------- RELAY CONTROL ----------

  // Relay ON state
  if (relayState == true)
  {
    if (currentMillis - previousRelayMillis >= relayOnTime)
    {
      relayState = false;
      previousRelayMillis = currentMillis;

      digitalWrite(RELAY, LOW);

      //Serial.println("Relay OFF");
    }
  }
  // Relay OFF state
  else
  {
    if (currentMillis - previousRelayMillis >= relayOffTime)
    {
      relayState = true;
      previousRelayMillis = currentMillis;

      digitalWrite(RELAY, relayHigh);

      // Serial.println("Relay ON");
    }
  }
}
