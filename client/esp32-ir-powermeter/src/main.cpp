#include <Arduino.h>
#include <WiFi.h>

//doesn't work for some reason, overwrite this in pubsubclient itself instead
#define MQTT_MAX_PACKET_SIZE 2048
#include <PubSubClient.h>
#include "config.h"
#include "loopConnection.h"
#include "loopMeter.h"
#include "loopMetaData.h"

WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);

void setup()
{
  Serial.begin(115200);
  Serial2.begin(300, SERIAL_7E1, 16, 17, true, 1000);
  Serial2.setRxBufferSize(1024);

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.println("Connecting to WiFi..");
  }

  mqttClient.setServer(MQTT_SERVER, 1883);

  xTaskCreate(
      loopConnection,
      "Connection",
      10000,           /* Stack size */
      NULL,            /* Parameter */
      1,               /* Priority */
      NULL);           /* Task handle. */
  xTaskCreate(
      loopMeter,
      "Meter",
      10000,           /* Stack size */
      NULL,            /* Parameter */
      1,               /* Priority */
      NULL);           /* Task handle. */
  xTaskCreate(
      loopMetaData,
      "SendMetaData",
      10000,           /* Stack size */
      NULL,            /* Parameter */
      1,               /* Priority */
      NULL);           /* Task handle. */
}

void loop(){
  delay(1000);
}
