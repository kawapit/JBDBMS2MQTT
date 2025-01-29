#ifndef __MQTT_H__
#define __MQTT_H__

#include <Arduino.h>
#include <ArduinoJson.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ESPDateTime.h>

#include "Setup.h"
#include "JbdBms.h"

// variables in main.cpp
extern bool restart;				// restart required
extern bool saveConfig;				// save settings required
extern bool relayState;
extern bool relay;

extern ulong restartTime;
extern bool chargeState, dischargeState;
extern unsigned int refresh;		// main.cpp

extern void wifiScan();
extern void wifiConnect();
extern bool Wakeup(bool wakeup);
extern bool processRelay();

extern String ErrorString();		// bms error text
extern String htmlProcessor(const String &var); // for debug only (Webserver.cpp)
extern String jsonRequest();

// Setup and Bms variables
extern Setup Settings;				// Settings point to Setup instance
extern JbdBms Bms;					// Bms is a pointer to JbdBms class
// because debugLevel is a static variable in JbdBms instance and Mqtt is the 2nd included file
// it must be addresses with instance
//extern byte JbdBms::debugLevel;
//extern AsyncWebSocketClient wsClient;
//extern PubSubClient mqttClient;

void setupMQTTClient();
void processMQTT();
void mqttPublishRelay();
void getJsonDevice(), getJsonData();

#endif /* __MQTT_H__ */
