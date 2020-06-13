#include <Arduino.h>
#include "clients.h"
#include "config.h"

enum ReadState
{
  STATE_INIT,
  STATE_START_READ,
  STATE_READING,
  STATE_WAIT
};

enum ReadState currentState = STATE_INIT;

bool publishMeterData(String s, unsigned long measurementTimeMs)
{
  if (wifiClient.connected() && mqttClient.connected())
  {
    char buffer[50];
    sprintf(buffer, "%lu", measurementTimeMs);
    mqttClient.publish("ir-powermeter/" METER_NAME "/measurement_time_ms", buffer, false);
    return mqttClient.publish("ir-powermeter/" METER_NAME "/data", s.c_str(), false);
  }
  return false;
}

String messageBuffer = "";

unsigned long measurementStart = 0;
unsigned long measurementTime = 0;

void loopMeter(void *param)
{
  while (true)
  {
    switch (currentState)
    {
    case STATE_INIT:
    {
      measurementStart = millis();
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
      measurementTime = millis() - measurementStart;
      if (!publishMeterData(messageBuffer, measurementTime))
      {
        Serial.println("publish failed!");
      }
      currentState = STATE_WAIT;
      break;
    }
    case STATE_WAIT:
    {
      unsigned long measurementAndTransmitTime = millis() - measurementStart;
      if (measurementAndTransmitTime <= 15000)
        delay(15000 - measurementAndTransmitTime);
      currentState = STATE_INIT;
      break;
    }
    }
  }
}