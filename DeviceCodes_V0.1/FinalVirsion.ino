#include <BluetoothSerial.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <ESP32Servo.h>

// ************************************************************** PIN MAP
#define SENSOR_PIN      36      
#define HUMIDIFIER_PIN  23     
#define RELAY2_PIN       4  
#define DIS_SDA_PIN     21     
#define DIS_SCL_PIN     22    
#define BLDC_PIN        18     
#define HX710B_DOUT     25     
#define HX710B_SCK      26   

// ************************************************************* OLED
#define SCREEN_WIDTH  128
#define SCREEN_HEIGHT  64

// ************************************************************* ESC RAMP
static const int RAMP_STEP  = 10;   // µs per step
static const int RAMP_DELAY = 20;   // ms between steps

// ************************************************************ BREATHING CONFIG
#define MAX_PEAKS               200       // buffer capacity

#define BASELINE_WINDOW_MS   60000UL      // 1-minute baseline collection (individual person brething behaviour detection)
#define THERAPY_CYCLE_MS     30000UL      // 30-second re-evaluation window
#define LIVE_BPM_WINDOW_MS   60000UL      // rolling window for live BPM display

static const int   BREATH_THRESHOLD  = 50;
static const int   BREATH_HYSTERESIS = 20;
static const unsigned long BREATH_MIN_GAP_MS = 1500;

static const float BPM_HYSTERESIS   = 3.0f;
static const float RECOVERY_MARGIN  = 1.5f;
static const float BPM_TOLERANCE    = 1.0f;

// ******************************************************************* HUMIDIFIER CONFIG
static const unsigned long HUM_ON_TIME        = 1000UL;   // 1sec ON
static const unsigned long HUM_OFF_TIME       = 3000UL;  // 3sec OFF
static const int           HUM_ADC_THRESHOLD  = 1000;

// ************************************************************** SYSTEM STATE
enum SystemMode { MODE_NONE, MODE_AUTO, MODE_MANUAL };
enum AutoState  { AUTO_COLLECT, AUTO_WATCH, AUTO_THERAPY };

SystemMode systemMode = MODE_NONE;
AutoState  autoState  = AUTO_COLLECT;

// ********************************************************** ESC
int currentSpeed    = 1000;
int targetSpeed     = 1074; // Base PWM corresponding to 0% fan speed 
int manualSpeedLVL  = 0;
int therapySpeedLVL = 1;

// **************************************************** SENSOR / EMA
float filteredValue  = 2000.0f;
float localMean      = 2000.0f;
int   sensorBaseline = 0;

static const float EMA_ALPHA  = 0.2f;
static const float MEAN_ALPHA = 0.003f;

// ********************************************************* PEAK DETECTOR STATE
bool          inPeak     = false;
unsigned long lastPeakMs = 0;

// ********************************************************** PEAK RING BUFFER
unsigned long peakTimestamps[MAX_PEAKS];
int           peakHead  = 0;
int           peakCount = 0;

// *********************************************************** LIVE BREATH STATS
float currentBPM    = 0.0f;
float baselineBPM   = 0.0f;
float avgInterval_s = 0.0f;
float minInterval_s = 0.0f;
float maxInterval_s = 0.0f;

// ******************************************************* AUTO MODE TIMING
unsigned long autoPhaseStartMs   = 0;
unsigned long lastTherapyCycleMs = 0;
float         prevCycleBPM       = 0.0f;

// ****************************************************** HUMIDIFIER STATE
bool          humEnabled     = false;
bool          humRelayActive = false;
unsigned long humLastToggle  = 0;

// ***************************************************** RELAY 2 STATE
bool          relay2Active     = false;
unsigned long relay2LastToggle = 0;

// ***************************************************** BPM DISPLAY REFRESH
unsigned long lastBpmRefreshMs = 0;
static const unsigned long BPM_REFRESH_MS = 2000UL;  // recalculate every 2 s

// ***************************************************** HX710B PRESSURE
long  hx710b_offset    = 0;
float hx710b_scale_hpa = 7065.0f;

// ***************************************************** OBJECTS
TwoWire          I2C_OLED = TwoWire(1);
BluetoothSerial  SerialBT;
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &I2C_OLED, -1);
Servo            esc;

// **************************************************** FORWARD DECLARATIONS
void  rampToSpeed(int target);
void  sendModePrompt();
void  parseBTInput();
int   readSensor();
void  calibrateSensor();
void  tareHX710B();
long  readHX710B_raw();
float readPressure_hPa();
bool  detectBreath(int filtered, unsigned long nowMs);
void  computeStats(unsigned long windowStartMs,float* bpm, float* avg, float* mn, float* mx);
void  runAutoLogic(unsigned long nowMs);
void  handleHumidifier(int sensorValue, unsigned long nowMs);
void  handleRelay2(int sensorValue, unsigned long nowMs);   // ← NEW
void  updateOLED(float bpm, float cmH2O, int speedLVL, bool humOn, const char* modeLabel, unsigned long nowMs);
void  sendBTTelemetry(float bpm, float cmH2O, int speedLVL, bool humOn);
void  printStatsSerial(const char* label, float bpm, float avg, float mn, float mx);
void  resetAutoState();

// ********************************************************************* SETUP
void setup()
{
    Serial.begin(115200);
    SerialBT.begin("BreathEZ");

    pinMode(HUMIDIFIER_PIN, OUTPUT);
    digitalWrite(HUMIDIFIER_PIN, LOW);

    pinMode(RELAY2_PIN, OUTPUT);
    digitalWrite(RELAY2_PIN, LOW);

    pinMode(HX710B_DOUT, INPUT);
    pinMode(HX710B_SCK,  OUTPUT);
    digitalWrite(HX710B_SCK, LOW);

    I2C_OLED.begin(DIS_SDA_PIN, DIS_SCL_PIN, 100000);
    if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) 
    {
        Serial.println("!!ERROR!! OLED not found — check wiring!");
        while (1) 
        { 
            delay(500); 
        }
    }
    display.clearDisplay();
    display.setTextSize(2);
    display.setTextColor(WHITE);
    display.setCursor(15, 25);
    display.println("BreathEZ");
    display.display();

    delay(1500);


    esc.attach(BLDC_PIN);
    Serial.println("Arming ESC...");
    esc.writeMicroseconds(1000);
    delay(3000);

    calibrateSensor();
    tareHX710B();

    Serial.println("System ready — waiting for Bluetooth connection.");
}

// ********************************************************************** LOOP
void loop()
{
    unsigned long now = millis();

    // ------------------------------------ No mode selected yet
    if (systemMode == MODE_NONE)
    {
        static bool promptSent = false;

        if (SerialBT.connected())
        {
            if (!promptSent) {
                delay(500);
                sendModePrompt();
                promptSent = true;
            }
            parseBTInput();

            display.clearDisplay();
            display.setTextSize(2); 
            display.setTextColor(WHITE);
            display.setCursor(12, 0);  
            display.println("BreathEZ");
            display.setTextSize(1);
            display.setCursor(5, 26); 
            display.println("Select Mode:");
            display.setCursor(5, 38); 
            display.println("  A  :  Auto Mode");
            display.setCursor(5, 50); 
            display.println("  M  :  Manual Mode");
            display.display();
        }
        else
        {
            promptSent = false;
            display.clearDisplay();
            display.setTextSize(1); display.setTextColor(WHITE);
            display.setCursor(8, 28);
            display.println("Waiting for BT...");
            display.display();
        }
        return;
    }

    // --------------------------------------------- Mode is active
    parseBTInput();

    int sensorValue = readSensor();
    localMean = (MEAN_ALPHA * sensorValue) + ((1.0f - MEAN_ALPHA) * localMean);
    detectBreath(sensorValue, now);

    if (now - lastBpmRefreshMs >= BPM_REFRESH_MS)
    {
        lastBpmRefreshMs = now;
        unsigned long winStart = (now > LIVE_BPM_WINDOW_MS) ? (now - LIVE_BPM_WINDOW_MS) : 0;
        computeStats(winStart, &currentBPM, &avgInterval_s, &minInterval_s, &maxInterval_s);
    }

    //------------------------------------------------ Preasure calculation
    float pressure_hPa   = readPressure_hPa();
    float pressure_cmH2O = pressure_hPa * 1.01972f;

  
    handleHumidifier(sensorValue, now);
    bool humOn = humRelayActive;

   
    handleRelay2(sensorValue, now);

    int speedLVL = 0;
    const char* modeLabel;

    if (systemMode == MODE_AUTO)
    {
        runAutoLogic(now);
        speedLVL  = (autoState == AUTO_THERAPY) ? therapySpeedLVL : 0;
        modeLabel = "AUTO";
    }
    else
    {
        rampToSpeed(targetSpeed);
        speedLVL  = manualSpeedLVL;
        modeLabel = "MAN";
    }

    updateOLED(currentBPM, pressure_cmH2O, speedLVL, humOn, modeLabel, now);
    sendBTTelemetry(currentBPM, pressure_cmH2O, speedLVL, humOn);

    Serial.printf("[%s] BPM:%.1f | Press:%.2f cmH2O | Fan:%d/10 PWM:%d | Humid:%s | Relay2:%s\n",
        modeLabel, currentBPM, pressure_cmH2O, speedLVL, targetSpeed,
        humOn ? "ON" : "OFF",
        relay2Active ? "ON" : "OFF");

    delay(100);
}

// /********************************************************************* */ SECONDARY RELAY CONTROL  
/*
 * Mirrors the humidifier enable condition exactly.
 * When sensorValue < HUM_ADC_THRESHOLD: pulses 1 s ON / 30 s OFF.
 * When sensorValue ≥ threshold: relay stays OFF immediately.
 */
void handleRelay2(int sensorValue, unsigned long nowMs)
{
    bool enabled = (sensorValue < HUM_ADC_THRESHOLD);

    if (!enabled)
    {
        if (relay2Active) 
        {
            relay2Active = false;
            digitalWrite(RELAY2_PIN, LOW);
        }
        return;
    }

    if (relay2Active)
    {
        if (nowMs - relay2LastToggle >= HUM_ON_TIME) 
        {
            relay2Active     = false;
            relay2LastToggle = nowMs;
            digitalWrite(RELAY2_PIN, LOW);
        }
    }
    else
    {
        if (nowMs - relay2LastToggle >= HUM_OFF_TIME) 
        {
            relay2Active     = true;
            relay2LastToggle = nowMs;
            digitalWrite(RELAY2_PIN, HIGH);
        }
    }
}

// ***************************************************************************** BREATH PEAK DETECTOR
bool detectBreath(int filtered, unsigned long nowMs)
{
    float enter = localMean + BREATH_THRESHOLD;
    float exit  = localMean + BREATH_THRESHOLD - BREATH_HYSTERESIS;

    if (!inPeak && filtered > enter)
    {
        inPeak = true;
    }
    else if (inPeak && filtered < exit)
    {
        inPeak = false;

        if ((nowMs - lastPeakMs) >= BREATH_MIN_GAP_MS)
        {
            peakTimestamps[peakHead] = nowMs;
            peakHead   = (peakHead + 1) % MAX_PEAKS;
            if (peakCount < MAX_PEAKS) peakCount++;
            lastPeakMs = nowMs;

            unsigned long winStart = (nowMs > LIVE_BPM_WINDOW_MS) ? (nowMs - LIVE_BPM_WINDOW_MS) : 0;
            computeStats(winStart, &currentBPM, &avgInterval_s, &minInterval_s, &maxInterval_s);

            Serial.printf("[BREATH] Peak detected @ %lums  Live BPM: %.1f\n", nowMs, currentBPM);
            return true;
        }
    }
    return false;
}

// *********************************************************************************************** COMPUTE STATS
void computeStats(unsigned long windowStartMs, float* bpm, float* avg, float* mn, float* mx)
{
    static unsigned long tmp[MAX_PEAKS];
    int cnt = 0;
    int total = min(peakCount, MAX_PEAKS);

    for (int i = 0; i < total; i++)
    {
        int idx = ((peakHead - 1 - i) + MAX_PEAKS) % MAX_PEAKS;
        if (peakTimestamps[idx] < windowStartMs) break;
        tmp[cnt++] = peakTimestamps[idx];
    }

    if (cnt < 2) 
    { 
        *bpm = 0; *avg = 0; *mn = 0; *mx = 0; 
        return; 
    }

    for (int i = 0; i < cnt - 1; i++)
        for (int j = 0; j < cnt - i - 1; j++)
            if (tmp[j] > tmp[j+1]) 
            {
                unsigned long t = tmp[j]; tmp[j] = tmp[j+1]; tmp[j+1] = t;
            }

    float sumI = 0.0f, minI = 1e9f, maxI = 0.0f;
    for (int i = 1; i < cnt; i++) 
    {
        float iv = (tmp[i] - tmp[i-1]) / 1000.0f;
        sumI += iv;
        if (iv < minI) minI = iv;
        if (iv > maxI) maxI = iv;
    }

    float spanSec = (tmp[cnt-1] - tmp[0]) / 1000.0f;
    *bpm = (spanSec > 0.0f) ? ((float)(cnt - 1) / spanSec * 60.0f) : 0.0f;
    *avg = sumI / (float)(cnt - 1);
    *mn  = minI;
    *mx  = maxI;
}

// *************************************************************** AUTO MODE LOGIC
void runAutoLogic(unsigned long nowMs)
{
    switch (autoState)
    {
    case AUTO_COLLECT:
    {
        static unsigned long lastCountPrint = 0;
        if (nowMs - lastCountPrint >= 5000UL) {
            lastCountPrint = nowMs;
            long remaining = (long)(autoPhaseStartMs + BASELINE_WINDOW_MS) - (long)nowMs;
            if (remaining < 0) remaining = 0;
            char buf[80];
            snprintf(buf, sizeof(buf),
                "[AUTO:COLLECT] Baseline collection... %ld s remaining", remaining / 1000L);
            Serial.println(buf);
            SerialBT.println(buf);
        }

        if (nowMs - autoPhaseStartMs >= BASELINE_WINDOW_MS)
        {
            computeStats(autoPhaseStartMs, &baselineBPM, &avgInterval_s, &minInterval_s, &maxInterval_s);
            currentBPM = baselineBPM;

            printStatsSerial("BASELINE RESULT", baselineBPM, avgInterval_s, minInterval_s, maxInterval_s);

            SerialBT.println("══════════════════════════════");
            SerialBT.printf("  Baseline BPM   : %.1f\n",    baselineBPM);
            SerialBT.printf("  Avg interval   : %.2f s\n",  avgInterval_s);
            SerialBT.printf("  Min interval   : %.2f s\n",  minInterval_s);
            SerialBT.printf("  Max interval   : %.2f s\n",  maxInterval_s);
            SerialBT.printf("  Therapy starts if BPM < %.1f\n",
                            baselineBPM - BPM_HYSTERESIS);
            SerialBT.println("══════════════════════════════");

            autoState = AUTO_WATCH;
            Serial.println("[AUTO] → WATCH state");
        }
        break;
    }

    case AUTO_WATCH:
    {
        unsigned long winStart = (nowMs > LIVE_BPM_WINDOW_MS) ? (nowMs - LIVE_BPM_WINDOW_MS) : 0;
        computeStats(winStart, &currentBPM, &avgInterval_s, &minInterval_s, &maxInterval_s);

        if (baselineBPM > 0.0f && currentBPM > 0.0f && currentBPM < (baselineBPM - BPM_HYSTERESIS))
        {
            therapySpeedLVL    = 1;
            targetSpeed        = map(therapySpeedLVL, 0, 10, 1074, 1800);
            prevCycleBPM       = currentBPM;
            lastTherapyCycleMs = nowMs;
            rampToSpeed(targetSpeed);

            Serial.printf("[AUTO] BPM dropped to %.1f (< threshold %.1f) — THERAPY START, Level %d\n", currentBPM, baselineBPM - BPM_HYSTERESIS, therapySpeedLVL);

            SerialBT.println("══════════════════════════════");
            SerialBT.printf("  THERAPY STARTED\n");
            SerialBT.printf("  BPM: %.1f  Fan Level: %d\n", currentBPM, therapySpeedLVL);
            SerialBT.println("══════════════════════════════");

            autoState = AUTO_THERAPY;
        }
        break;
    }

    case AUTO_THERAPY:
    {
        if (nowMs - lastTherapyCycleMs >= THERAPY_CYCLE_MS)
        {
            lastTherapyCycleMs = nowMs;

            unsigned long cycleStart = (nowMs > THERAPY_CYCLE_MS) ? (nowMs - THERAPY_CYCLE_MS) : 0;
            float cycleBPM, cycleAvg, cycleMn, cycleMx;
            computeStats(cycleStart, &cycleBPM, &cycleAvg, &cycleMn, &cycleMx);
            currentBPM    = cycleBPM;
            avgInterval_s = cycleAvg;
            minInterval_s = cycleMn;
            maxInterval_s = cycleMx;

            printStatsSerial("THERAPY CYCLE", cycleBPM, cycleAvg, cycleMn, cycleMx);
            SerialBT.printf("[THERAPY CYCLE] BPM:%.1f Avg:%.2fs Min:%.2fs Max:%.2fs\n", cycleBPM, cycleAvg, cycleMn, cycleMx);

            if (cycleBPM >= (baselineBPM - RECOVERY_MARGIN))
            {
                therapySpeedLVL = 0;
                targetSpeed     = 1074;
                rampToSpeed(targetSpeed);
                autoState = AUTO_WATCH;
                Serial.println("[AUTO] Patient RECOVERED — fan stopped → WATCH");
                SerialBT.println("[AUTO] Patient recovered — therapy stopped.");
                break;
            }

            if (cycleBPM > prevCycleBPM + BPM_TOLERANCE)
            {
                therapySpeedLVL = max(1, therapySpeedLVL - 1);
                Serial.printf("[AUTO] IMPROVING (%.1f → %.1f) — Fan ↓ Level %d\n", prevCycleBPM, cycleBPM, therapySpeedLVL);
                SerialBT.printf("[AUTO] Improving → Fan Level: %d\n", therapySpeedLVL);
            }
            else if (cycleBPM < prevCycleBPM - BPM_TOLERANCE)
            {
                therapySpeedLVL = min(10, therapySpeedLVL + 1);
                Serial.printf("[AUTO] WORSENING (%.1f → %.1f) — Fan ↑ Level %d\n", prevCycleBPM, cycleBPM, therapySpeedLVL);
                SerialBT.printf("[AUTO] Worsening → Fan Level: %d\n", therapySpeedLVL);
            }
            else
            {
                Serial.printf("[AUTO] STABLE (%.1f) — Fan holding Level %d\n", cycleBPM, therapySpeedLVL);
                SerialBT.printf("[AUTO] Stable → Fan Level: %d (unchanged)\n", therapySpeedLVL);
            }

            prevCycleBPM = cycleBPM;
            targetSpeed  = map(therapySpeedLVL, 0, 10, 1074, 1800);
            rampToSpeed(targetSpeed);
        }
        break;
    }
    }
}

//********************************************************************** HUMIDIFIER CONTROL
void handleHumidifier(int sensorValue, unsigned long nowMs)
{
    humEnabled = (sensorValue < HUM_ADC_THRESHOLD);

    if (!humEnabled)
    {
        if (humRelayActive) 
        {
            humRelayActive = false;
            digitalWrite(HUMIDIFIER_PIN, LOW);
        }
        return;
    }

    if (humRelayActive)
    {
        if (nowMs - humLastToggle >= HUM_ON_TIME) 
        {
            humRelayActive = false;
            humLastToggle  = nowMs;
            digitalWrite(HUMIDIFIER_PIN, LOW);
        }
    }
    else
    {
        if (nowMs - humLastToggle >= HUM_OFF_TIME) 
        {
            humRelayActive = true;
            humLastToggle  = nowMs;
            digitalWrite(HUMIDIFIER_PIN, HIGH);
        }
    }
}

// *************************************************************************** READ SENSOR
int readSensor()
{
    const int N = 9;
    int r[N];
    for (int i = 0; i < N; i++) { r[i] = analogRead(SENSOR_PIN); delay(2); }

    for (int i = 0; i < N - 1; i++)
        for (int j = 0; j < N - i - 1; j++)
            if (r[j] > r[j+1]) { int t = r[j]; r[j] = r[j+1]; r[j+1] = t; }

    int median = r[N / 2];
    filteredValue = (EMA_ALPHA * median) + ((1.0f - EMA_ALPHA) * filteredValue);
    return (int)filteredValue;
}

// ************************************************************************** SENSOR CALIBRATION
void calibrateSensor()
{
    display.clearDisplay();
    display.setTextSize(1); 
    display.setTextColor(WHITE);
    display.setCursor(5, 20); 
    display.println("Calibrating sensor");
    display.setCursor(5, 34); 
    display.println("Keep mask clear...");
    display.display();

    Serial.println("[CAL] Calibrating sensor — keep mask clear for 3 s...");
    SerialBT.println("[CAL] Calibrating sensor — keep mask clear for 3 s...");

    //---------------------------------------- Sensor ADC noise reduction
    long sum = 0;
    for (int i = 0; i < 50; i++) 
    { 
        sum += analogRead(SENSOR_PIN); delay(60); 
    }
    sensorBaseline = (int)(sum / 50);
    localMean      = (float)sensorBaseline;
    filteredValue  = (float)sensorBaseline;

    Serial.printf("[CAL] Sensor baseline: %d ADC\n", sensorBaseline);
    SerialBT.printf("[CAL] Sensor baseline: %d ADC\n", sensorBaseline);
}

// ********************************************************** BLUETOOTH PARSER
void parseBTInput()
{
    if (!SerialBT.available()) return;

    String input = "";
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

    if (upper == "A" || upper == "AUTO")
    {
        systemMode = MODE_AUTO;
        resetAutoState();
        SerialBT.println("══════════════════════════════");
        SerialBT.println("  AUTO MODE ACTIVATED");
        SerialBT.println("  Collecting 60 s baseline.");
        SerialBT.println("  Breathe normally, mask off.");
        SerialBT.println("══════════════════════════════");
        Serial.println("[MODE] AUTO — baseline collection started.");
        return;
    }

    if (upper == "M" || upper == "MANUAL")
    {
        systemMode     = MODE_MANUAL;
        targetSpeed    = 1074;
        manualSpeedLVL = 0;
        rampToSpeed(targetSpeed);
        SerialBT.println("══════════════════════════════");
        SerialBT.println("  MANUAL MODE");
        SerialBT.println("  Send speed level 0–10.");
        SerialBT.println("══════════════════════════════");
        Serial.println("[MODE] MANUAL.");
        return;
    }

    if (upper == "R" || upper == "RESET")
    {
        systemMode  = MODE_NONE;
        targetSpeed = 1074;
        rampToSpeed(targetSpeed);
        Serial.println("[MODE] RESET → MODE_NONE");
        SerialBT.println("System reset. Send 'A' or 'M' to begin.");
        return;
    }

    if (upper == "C" || upper == "CAL")
    {
        systemMode  = MODE_NONE;
        targetSpeed = 1074;
        rampToSpeed(targetSpeed);
        calibrateSensor();
        tareHX710B();
        sendModePrompt();
        return;
    }

    if (systemMode == MODE_MANUAL)
    {
        bool isNum = true;
        for (int i = 0; i < (int)input.length(); i++)
            if (!isDigit(input[i])) { isNum = false; break; }

        if (isNum) 
        {
            int lvl        = constrain(input.toInt(), 0, 10);
            manualSpeedLVL = lvl;
            targetSpeed    = map(lvl, 0, 10, 1074, 1800);
            rampToSpeed(targetSpeed);
            Serial.printf("[MANUAL] Speed %d/10, PWM %d µs\n", lvl, targetSpeed);
            SerialBT.printf("MANUAL Speed: %d/10  PWM: %d µs\n", lvl, targetSpeed);
        } 
        else 
        {
            Serial.println("Unknown command. Send 0–10, or 'A'/'R' to change mode.");
            SerialBT.println("Unknown command. Send 0–10, or 'A'/'R' to change mode.");
        }
    }
    else if (systemMode == MODE_AUTO)
    {
        Serial.println("In AUTO mode. Send 'M' to switch to Manual, 'R' to reset.");
        SerialBT.println("In AUTO mode. Send 'M' to switch to Manual, 'R' to reset.");
    }
    else
    {
        Serial.println("Send 'A' (Auto) or 'M' (Manual) to begin.");
        SerialBT.println("Send 'A' (Auto) or 'M' (Manual) to begin.");
    }
}

//*****************************************************************  MODE PROMPT BT
void sendModePrompt()
{
    SerialBT.println("══════════════════════════════════");
    SerialBT.println("       BreathEZ CPAP v2.0         ");
    SerialBT.println("══════════════════════════════════");
    SerialBT.println("  A  :  Auto Mode");
    SerialBT.println("        (60 s calibration)");
    SerialBT.println("  M  :  Manual Mode  [0–10]");
    SerialBT.println("  C  :  Recalibrate sensor");
    SerialBT.println("  R  :  Reset");
    SerialBT.println("══════════════════════════════════");
}

// ******************************************************************************************************************* OLED UPDATE
void updateOLED(float bpm, float cmH2O, int speedLVL, bool humOn, const char* modeLabel, unsigned long nowMs)
{
    display.clearDisplay();
    display.setTextColor(WHITE);

    display.setTextSize(1);
    display.setCursor(0, 0);  
    display.print("BreathEZ");
    display.setCursor(86, 0); 
    display.print(modeLabel);
    display.drawLine(0, 10, 127, 10, WHITE);

    if (systemMode == MODE_AUTO && autoState == AUTO_COLLECT)
    {
        long remaining = (long)(autoPhaseStartMs + BASELINE_WINDOW_MS) - (long)nowMs;
        if (remaining < 0) remaining = 0;
        display.setCursor(0, 15);
        display.print("Collecting baseline");
        display.setCursor(0, 27);
        display.print("Remaining: ");
        display.print(remaining / 1000L);
        display.print(" s");
        display.setCursor(0, 42);
        display.print("BPM so far: ");
        display.print(bpm, 1);
        display.display();
        return;
    }

    display.setTextSize(1);
    display.setCursor(5, 20);
    display.print(bpm, 1);
    display.setTextSize(1);
    display.setCursor(40, 20);
    display.print("BPM");

    display.setTextSize(1);
    display.setCursor(0, 34);
    display.print("Press:");
    display.print(cmH2O, 1);
    display.print(" cmH2O");

    display.setCursor(0, 46);
    display.print("Humid: ");
    display.print(humOn ? "ON " : "OFF");

    display.setCursor(0, 56);
    display.print("Fan: ");
    display.print(speedLVL);
    display.print("/10  PWM:");
    display.print(targetSpeed);

    display.display();
}

// ******************************************************************************* BT TELEMETRY
void sendBTTelemetry(float bpm, float cmH2O, int speedLVL, bool humOn)
{
    const char* stateStr;
    if (systemMode == MODE_AUTO) 
    {
        if      (autoState == AUTO_COLLECT) stateStr = "COLLECT";
        else if (autoState == AUTO_WATCH)   stateStr = "WATCH";
        else                                stateStr = "THERAPY";
    } 
    else if (systemMode == MODE_MANUAL) 
    {
        stateStr = "MANUAL";
    } 
    else 
    {
        stateStr = "IDLE";
    }

    SerialBT.printf(
        "State:%-7s | BPM:%5.1f | Base:%5.1f | "
        "Avg:%5.2fs Min:%5.2fs Max:%5.2fs | "
        "Press:%6.2f cmH2O | Fan:%2d/10 PWM:%4d | Humid:%s | Relay2:%s\n",
        stateStr,
        bpm, baselineBPM,
        avgInterval_s, minInterval_s, maxInterval_s,
        cmH2O,
        speedLVL, targetSpeed,
        humOn ? "ON" : "OFF",
        relay2Active ? "ON" : "OFF"
    );
}

// ******************************************************************** SERIAL STATS PRINT
void printStatsSerial(const char* label, float bpm, float avg, float mn, float mx)
{
    Serial.println("─────────────────────────────────────────────");
    Serial.printf("  [%s]\n", label);
    Serial.printf("  BPM          : %.2f\n",  bpm);
    Serial.printf("  Avg interval : %.2f s\n", avg);
    Serial.printf("  Min interval : %.2f s\n", mn);
    Serial.printf("  Max interval : %.2f s\n", mx);
    Serial.println("─────────────────────────────────────────────");
}

// ******************************************************************* RESET AUTO STATE
void resetAutoState()
{
    autoState          = AUTO_COLLECT;
    autoPhaseStartMs   = millis();
    lastTherapyCycleMs = 0;
    prevCycleBPM       = 0.0f;
    baselineBPM        = 0.0f;
    currentBPM         = 0.0f;
    peakCount          = 0;
    peakHead           = 0;
    therapySpeedLVL    = 1;
    targetSpeed        = 1074;
    rampToSpeed(targetSpeed);
}

// ******************************************************************************************** ESC RAMP
void rampToSpeed(int target)
{
    target = constrain(target, 1000, 2000);
    if (currentSpeed == target) return;

    if (currentSpeed < target)
        for (int s = currentSpeed; s <= target; s += RAMP_STEP)
            { 
                esc.writeMicroseconds(s); 
                delay(RAMP_DELAY); 
            }
    else
        for (int s = currentSpeed; s >= target; s -= RAMP_STEP)
            { 
                esc.writeMicroseconds(s); 
                delay(RAMP_DELAY); 
            }

    esc.writeMicroseconds(target);
    currentSpeed = target;
    Serial.printf("[ESC] Target PWM: %d µs\n", target);
}

// ********************************************************************* HX710B RAW READ
long readHX710B_raw()
{
    unsigned long t0 = millis();
    while (digitalRead(HX710B_DOUT) == HIGH)
    {
        if (millis() - t0 > 200) 
        {
            Serial.println("!!ERROR!! HX710B timeout — check wiring/power!");
            return 0;
        }
        delay(1);
    }

    long data = 0;
    for (int i = 0; i < 24; i++) {
        digitalWrite(HX710B_SCK, HIGH); 
        delayMicroseconds(2);
        int bit = digitalRead(HX710B_DOUT);
        digitalWrite(HX710B_SCK, LOW);  
        delayMicroseconds(2);
        data = (data << 1) | bit;
    }
    digitalWrite(HX710B_SCK, HIGH); 
    delayMicroseconds(2);
    digitalWrite(HX710B_SCK, LOW); 
    delayMicroseconds(2);

    if (data & 0x800000) data |= 0xFF000000;
    return data;
}

//****************************************************************** HX710B → hPa
float readPressure_hPa()
{
    long raw       = readHX710B_raw();
    long corrected = raw - hx710b_offset;
    float hpa      = (float)corrected / hx710b_scale_hpa;
    return max(hpa, 0.0f);
}

// ************************************************************** HX710B TARE
void tareHX710B()
{
    display.clearDisplay();
    display.setTextSize(1); display.setTextColor(WHITE);
    display.setCursor(5, 18); display.println("Taring pressure...");
    display.setCursor(5, 32); display.println("Keep mask off nozzle");
    display.display();

    Serial.println("[TARE] HX710B tare — ensure no airflow...");

    for (int i = 0; i < 3; i++) 
    { 
        readHX710B_raw(); 
        delay(100); 
    }

    long sum = 0;
    for (int i = 0; i < 20; i++) 
    { 
        sum += readHX710B_raw(); 
        delay(100); 
    }
    hx710b_offset = sum / 20;

    Serial.printf("[TARE] HX710B offset: %ld\n", hx710b_offset);
}
