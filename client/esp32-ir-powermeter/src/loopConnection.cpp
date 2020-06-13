#include <Arduino.h>
#include <WiFi.h>
#include "clients.h"
#include "config.h"

void ensureConnected()
{
  if (WiFi.status() != WL_CONNECTED)
  {
    Serial.println("Not Connected to WiFi! Rebooting!");
    delay(1000);
    ESP.restart();
  }
  else if (!mqttClient.connected())
  {
    Serial.println("Connecting to Mqtt...");
    if (mqttClient.connect("ir-powermeter-" METER_NAME, MQTT_USER, MQTT_PASSWORD, "ir-powermeter/" METER_NAME "/dead", MQTTQOS0, false, "pwease weconnect me UwU"))
    {
      mqttClient.publish("ir-powermeter/" METER_NAME "/alive", "I'm alive!", false);
      Serial.println("connected");
    }
    else
    {
      Serial.print("failed, rc=");
      Serial.print(mqttClient.state());
      delay(1000);
    }
  }
}

void loopConnection(void *param)
{
  while (true)
  {
    ensureConnected();
    mqttClient.loop();
    delay(1000);
  }
}