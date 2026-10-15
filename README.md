# Room Air Quality Monitor

An Arduino UNO R4 WiFi module that tracks air quality, temperature, humidity, and pressure with a Bosch BME680. A color-coded LED shows the room temperature at a glance, and a light sensor switches the device to a night light when the room goes dark.

## Features

- **Air quality:** Static IAQ index (0 to 500) and estimated CO2 equivalent, calculated by Bosch BSEC2
- **Comfort:** temperature, humidity, pressure, and a "feels like" heat index
- **OLED display:** live readings plus the sensor's calibration status
- **LED matrix:** alternates between ambient and feels-like temperature
- **RGB LED:** blue, purple, green, orange, or red by temperature range
- **Night mode:** display and RGB LED switch off and white night-light LEDs switch on in the dark
- **Bluetooth LE:** readings stream to a phone or computer

## Hardware

| Part | Role |
|---|---|
| Arduino UNO R4 WiFi | Controller, LED matrix, Bluetooth |
| BME680 breakout | Gas, temperature, humidity, pressure (I2C) |
| 0.96" SSD1306 OLED, 128x64 | Display (I2C, address 0x3C) |
| RGB LED | Temperature indicator |
| Photodiode light sensor module | Day/night detection |
| White LEDs | Night light |

| Signal | Pin |
|---|---|
| I2C SDA / SCL | A4 / A5 |
| RGB LED red / green / blue | D5 / D6 / D9 |
| Light sensor | A0 |
| White LEDs | D13 |

## Getting started

1. Install the **Arduino UNO R4 Boards** core in the Arduino IDE.
2. In Library Manager, install:
   - `BSEC2 Software Library`
   - `Adafruit GFX Library`
   - `Adafruit SSD1306`
   - `ArduinoGraphics`
   - `ArduinoBLE`
4. Open `BME680_Display_wIAQ.ino`, select **Arduino UNO R4 WiFi**, and upload.
5. Open the Serial Monitor at 115200 baud to watch the readings.

BSEC2 uses Bosch's own license, so install it through Library Manager and do not commit it to this repo.

## How it works

**Air quality.** The BME680 uses a heated metal-oxide gas sensor whose electrical resistance changes in response to VOCs and other gases in the surrounding air. The BSEC2 package converts that resistance into an IAQ index, where 0 to 50 indicates excellent air quality, 101 to 150 is lightly polluted, and above 250 is heavily polluted. The CO2 value is an estimate inferred from VOCs rather than a direct measurement.

**Calibration.** The sensor needs time to calibrate to a baseline for clean air, this can take from several minutes to a few hours. The OLED header shows progress:

| Header | Meaning |
|---|---|
| `SYSTEM STARTING...` / `ENV [STABILIZING]` | No usable IAQ yet |
| `ENV [CALIBRATING]` | IAQ shown, still settling |
| `ROOM ENVIRONMENT` | Fully calibrated |

**Feels-like temperature.** The sketch computes the NWS heat index ([equations from NOAA's Weather Prediction Center](https://www.wpc.ncep.noaa.gov/html/heatindex_equation.shtml)), which accounts for humidity. The formulas use temperature $T$ in °F and relative humidity $RH$ in %, and the result is converted to °C for display.

1. Simple estimate (Steadman). If it is below 80 °F, this is the final answer:

$$HI = 0.5\left[T + 61 + 1.2\,(T - 68) + 0.094\,RH\right]$$

2. Otherwise, a full regression (Rothfusz) is applied:

$$
\begin{aligned}
HI ={}& -42.379 + 2.04901523\,T + 10.14333127\,RH - 0.22475541\,T\,RH \\
& - 0.00683783\,T^2 - 0.05481717\,RH^2 + 0.00122874\,T^2 RH \\
& + 0.00085282\,T\,RH^2 - 0.00000199\,T^2 RH^2
\end{aligned}
$$

3. Apply an adjustment at the extremes, where the regression drifts:

| Condition | Adjustment added to $HI$ |
|---|---|
| $RH < 13$ and $80 \le T \le 112$ | $-\dfrac{13 - RH}{4}\sqrt{\dfrac{17 - \lvert T - 95 \rvert}{17}}$ |
| $RH > 85$ and $80 \le T \le 87$ | $+\dfrac{RH - 85}{10}\cdot\dfrac{87 - T}{5}$ |

Finally, $HI_{\,°C} = (HI - 32) \times \tfrac{5}{9}$.

**Temperature indicator.** The RGB LED follows the feels-like value.

| Feels like | Color |
|---|---|
| below 20 C | Blue |
| 20 to 22 C | Purple |
| 22 to 25 C | Green |
| 25 to 27 C | Orange |
| 27 C and above | Red |

**Night mode.** The light sensor voltage output rises as ambient light decreases. Night mode activates when the 14-bit sensor reading exceeds 5700 and deactivates when it falls below 5300. A deadband/hysteresis gap is implemented to prevent rapid switching/flickering when the light level hovers near the switching point. The UNO R4 LED matrix stays on when night mode is active.

**Bluetooth.** The device advertises itself as `RP-UNO_R4_Sensor` via Bluetooth Low Energy (BLE) and sends IAQ, temperature, humidity, and pressure every 2 s while connected. Values are 32-bit floats, so choose the **float32** format in a BLE app such as nRF Connect.

## Tuning

Change these constants at the top of the sketch:

| Constant | Default | Effect |
|---|---|---|
| `lightThreshold` | 5500 | Day/night switching point |
| `hysteresisBuffer` | 200 | Width of the no-flicker band |
| `displayInterval` | 2000 ms | Display and Bluetooth update rate |
| `toggleInterval` | 4000 ms | Matrix swaps between ambient and feels-like |

## Known limitations

- Calibration restarts every time power is lost.
- The estimated CO2 value is calculated but not yet displayed.
- Generic BLE apps may misread the values, since the data format differs from the Bluetooth standard.

## License

This project is licensed under the terms of the MIT License.
