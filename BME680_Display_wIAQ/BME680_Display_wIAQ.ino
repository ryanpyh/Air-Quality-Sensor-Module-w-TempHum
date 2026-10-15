#include <Wire.h>
#include <bsec2.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <ArduinoGraphics.h>
#include "Arduino_LED_Matrix.h"
#include <ArduinoBLE.h>

// Hardware Pin Definitions
#define redledPin 5
#define greenledPin 6
#define blueledPin 9
#define lightA0Pin A0
#define whiteledVinPin 13

#define SEALEVELPRESSURE_HPA (1013.25)

// OLED Configuration
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define SCREEN_ADDRESS 0x3C
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// BSEC2 and System Objects
Bsec2 envSensor;
ArduinoLEDMatrix matrix;

// Bluetooth identifiers
BLEService customService("181A");
BLEFloatCharacteristic bleIaq("2A66", BLERead | BLENotify);   // IAQ Score
BLEFloatCharacteristic bleTemp("2A6E", BLERead | BLENotify);  // Temperature
BLEFloatCharacteristic bleHum("2A6F", BLERead | BLENotify);   // Humidity
BLEFloatCharacteristic blePres("2A6D", BLERead | BLENotify);  // Pressure

BLEDescriptor iaqDescriptor("2901", "IAQ Score");
BLEDescriptor tempDescriptor("2901", "Temperature (C)");
BLEDescriptor humDescriptor("2901", "Humidity (%)");
BLEDescriptor presDescriptor("2901", "Pressure (hPa)");

// Global Environmental Data
float iaqScore = 0.0;
float eco2Value = 0.0;
float compTemp = 0.0;
float compHum = 0.0;
float pressureHpa = 0.0;
uint8_t iaqAccuracy = 0;

// Light Sensing Logic
volatile bool isDark = false;
bool lastDarkState = false;
const int lightThreshold = 5500;
const int hysteresisBuffer = 200;
const int darkLimit = lightThreshold + hysteresisBuffer;
const int brightLimit = lightThreshold - hysteresisBuffer;

// Timing Management
unsigned long lastDisplayUpdate = 0;
const long displayInterval = 2000;
unsigned long bootTime;

// Matrix Toggling Variables
bool showFeelsLike = false;
unsigned long lastTempDisplayToggle = 0;
const long toggleInterval = 4000;

// Function Prototypes
void checkBsecStatus(Bsec2 &bsec);
void newDataCallback(const bme68x_data data, const bsecOutputs outputs, Bsec2 bsec);
void updateMatrix(bool feelsLikeMode);
void updateOLED();
void updateRGBLED(float currentHI);
void updateLightState();
float calculateHeatIndex(float tempC, float humidity);
void updateBluetooth();

void setup() {
  Serial.begin(115200);
  bootTime = millis();

  Wire.begin();
  Wire.setClock(100000);
  delay(100);

  while (!Serial && millis() < 5000)
    ;

  analogReadResolution(14);
  matrix.begin();

  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("OLED Init Failed"));
  }

  display.clearDisplay();
  display.display();

  pinMode(redledPin, OUTPUT);
  pinMode(greenledPin, OUTPUT);
  pinMode(blueledPin, OUTPUT);
  pinMode(whiteledVinPin, OUTPUT);
  pinMode(lightA0Pin, INPUT);

  // Initialize BSEC2
  bool sensorFound = envSensor.begin(BME68X_I2C_ADDR_HIGH, Wire);
  if (!sensorFound) {
    sensorFound = envSensor.begin(BME68X_I2C_ADDR_LOW, Wire);
  }

  if (!sensorFound) {
    checkBsecStatus(envSensor);
    display.clearDisplay();
    display.setCursor(0, 20);
    display.setTextSize(1);
    display.println(F("BME680 NOT FOUND"));
    display.display();
  } else {
    bsec_virtual_sensor_t sensorList[] = {
      BSEC_OUTPUT_STATIC_IAQ,
      BSEC_OUTPUT_CO2_EQUIVALENT,
      BSEC_OUTPUT_SENSOR_HEAT_COMPENSATED_TEMPERATURE,
      BSEC_OUTPUT_SENSOR_HEAT_COMPENSATED_HUMIDITY,
      BSEC_OUTPUT_RAW_PRESSURE
    };

    if (!envSensor.updateSubscription(sensorList, 5, BSEC_SAMPLE_RATE_LP)) {
      checkBsecStatus(envSensor);
    }
    envSensor.attachCallback(newDataCallback);
  }

  updateLightState();
  updateOLED();

  Serial.println("\n--- Initializing BLE ---");
  if (!BLE.begin()) {
    Serial.println("Error: BLE hardware failed to start!");
  } else {
    BLE.setLocalName("RP-UNO_R4_Sensor");

    BLE.setAppearance(0x0540);

    BLE.setAdvertisedService(customService);

    bleIaq.addDescriptor(iaqDescriptor);
    customService.addCharacteristic(bleIaq);

    bleTemp.addDescriptor(tempDescriptor);
    customService.addCharacteristic(bleTemp);

    bleHum.addDescriptor(humDescriptor);
    customService.addCharacteristic(bleHum);

    blePres.addDescriptor(presDescriptor);
    customService.addCharacteristic(blePres);

    BLE.addService(customService);
    BLE.advertise();
    Serial.println("BLE Peripheral Active as 'RP-UNO_R4'");
  }
}

void loop() {
  updateLightState();

  if (!envSensor.run()) {
    // Basic engine run
  }

  BLE.poll();

  unsigned long currentMillis = millis();

  if (currentMillis - lastDisplayUpdate >= displayInterval) {
    lastDisplayUpdate = currentMillis;
    updateOLED();

    updateBluetooth();
  }

  if (currentMillis - lastTempDisplayToggle >= toggleInterval) {
    lastTempDisplayToggle = currentMillis;
    showFeelsLike = !showFeelsLike;
    updateMatrix(showFeelsLike);
  }
}

void newDataCallback(const bme68x_data data, const bsecOutputs outputs, Bsec2 bsec) {
  if (!outputs.nOutputs) return;

  for (uint8_t i = 0; i < outputs.nOutputs; i++) {
    const bsecData &output = outputs.output[i];

    switch (output.sensor_id) {
      case BSEC_OUTPUT_STATIC_IAQ:
        iaqScore = output.signal;
        iaqAccuracy = output.accuracy;
        break;
      case BSEC_OUTPUT_CO2_EQUIVALENT:
        eco2Value = output.signal;
        break;
      case BSEC_OUTPUT_SENSOR_HEAT_COMPENSATED_TEMPERATURE:
        compTemp = output.signal;
        break;
      case BSEC_OUTPUT_SENSOR_HEAT_COMPENSATED_HUMIDITY:
        compHum = output.signal;
        break;
      case BSEC_OUTPUT_RAW_PRESSURE:
        pressureHpa = output.signal;
        break;
    }
  }

  updateRGBLED(calculateHeatIndex(compTemp, compHum));

  float currentFL = calculateHeatIndex(compTemp, compHum);

  Serial.print(F("IAQ: "));
  Serial.print(iaqScore, 1);
  Serial.print(F(" | Acc: "));
  Serial.print(iaqAccuracy);
  Serial.print(F(" | Temp: "));
  Serial.print(compTemp, 1);
  Serial.print(F(" | Feels Like: "));
  Serial.print(currentFL, 1);
  Serial.print(F(" | Humidity: "));
  Serial.print(compHum, 1);
  Serial.print(F(" | Pressure: "));
  Serial.print(pressureHpa, 1);
  Serial.println(F(" hPa"));
}

float calculateHeatIndex(float tempC, float humidity) {
  float tempF = (tempC * 9.0 / 5.0) + 32.0;
  float RH = humidity;
  float HI = 0.5 * (tempF + 61.0 + ((tempF - 68.0) * 1.2) + (RH * 0.094));

  if (HI < 80) return (HI - 32.0) * (5.0 / 9.0);

  HI = -42.379 + 2.04901523 * tempF + 10.14333127 * RH - 0.22475541 * tempF * RH - 0.00683783 * tempF * tempF
       - 0.05481717 * RH * RH + 0.00122874 * tempF * tempF * RH + 0.00085282 * tempF * RH * RH - 0.00000199 * tempF * tempF * RH * RH;

  if (RH < 13.0 && tempF >= 80.0 && tempF <= 112.0) {
    HI -= ((13.0 - RH) / 4.0) * (sqrt((17.0 - abs(tempF - 95.0)) / 17.0));
  } else if (RH > 85.0 && tempF >= 80.0 && tempF <= 87.0) {
    HI += ((RH - 85.0) / 10.0) * ((87.0 - tempF) / 5.0);
  }

  return (HI - 32.0) * (5.0 / 9.0);
}

void updateOLED() {
  if (isDark) {
    display.clearDisplay();
    display.display();
    display.ssd1306_command(SSD1306_DISPLAYOFF);
    return;
  }

  display.ssd1306_command(SSD1306_DISPLAYON);
  
  display.clearDisplay();
  display.setCursor(0, 0);
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  if (compTemp == 0 && iaqAccuracy == 0) {
    display.println(F("SYSTEM STARTING..."));
  } else if (iaqAccuracy == 0) {
    display.println(F("ENV [STABILIZING]"));
  } else if (iaqAccuracy < 3) {
    display.println(F("ENV [CALIBRATING]"));
  } else {
    display.println(F("ROOM ENVIRONMENT"));
  }
  display.drawLine(0, 10, 128, 10, SSD1306_WHITE);

  display.setCursor(0, 16);

  display.print(F("IAQ:       "));
  if (iaqAccuracy > 0) display.println(iaqScore, 1);
  else display.println(F("---"));

  display.print(F("Temp:      "));
  if (compTemp != 0) {
    display.print(compTemp, 1);
    display.println(F(" C"));
  } else {
    display.println(F("---"));
  }

  display.print(F("Feels:     "));
  if (compTemp != 0) {
    display.print(calculateHeatIndex(compTemp, compHum), 1);
    display.println(F(" C"));
  } else {
    display.println(F("---"));
  }

  display.print(F("Humidity:  "));
  if (compHum != 0) {
    display.print(compHum, 2);
    display.println(F(" %"));
  } else {
    display.println(F("---"));
  }

  display.print(F("Pressure:  "));
  if (pressureHpa != 0) {
    display.print(pressureHpa, 2);
    display.println(F(" hPa"));
  } else {
    display.println(F("---"));
  }

  display.display();
}

void updateRGBLED(float currentHI) {
  if (isDark) {
    setColor(0, 0, 0);
    return;
  }

  if (currentHI < 20.0) {
    setColor(0, 0, 40);
  } else if (currentHI >= 20.0 && currentHI < 22.0) {
    setColor(60, 0, 60);
  } else if (currentHI > 25.0 && currentHI < 27.0) {
    setColor(75, 25, 0);
  } else if (currentHI >= 27.0) {
    setColor(50, 0, 0);
  } else {
    setColor(0, 50, 0);
  }
}

void updateLightState() {
  int currentLight = analogRead(lightA0Pin);

  bool nextState = isDark;
  if (!isDark && currentLight > darkLimit) {
    nextState = true;
  } else if (isDark && currentLight < brightLimit) {
    nextState = false;
  }

  isDark = nextState;

  if (isDark != lastDarkState) {
    lastDarkState = isDark;
    digitalWrite(whiteledVinPin, isDark ? HIGH : LOW);
    updateRGBLED(calculateHeatIndex(compTemp, compHum));
    updateMatrix(showFeelsLike);
  }
}

void updateMatrix(bool feelsLikeMode) {
  if (compTemp == 0) return;

  matrix.beginDraw();
  matrix.clear();
  matrix.stroke(0xFF);
  matrix.textFont(Font_4x6);

  float displayVal = feelsLikeMode ? calculateHeatIndex(compTemp, compHum) : compTemp;
  char label = feelsLikeMode ? 'F' : 'A';

  matrix.beginText(0, 1, 0xFF);
  matrix.print(label);
  matrix.endText();

  matrix.beginText(5, 1, 0xFF);
  matrix.print((int)round(displayVal));
  matrix.write(176);
  matrix.endText();

  matrix.endDraw();
}

void setColor(int r, int g, int b) {
  analogWrite(redledPin, r);
  analogWrite(greenledPin, g);
  analogWrite(blueledPin, b);
}

void checkBsecStatus(Bsec2 &bsec) {
  if (bsec.status < BSEC_OK) {
    Serial.println("BSEC error code: " + String(bsec.status));
  } else if (bsec.status > BSEC_OK) {
    Serial.println("BSEC warning code: " + String(bsec.status));
  }
}

// Standalone function for Bluetooth updates
void updateBluetooth() {
  BLEDevice central = BLE.central();

  // Only transmit if a central device (like a phone) is connected
  if (central && central.connected()) {
    // Update all environmental characteristics with current sensor global values
    bleIaq.writeValue(iaqScore);
    bleTemp.writeValue(compTemp);
    bleHum.writeValue(compHum);
    blePres.writeValue(pressureHpa);
  }
}