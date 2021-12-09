///////////////////////////////////////////////////////////////////////////
//   PINS
///////////////////////////////////////////////////////////////////////////
#include "secrets.h"
#define PULSADOR1   D3  // GPIO0 - Tecla Bomba    
#define SALIDA1     D4  // GPIO1 - Salida a Bomba  

///////////////////////////////////////////////////////////////////////////
//   WiFi
///////////////////////////////////////////////////////////////////////////
#define WIFI_SSID       WIFI_SECRET_SSID
#define WIFI_PASSWORD   WIFI_SECRET_PASS
//#define DHCP            // uncomment for use DHCP
#ifndef DHCP
IPAddress NODE_IP(192,168,1,55);
IPAddress NODE_GW(192,168,1,1);
IPAddress NODE_MASK(255,255,255,0);
#endif
///////////////////////////////////////////////////////////////////////////
//   MQTT
///////////////////////////////////////////////////////////////////////////
#define MQTT_CLIENT_ID    "NodoBomba"
#define MQTT_USERNAME     MQTT_SECRET_USERNAME
#define MQTT_PASSWORD     MQTT_SECRET_PASSWORD
// #define MQTT_SERVER       "iotdevar.duckdns.org"
#define MQTT_SERVER       "192.168.1.3"
#define MQTT_SERVER_PORT  1883

#define BASE_TOPIC "/" MQTT_CLIENT_ID
#define SET_TOPIC "/set"
#define STATUS_TOPIC "/status"
#define MQTT_CONNECTED_STATUS "online"
#define MQTT_DISCONNECTED_STATUS "offline"

#define ON   "1"
#define OFF  "0"

#define TIMEOUT 5000

#define RETAIN true
#define QoS     0

///////// NTP ////////////
#define USE_NTP 
#define NTP_SERVER "pool.ntp.org"

///////////////////////////////////////////////////////////////////////////
//   DEBUG
///////////////////////////////////////////////////////////////////////////
#define DEBUG_TELNET
//#define DEBUG_SERIAL
