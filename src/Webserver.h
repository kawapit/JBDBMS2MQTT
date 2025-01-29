#ifndef __WEBSERVER_H__
#define __WEBSERVER_H__

#include <Arduino.h>
#include <WiFi.h>
//#include <LittleFS.h>
#include <ESPmDNS.h>
#include <AsyncTCP.h>				// new
#include <ESPAsyncWebServer.h>		// ESPAsyncWebServer.h before AsynJson.h!!!
#include <SPIFFS.h>
#include <AsyncJson.h>
#include <ArduinoJson.h>
//#include <SPIFFSEditor.h>
#include <Update.h>
#include <PubSubClient.h>

#include "Setup.h"
#include "JbdBms.h"
//#include "Html.h"
#define LittleFS SPIFFS				// if using LittleFS.h

// variables in main.cpp:
extern bool restart;				// restart required
extern bool saveConfig;				// save settings required
extern bool haDiscTrigger;
extern ulong restartTime;
extern bool relay;
extern byte error;
extern bool chargeState, dischargeState, balance;
extern bool Wakeup(bool wakeup);
extern unsigned int refresh;		// mqtt refresh (main.cpp)
// Setup and Bms variables and functions:
extern Setup Settings;				// Settings point to Setup instance
extern JbdBms Bms;					// Bms is a pointer to JbdBms instance
extern PubSubClient mqttClient;
extern char topicBuffer[];			// for mqtt publishing
extern char *fullTopic(char *bufPtr, char const *prefix); // (Mqtt.cpp)
#define JSON_BUFFER 2048
extern StaticJsonDocument<JSON_BUFFER> bmsJson;
extern void getJsonDevice();
extern void getJsonData();
extern void getJsonRelay();
// funktion prototypes:
void setupOTA();
void setupWebserver();
void processWebserver();
void cleanupClients();				// linker error (Webserver.cpp)
//AsyncWebSocket socket();
String jsonRequest();
String htmlProcessor(const String &var);
String fileSize(const size_t bytes);
String directoryString(bool ishtml);

#endif /* __WEBSERVER_H__ */
