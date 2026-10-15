# Room Air Quality Monitor

An Arduino UNO R4 WiFi module that tracks indoor air quality, temperature, humidity, and pressure with a Bosch BME680. Readings appear on a 0.96" OLED and an on-board LED matrix, a color-coded RGB LED shows the temperature range at a glance, and the data is also available over Bluetooth Low Energy. A light sensor switches the device between an active mode and a dark-room night-light mode.

## Features

- **Air quality:** Bosch BSEC2 converts the BME680 gas resistance into a Static IAQ index (0 to 500) and an estimated CO2 equivalent.
- **Ambient conditions:** heat-compensated temperature, relative humidity, and barometric pressure.
- **"Feels like" temperature:** NWS heat index computed from temperature and humidity.
- **OLED readout:** five live values plus a status line that reports sensor calibration state.
- **LED matrix:** alternates between ambient temperature (`A`) and feels-like temperature (`F`) every 4 s.
- **RGB temperature indicator:** one glance tells you whether the room is cold, cool, comfortable, warm, or hot, with no need to read the display.
- **Automatic night mode:** a light sensor turns the display and RGB indicator off and a white night light on when the room goes dark.
- **BLE output:** IAQ, temperature, humidity, and pressure notify a connected phone or computer every 2 s.

## Hardware

| Part | Role | Interface |
|---|---|---|
| Arduino UNO R4 WiFi | Controller, 12x8 LED matrix, BLE radio | n/a |
| BME680 breakout | Gas (VOC), temperature, humidity, pressure | I2C, address 0x77 (falls back to 0x76) |
| SSD1306 0.96" OLED, 128x64 | Readout | I2C, address 0x3C |
| Common-cathode RGB LED | Temperature indicator | PWM |
| Photodiode/light sensor module | Day/night detection | Analog |
| White LEDs | Night light | Digital |

### Pin map

| Signal | Pin |
|---|---|
| I2C SDA / SCL | A4 / A5 (default `Wire`), 100 kHz |
| RGB LED red / green / blue | D5 / D6 / D9 (PWM) |
| Light sensor output | A0 |
| White night-light LEDs | D13 |

Use a current-limiting resistor on every LED.

## Software setup

1. Install the Arduino IDE and the **Arduino UNO R4 Boards** core.
2. In Library Manager, install:
   - `BSEC2 Software Library` (Bosch Sensortec)
   - `Adafruit GFX Library`
   - `Adafruit SSD1306`
   - `ArduinoGraphics`
   - `ArduinoBLE`
   - `Arduino_LED_Matrix` ships with the UNO R4 core, so it needs no separate install.
3. Open `BME680_Display_wIAQ.ino`, select **Arduino UNO R4 WiFi**, and upload.
4. Open the Serial Monitor at 115200 baud to see one log line per sensor update.

BSEC2 is distributed under Bosch's own license and is not open source, so install it through Library Manager and do not commit it to this repo.

## How it works

### Air quality (IAQ and eCO2)

The BME680 gas sensor is a metal-oxide surface heated to several hundred degrees Celsius. Volatile organic compounds (VOCs) react at that surface and change its electrical resistance, so lower resistance means more VOCs in the air. The raw resistance depends on the sensor's history, so BSEC2 processes it into a Static IAQ index.

| Static IAQ | Air quality |
|---|---|
| 0 to 50 | Excellent |
| 51 to 100 | Good |
| 101 to 150 | Lightly polluted |
| 151 to 200 | Moderately polluted |
| 201 to 250 | Heavily polluted |
| 251 to 350 | Severely polluted |
| above 350 | Extremely polluted |

The CO2 value is an **estimate** inferred from VOC levels, not a direct CO2 measurement. It tracks human occupancy and ventilation reasonably well but cannot replace an NDIR CO2 sensor.

### Calibration status

BSEC2 reports an accuracy level from 0 to 3 alongside IAQ. The OLED header translates it:

| BSEC accuracy | Header text | IAQ shown |
|---|---|---|
| no data yet | `SYSTEM STARTING...` | `---` |
| 0 | `ENV [STABILIZING]` | `---` |
| 1 to 2 | `ENV [CALIBRATING]` | value |
| 3 | `ROOM ENVIRONMENT` | value |

Calibration needs the sensor to see both clean air and an air-quality change, so it can take from several minutes to a few hours. Treat IAQ values as indicative until the header reads `ROOM ENVIRONMENT`.

### Temperature indicator and "feels like"

BSEC2 outputs heat-compensated temperature and humidity, which remove the sensor's own self-heating from the reading. The "feels like" value is the NWS heat index (Rothfusz regression, with the simple Steadman formula below 80 F). Physically, high humidity slows sweat evaporation, which is the body's main cooling path, so the same air temperature feels warmer.

The RGB LED color follows the **feels-like** value:

| Feels like | Color |
|---|---|
| below 20 C | Blue |
| 20 to below 22 C | Purple |
| 22 to 25 C | Green |
| above 25 to below 27 C | Orange |
| 27 C and above | Red |

PWM duty cycles are kept low on purpose so the LED is visible without being bright in a dark room.

### Night mode

The light sensor is read through the R4's 14-bit ADC (0 to 16383). The sensor module's output **rises** as the room gets darker.

| State | Condition | Behavior |
|---|---|---|
| Night | reading above 5700 | OLED off, RGB LED off, white LEDs on |
| Active | reading below 5300 | OLED on, RGB LED on, white LEDs off |

The two thresholds sit 200 counts either side of a 5500 midpoint. This hysteresis works like a Schmitt trigger: once the state flips, the reading must travel 400 counts the other way to flip it back, so a reading hovering near the threshold cannot make the lights flicker.

The LED matrix keeps running in night mode.

### Bluetooth Low Energy

The device advertises as `RP-UNO_R4_Sensor` and exposes an Environmental Sensing service (0x181A). Each characteristic supports read and notify and holds a little-endian 32-bit float. Values update every 2 s while a client is connected.

| Characteristic | UUID | Unit |
|---|---|---|
| IAQ score | 0x2A66 | index |
| Temperature | 0x2A6E | C |
| Humidity | 0x2A6F | % RH |
| Pressure | 0x2A6D | hPa |

In nRF Connect or a similar app, set the value format to **float32**. Generic apps that decode these UUIDs by the Bluetooth SIG definitions expect scaled integers and will show wrong numbers.

## Configuration

| Constant | Default | Effect |
|---|---|---|
| `lightThreshold` | 5500 | Midpoint between day and night readings |
| `hysteresisBuffer` | 200 | Half-width of the switching band |
| `displayInterval` | 2000 ms | OLED and BLE update period |
| `toggleInterval` | 4000 ms | LED matrix swap between `A` and `F` |
| `BSEC_SAMPLE_RATE_LP` | 3 s per sample | BSEC2 low-power sampling mode |

Tune `lightThreshold` for your sensor and room by printing the raw `analogRead` value in daylight and in the dark.

## Known limitations and ideas

- Calibration restarts at every power cycle. Saving the BSEC2 state to the R4's data flash and restoring it at boot would skip most of the warm-up.
- CO2 is estimated, and the estimate is computed but not yet shown on the OLED or sent over BLE.
- Static IAQ does not adapt its baseline over time, so a space that is always polluted will read as normal after long exposure.
- BLE uses SIG-assigned UUIDs with non-standard data formats. Custom 128-bit UUIDs would avoid confusion with generic apps.
- Possible additions: a 3D-printed enclosure, an IAQ trend graph on the OLED, and logging to a phone app.

## License

Add your preferred license here (MIT is common for hobby projects). BSEC2 keeps Bosch's license regardless.
