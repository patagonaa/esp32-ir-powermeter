#include <Arduino.h>
#include <WiFi.h>

//doesn't work for some reason, overwrite this in pubsubclient itself instead
#define MQTT_MAX_PACKET_SIZE 2048
#include <PubSubClient.h>
#include "credentials.h"

#define METER_NAME "esp32-01"

enum ReadState
{
  STATE_INIT,
  STATE_START_READ,
  STATE_READING,
  STATE_IDLE
};

enum ReadState currentState = STATE_INIT;

WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);

void mqtt_keepalive(void *param)
{
  while (true)
  {
    mqttClient.loop();
    delay(1000);
  }
}

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
      mqtt_keepalive,   /* Task function. */
      "MqttKeepAlive", /* String with name of task. */
      10000,     /* Stack size in bytes. */
      NULL,      /* Parameter passed as input of the task */
      1,         /* Priority of the task. */
      NULL);     /* Task handle. */
}

void ensureConnected()
{
  if (WiFi.status() != WL_CONNECTED)
  {
    Serial.println("Not Connected to WiFi");
  }
  else if (!mqttClient.connected())
  {
    Serial.println("Connecting to Mqtt...");
    if (mqttClient.connect("ir-powermeter-" METER_NAME, MQTT_USER, MQTT_PASSWORD, "ir-powermeter/" METER_NAME "/dead", MQTTQOS0, false, "pwease weconnect me UwU"))
    {
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

bool publishMeterData(String s)
{
  if (wifiClient.connected() && mqttClient.connected())
  {
    return mqttClient.publish("ir-powermeter/" METER_NAME "/data", s.c_str(), false);
  }
  return false;
}

String messageBuffer = "";

void loop()
{
  ensureConnected();

  switch (currentState)
  {
  case STATE_INIT:
  {
    Serial2.print("/?!\r\n");
    Serial.println("Initializing");
    delay(500);
    String s = Serial2.readString();
    if (s.startsWith("/LGZ"))
    {
      Serial.println("Initialized " + s);
      currentState = STATE_START_READ;
      //delay(2000);
    }
    else
    {
      Serial.println("Invalid Init Response:");
      Serial.println(s);
      delay(5000);
    }

    break;
  }
  case STATE_START_READ:
  {
    while (Serial2.read() != -1)
    { // ignore rest of buffer
    }
    Serial2.print("\x06"
                  "000\r\n");
    Serial.println("START_READ");
    unsigned long startTime = millis();
    while (millis() - startTime < 10000)
    {
      int readChar = Serial2.peek();
      if (readChar != -1)
      {
        if (readChar == '\x02')
        {
          Serial2.read();
        }
        Serial.println("Read Started!");
        goto read_start_done;
      }
      else
      {
        yield();
      }
    }
    //timeout
    Serial.println("START_READ Timeout");
    currentState = STATE_INIT;
    break;

  read_start_done:
    messageBuffer = "";
    currentState = STATE_READING;
    break;
  }
  case STATE_READING:
  {
    Serial.println("READING");
    unsigned long lastCharTime = millis();
    int chr;
    while ((millis() - lastCharTime) < 5000)
    {
      chr = Serial2.read();
      if (chr == '\x03') // ETX
      {
        goto read_done;
      }
      if (chr != -1)
      {
        messageBuffer += (char)chr;
        lastCharTime = millis();
      }
    }
    //timeout
    currentState = STATE_START_READ;
    Serial.println("READING Timeout");
    break;
  read_done:
    while (Serial2.read() != -1)
    { // ignore rest of buffer
    }
    Serial.println("Read Data:");
    Serial.println(messageBuffer);
    if (!publishMeterData(messageBuffer))
    {
      Serial.println("publish failed!");
    }
    currentState = STATE_IDLE;
    break;
  }
  case STATE_IDLE:
  {
    delay(30000);
    currentState = STATE_INIT;
    break;
  }
  }
}