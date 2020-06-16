# esp32-ir-powermeter
ESP32 project to read power usage from a digital power meter via the infrared interface.
This was developed for a Landis+Gyr E350 power meter, but might work with similar power meters.

## Hardware
Hardware is just a ESP32 with an IR receiver hooked up to pin 16 (with a pullup resistor) and an IR LED hooked up to pin 17 of the ESP32.

## Transfer
Data is transferred via an MQTT broker. The node script under `server_influxdb` takes the received data, converts it into usable form and writes it to an InfluxDB database.
