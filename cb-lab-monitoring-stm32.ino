#include <Wire.h>

// ================= UART =================
HardwareSerial ZphsSerial(PA10, PA9);   // Air sensor
HardwareSerial EspSerial (PC7,  PC6);   // ESP32

// ================= PINS =================
#define TURBIDITY_PIN  A2
#define PH_PIN         A1
#define TDS_PIN        A0

// ================= CONSTANTS =================
#define VREF            3.3f
#define ADC_RES         4095.0f
#define SAMPLES         10
#define DIVIDER_SCALE   1.5f
#define PH_NEUTRAL_V    1.65f
#define PH_SLOPE       -0.18f

#define ZPHS_BAUD       9600
#define ZPHS_RESP_LEN   26

const byte ZPHS_CMD[9] = {0xFF,0x01,0x86,0,0,0,0,0,0x79};

// ================= TIMING =================
#define SENSOR_INTERVAL   500    // faster reading
#define PUBLISH_INTERVAL  1000   // send every 1 sec
#define AIR_REQ_INTERVAL  2000   // request air sensor

unsigned long lastSensor = 0;
unsigned long lastSend   = 0;
unsigned long lastAirReq = 0;

// ================= GLOBALS =================
float g_turbidity = 0, g_ph = 0, g_tds = 0;
float ambientTemp = 25.0f;

struct AirData {
  bool valid;
  int pm1, pm25, pm10, co2, voc;
  float temp, hum, ch2o, co, o3, no2;
} air;

// ================= HELPERS =================
float safeF(float v) {
  if (isnan(v) || isinf(v)) return 0.0f;
  return v;
}

float readAvgVoltage(int pin, bool divider=false) {
  long sum = 0;
  for(int i=0;i<SAMPLES;i++) sum += analogRead(pin);
  float v = (sum / (float)SAMPLES) * (VREF / ADC_RES);
  return divider ? v * DIVIDER_SCALE : v;
}

float readTurbidity() {
  // 1. Read the raw voltage from the sensor
  float v = readAvgVoltage(TURBIDITY_PIN, false); 

  // 2. Your custom calibration values
  float voltClean = 1.5f;    // Voltage in completely clear water (0 NTU)
  float voltDirty = 2.5f;    // Voltage when completely blocked (Max NTU)
  float maxNTU    = 3000.0f; // The standard maximum NTU scale for these sensors

  // 3. Failsafes: Keep the reading within bounds
  // If the voltage drops below 1.5V (e.g., 1.45V), just call it 0 NTU
  if (v <= voltClean) return 0.0f; 
  
  // If the voltage goes above 2.5V (e.g., 2.6V), cap it at 3000 NTU
  if (v >= voltDirty) return maxNTU;

  // 4. The Calibration Math (Linear Mapping)
  float ntu = ((v - voltClean) / (voltDirty - voltClean)) * maxNTU;
  
  return safeF(ntu);
}

float readPH() {
  float v = readAvgVoltage(PH_PIN);
  return safeF(7.0f + (v - PH_NEUTRAL_V) / PH_SLOPE);
}

float readTDS() {
  float v = readAvgVoltage(TDS_PIN);
  float coeff = 1.0f + 0.02f*(ambientTemp - 25.0f);
  float cv = v / coeff;
  float tds = (133.42f*cv*cv*cv - 255.86f*cv*cv + 857.39f*cv)*0.5f;
  return safeF(tds);
}

// ================= NON-BLOCKING AIR READ =================
void requestAirSensor() {
  ZphsSerial.write(ZPHS_CMD, sizeof(ZPHS_CMD));
}

void readAirSensor() {
  static byte buf[ZPHS_RESP_LEN];
  static int index = 0;

  while (ZphsSerial.available()) {
    byte b = ZphsSerial.read();

    if (index == 0 && b != 0xFF) continue;

    buf[index++] = b;

    if (index == ZPHS_RESP_LEN) {
      index = 0;

      if (buf[1] == 0x86) {
        air.valid = true;
        air.pm1  = (buf[2]<<8)|buf[3];
        air.pm25 = (buf[4]<<8)|buf[5];
        air.pm10 = (buf[6]<<8)|buf[7];
        air.co2  = (buf[8]<<8)|buf[9];
        air.voc  = buf[10];
        air.temp = (((buf[11]<<8)|buf[12]) - 500)*0.1f;
        air.hum  = (buf[13]<<8)|buf[14];
        air.ch2o = ((buf[15]<<8)|buf[16])*0.001f;
        air.co   = ((buf[17]<<8)|buf[18])*0.1f;
        air.o3   = ((buf[19]<<8)|buf[20])*0.01f;
        air.no2  = ((buf[21]<<8)|buf[22])*0.01f;

        ambientTemp = air.temp;
      }
    }
  }
}

// ================= SERIAL PRINT =================
void printData() {
  Serial.print("Turbidity: "); Serial.print(g_turbidity);
  Serial.print(" | pH: "); Serial.print(g_ph);
  Serial.print(" | TDS: "); Serial.print(g_tds);
  Serial.print(" | CO2: "); Serial.println(air.co2);
}

// ================= SEND JSON =================
void sendESP() {
  String payload = "{";
  
  // Water Data
  payload += "\"turbidity\":" + String(g_turbidity, 1) + ",";
  payload += "\"ph\":"        + String(g_ph, 2) + ",";
  payload += "\"tds\":"       + String(g_tds, 1) + ",";

  // Air Data
  if (air.valid) {
    payload += "\"pm1\":"      + String(air.pm1) + ",";
    payload += "\"pm25\":"     + String(air.pm25) + ",";
    payload += "\"pm10\":"     + String(air.pm10) + ",";
    payload += "\"co2\":"      + String(air.co2) + ",";
    payload += "\"voc\":"      + String(air.voc) + ",";
    payload += "\"temp\":"     + String(air.temp, 1) + ",";
    payload += "\"humidity\":" + String(air.hum, 1) + ",";
    payload += "\"ch2o\":"     + String(air.ch2o, 3) + ",";
    payload += "\"co\":"       + String(air.co, 1) + ",";
    payload += "\"o3\":"       + String(air.o3, 2) + ",";
    payload += "\"no2\":"      + String(air.no2, 2);
  } else {
    // Failsafe: Send zeros if air sensor hasn't reported yet to prevent ESP32 JSON crash
    payload += "\"pm1\":0,\"pm25\":0,\"pm10\":0,\"co2\":0,\"voc\":0,\"temp\":0.0,\"humidity\":0.0,\"ch2o\":0.0,\"co\":0.0,\"o3\":0.0,\"no2\":0.0";
  }
  
  payload += "}";

  EspSerial.println(payload);
}

// ================= SETUP =================
void setup() {
  Serial.begin(115200);
  ZphsSerial.begin(ZPHS_BAUD);
  EspSerial.begin(115200);

  analogReadResolution(12);

  Serial.println("SYSTEM STARTED");
}

// ================= LOOP =================
void loop() {
  unsigned long now = millis();

  // ---- Continuous air sensor read ----
  readAirSensor();

  // ---- Request air data periodically ----
  if (now - lastAirReq >= AIR_REQ_INTERVAL) {
    lastAirReq = now;
    requestAirSensor();
  }

  // ---- Read analog sensors ----
  if (now - lastSensor >= SENSOR_INTERVAL) {
    lastSensor = now;
    g_turbidity = readTurbidity();
    g_ph        = readPH();
    g_tds       = readTDS();
  }

  // ---- Print to Serial ----
  if (now - lastSend >= PUBLISH_INTERVAL) {
    lastSend = now;
    printData();
    sendESP();
  }
}