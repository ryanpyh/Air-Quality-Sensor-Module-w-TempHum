This is the code for a module to monitor the air quality (VOC, CO2) in a space, as well as the local ambient temperature, humidity, atmospheric pressure.

A BME680 module is used to monitor the above metric, which are routed to a 0.96" OLED display via I2C protocol, which also shows the calibration status of the BME680 module. An RGB LED is also installed as a temperature indicator, which changes color based on which range the current temperature is in. This allows users to quickly determine the ambient temperature range without needing to read from the display.

A photodiode module is also implemented as a complementary sensor to detect the ambient light levels and toggle between active (temperature indicator and display on) mode and power-saving/night mode (temperature indicator and display off, white night light LEDs on).
