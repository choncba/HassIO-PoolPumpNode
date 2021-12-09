/*
  Nodo para la bomba de agua de la pileta
  Conectado a Home Assistant
  Hardware:
          - ESP8266 ESP01 con 512Kb
          - 1 entradas de Pulsador
          - 1 Salidas a Triac
*/

#include <Arduino.h>
#include <NTPClient.h>
#include <ESP8266WiFi.h>          // https://github.com/esp8266/Arduino
#include <PubSubClient.h>         // https://github.com/knolleary/pubsubclient
#include <WiFiUdp.h>
#include "config.h"

#if defined(DEBUG_TELNET)
WiFiServer  telnetServer(23);
WiFiClient  telnetClient;
#define     DEBUG_PRINT(x)    telnetClient.print(x)
#define     DEBUG_PRINTLN(x)  telnetClient.println(x)
#elif defined(DEBUG_SERIAL)
#define     DEBUG_PRINT(x)    Serial.print(x)
#define     DEBUG_PRINTLN(x)  Serial.println(x)
#else
#define     DEBUG_PRINT(x)
#define     DEBUG_PRINTLN(x)
#endif

WiFiClient    wifiClient;
PubSubClient  mqttClient(wifiClient);

#ifdef USE_NTP
WiFiUDP ntpUDP;
//NTPClient timeClient(ntpUDP);
NTPClient timeClient(ntpUDP, NTP_SERVER, 3600, 60000);
#endif

void PublicarLuz(void);
void PublicarTecla(void);

const char Tecla_txt[14] = "tecla_cochera";
const char Luz_txt [12] = "luz_cochera";

struct Status{
boolean Luz = 0;
uint8_t Tecla = 1;
}nodo;

#pragma region Debug TELNET
///////////////////////////////////////////////////////////////////////////
//   TELNET
///////////////////////////////////////////////////////////////////////////
/*
   Function called to handle Telnet clients
   https://www.youtube.com/watch?v=j9yW10OcahI
*/
#if defined(DEBUG_TELNET)
void handleTelnet(void) {
  if (telnetServer.hasClient()) {
    if (!telnetClient || !telnetClient.connected()) {
      if (telnetClient) {
        telnetClient.stop();
      }
      telnetClient = telnetServer.available();
    } else {
      telnetServer.available().stop();
    }
  }
}
#endif
#pragma endregion

#pragma region Funciones WiFi
///////////////////////////////////////////////////////////////////////////
//   WiFi
///////////////////////////////////////////////////////////////////////////

void handleWiFiEvent(WiFiEvent_t event) {
  switch (event) {
    case WIFI_EVENT_STAMODE_GOT_IP:
      DEBUG_PRINTLN(F("INFO: WiFi connected"));
      DEBUG_PRINT(F("INFO: IP address: "));
      DEBUG_PRINTLN(WiFi.localIP());
      break;
    case WIFI_EVENT_STAMODE_DISCONNECTED:
      DEBUG_PRINTLN(F("ERROR: WiFi losts connection"));
      /*
         TODO: Do something smarter than rebooting the device
      */
      delay(1000);
      WiFi.reconnect();
      //ESP.restart();
      break;
    default:
      DEBUG_PRINT(F("INFO: WiFi event: "));
      DEBUG_PRINTLN(event);
      break;
  }
}

void setupWiFi() {
  DEBUG_PRINT(F("INFO: WiFi connecting to: "));
  DEBUG_PRINTLN(WIFI_SSID);

  delay(10);

  WiFi.mode(WIFI_STA);
  WiFi.onEvent(handleWiFiEvent);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
#ifndef DHCP
    WiFi.config(NODE_IP, NODE_GW, NODE_MASK);
#endif

  randomSeed(micros());
}
#pragma endregion

#pragma region Funciones MQTT
///////////////////////////////////////////////////////////////////////////
//   MQTT
///////////////////////////////////////////////////////////////////////////
String StringTopic = "";
char CharTopic[100];
String StringData = "";
char CharData[10];

volatile unsigned long lastMQTTConnection = TIMEOUT;
/*
   Function called when a MQTT message has arrived
   @param p_topic   The topic of the MQTT message
   @param p_payload The payload of the MQTT message
   @param p_length  The length of the payload
*/
void handleMQTTMessage(char* p_topic, byte* p_payload, unsigned int p_length) {
  // concatenates the payload into a string
  uint8_t i = 0;
  String topic = String(p_topic);
  // for (i = 0; i < p_length; i++) {
  //   topic.concat((char)p_topic[i]);
  // }

  String payload;
  for (i = 0; i < p_length; i++) {
    payload.concat((char)p_payload[i]);
  }
  
  DEBUG_PRINTLN(F("INFO: New MQTT message received"));
  DEBUG_PRINT(F("INFO: MQTT topic: "));
  DEBUG_PRINTLN(topic);
  DEBUG_PRINT(F("INFO: MQTT payload: "));
  DEBUG_PRINTLN(payload);
  
  boolean set_value = false;

  if (topic.indexOf(Luz_txt)>0)
  {
    DEBUG_PRINT(F("FOUND: "));
    DEBUG_PRINTLN(Luz_txt);
    if(payload.equals(ON))
    {
      set_value = true;
      DEBUG_PRINTLN(F("PAYLOAD: ON"));
    }  
    if(payload.equals(OFF))
    {
      set_value = false;
      DEBUG_PRINTLN(F("PAYLOAD: OFF"));
    } 
    
    if(set_value != nodo.Luz)
    {
      nodo.Luz = set_value;
      digitalWrite(SALIDA1, nodo.Luz);
      DEBUG_PRINT(Luz_txt);
      DEBUG_PRINT(F(" TURNED: "));
      DEBUG_PRINTLN((nodo.Luz)?"ON":"OFF");
    }
    PublicarLuz();
  }
}

/*
  Function called to subscribe to a MQTT topic
*/
void subscribeToMQTT(char* p_topic) {
  if (mqttClient.subscribe(p_topic)) {
    DEBUG_PRINT(F("INFO: Sending the MQTT subscribe succeeded for topic: "));
    DEBUG_PRINTLN(p_topic);
  } else {
    DEBUG_PRINT(F("ERROR: Sending the MQTT subscribe failed for topic: "));
    DEBUG_PRINTLN(p_topic);
  }
}

/*
  Function called to publish to a MQTT topic with the given payload
*/
void publishToMQTT(char* p_topic, char* p_payload) {
  if (mqttClient.publish(p_topic, p_payload, RETAIN)) {
    DEBUG_PRINT(F("INFO: MQTT message published successfully, topic: "));
    DEBUG_PRINT(p_topic);
    DEBUG_PRINT(F(", payload: "));
    DEBUG_PRINTLN(p_payload);
  } else {
    DEBUG_PRINTLN(F("ERROR: MQTT message not published, either connection lost, or message too large. Topic: "));
    DEBUG_PRINT(p_topic);
    DEBUG_PRINT(F(" , payload: "));
    DEBUG_PRINTLN(p_payload);
  }
}

/*
  Function called to connect/reconnect to the MQTT broker
*/
void connectToMQTT()
{
  if (!mqttClient.connected())
  {
    if (lastMQTTConnection + TIMEOUT < millis())
    {
      StringTopic = String(BASE_TOPIC) + String(STATUS_TOPIC);
      StringTopic.toCharArray(CharTopic, sizeof(CharTopic));
      if (mqttClient.connect(MQTT_CLIENT_ID, MQTT_USERNAME, MQTT_PASSWORD, CharTopic, QoS, RETAIN, MQTT_DISCONNECTED_STATUS))
      {
        DEBUG_PRINTLN(F("INFO: The client is successfully connected to the MQTT broker"));
        publishToMQTT(CharTopic, MQTT_CONNECTED_STATUS);

        PublicarLuz();
        PublicarTecla();

        StringTopic = String(BASE_TOPIC) + "/" + String(Luz_txt) + String(SET_TOPIC);
        StringTopic.toCharArray(CharTopic, sizeof(CharTopic));
        subscribeToMQTT(CharTopic);
      } 
      else
      {
        DEBUG_PRINTLN(F("ERROR: The connection to the MQTT broker failed"));
        DEBUG_PRINT(F("INFO: MQTT username: "));
        DEBUG_PRINTLN(MQTT_USERNAME);
        DEBUG_PRINT(F("INFO: MQTT password: "));
        DEBUG_PRINTLN(MQTT_PASSWORD);
        DEBUG_PRINT(F("INFO: MQTT broker: "));
        DEBUG_PRINTLN(MQTT_SERVER);
      }
      lastMQTTConnection = millis();
    }
  }
}
#pragma endregion

#pragma region Publicaciones MQTT
void PublicarTecla()
{
  StringTopic = String(BASE_TOPIC) + "/" + String(Tecla_txt) + String(STATUS_TOPIC);
  StringTopic.toCharArray(CharTopic, sizeof(CharTopic));
  StringData = String((nodo.Tecla)?ON:OFF);
  StringData.toCharArray(CharData, sizeof(CharData));
  publishToMQTT(CharTopic, CharData);
}

void PublicarLuz()
{
  StringTopic = String(BASE_TOPIC) + "/" + String(Luz_txt) + String(STATUS_TOPIC);
  StringTopic.toCharArray(CharTopic, sizeof(CharTopic));
  StringData = String((nodo.Luz)?ON:OFF);
  StringData.toCharArray(CharData, sizeof(CharData));
  publishToMQTT(CharTopic, CharData);
}

#pragma endregion

#pragma region Lectura de Teclas y sensor

void CheckTeclas(){
  static uint8_t RoundCheck = 0;
  const uint8_t RoundCheckThreshole = 8;

  uint8_t curStatus = digitalRead(PULSADOR1);
  if (nodo.Tecla != curStatus)
  {
    delay(5);
    curStatus = digitalRead(PULSADOR1);
    if (nodo.Tecla != curStatus)
    {
      RoundCheck++;
    }
    else  RoundCheck = 0;

    if (RoundCheck >= RoundCheckThreshole)
    {
      if(!nodo.Tecla&curStatus) // Si la tecla pasa de 0 a 1
      {
        nodo.Luz^=1;            // Invierto la salida 
        digitalWrite(SALIDA1, nodo.Luz);  // Activo/Desactivo Salida
        PublicarLuz();
      }
      nodo.Tecla = curStatus;
      RoundCheck = 0;
      
      PublicarTecla();
    }
  }

}

#pragma endregion

///////////////////////////////////////////////////////////////////////////
//  SETUP() AND LOOP()
///////////////////////////////////////////////////////////////////////////

void setup()
{
  pinMode(PULSADOR1, INPUT);
  pinMode(SALIDA1, OUTPUT);

#if defined(DEBUG_SERIAL)
  Serial.begin(115200);
#elif defined(DEBUG_TELNET)
  telnetServer.begin();
  telnetServer.setNoDelay(true);
#endif

  setupWiFi();

  mqttClient.setServer(MQTT_SERVER, MQTT_SERVER_PORT);
  mqttClient.setCallback(handleMQTTMessage);

  connectToMQTT();
  #ifdef USE_NTP
  timeClient.begin();
  #endif

  digitalWrite(SALIDA1, LOW);

}

void loop()
{
#if defined(DEBUG_TELNET)
  handleTelnet();
  yield();
#endif

  connectToMQTT();
  mqttClient.loop();
  yield();

  CheckTeclas();
  yield();

#ifdef USE_NTP
  timeClient.update();
  Serial.println(timeClient.getFormattedTime());
#endif

}