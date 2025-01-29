/*
JbdBMS2MQTT Project
https://github.com/yash-it/JbdBMS2MQTT
*/
#ifndef __SETUP_CPP__
#define __SETUP_CPP__

#include "Setup.h"

#ifndef EEPROM_SETTINGS
const IPAddress local_IP(192, 168, 6, 145);	// static IP address setup
const IPAddress gateway(192, 168, 6, 250);
const IPAddress DNS(192, 168, 6, 254);
const IPAddress subnet(255, 255, 255, 0);

const char* ntpServer = NTP_SERVER;
const long  gmtOffset_sec = 3600;

const char* BMSname1 = "BSJBD202210-24-000"; 
const char* BMSname2 = "BSJBD202308-01-104";
 
char mqtt_topic_root[13] = "/ESP32-BMS0";
const char* mqtt_topic_stat = "/stat/";
const char* mqtt_topic_tele = "/tele/";
const char* mqtt_topic_cmnd = "/cmnd/";
 #else
//NOTE 1: if you want to change what is pushed via MQTT - edit function: pushBatteryDataToMqtt
//NOTE 2: MQTT_TOPIC_STAT is where battery will push MQTT topics. For example "soc" will be pushed to: "/ESP32-BMS/grid_battery/soc"
 #endif
//String topic = "";						// default first part of topic. We will add device ID in setup
//String devicePrefix = "/ESP32_BMS0/";		// prefix for datapath for every device

extern bool relayState;		//main.cpp

//class Setup // create instance with: Setup Settings;
//{
  // change eeprom config version ONLY when new parameter is added and need reset the parameter
  unsigned int configVersion = 11;

// public
dataStruct Setup::data;		// setup data storage
relayStruct Setup::relay;	// relay data storage

void Setup::load()
{
  Serial.println(F("reading eeprom settings..."));
  data = {};		// clear befor load data
  EEPROM.begin(EEPROM_SIZE);
  EEPROM.get(0, data);
  EEPROM.end();
  checkConfig();
  check();
}

void Setup::save()
{
  Serial.println(F("writing eeprom settings..."));
  check();
  EEPROM.begin(EEPROM_SIZE);
  EEPROM.put(0, data);
  EEPROM.commit();
  EEPROM.end();
}

void Setup::reset()
{
  data = {};
  save();
}

//  ParseIPv4(&Settings->ipv4_address[0], PSTR(WIFI_IP_ADDRESS));
bool Setup::ParseIPv4(uint32_t* addr, const char* str_p)
{
  uint8_t *part = (uint8_t*)addr;
  uint8_t i;
  char str_r[strlen_P(str_p) + 1];
  char *str = &str_r[0];
  strcpy_P(str, str_p);

  *addr = 0;
  for (i = 0; i < 4; i++) {
	part[i] = strtoul(str, nullptr, 10);	// convert byte
	str = strchr(str, '.');
	if (str == nullptr || *str == '\0') {
	  break;								// no more separators, exit
	}
	str++;									// point to next character after separator
  }
  return (3 == i);
}


void Setup::print()
{
  Serial.print(">> Settings <<\n");
  Serial.print("HTTP Server: "); Serial.print(IP_ADDRESS); Serial.print(':'); Serial.print(data.httpPort);
  Serial.print(" user: "); Serial.print(data.httpUser); Serial.print('/'); Serial.println(data.httpPass);
  Serial.print("MQTT Server: "); Serial.print(data.mqttServer); Serial.print(':'); Serial.print(data.mqttPort);
  Serial.print(" user: "); Serial.print(data.mqttUser); Serial.print('/'); Serial.println(data.mqttPass);
  Serial.print("Device: "); Serial.print(data.deviceName); Serial.printf(" %d.%d\n", int(data.config / 10), data.config % 10);
  Serial.print("Topic: "); Serial.print(data.mqttTopic);
  Serial.print("  Trigger: "); Serial.println(data.mqttTrigger);
  Serial.print("BMS Name: "); Serial.println(data.bmsName);  
  Serial.print("MQTT/BMS Refresh: "); Serial.print(data.mqttRefresh);
  Serial.print("s/"); Serial.print(data.bmsRefresh); Serial.println('s');
  Serial.print("Sleep/Wakeup Voltages: "); Serial.print(data.sleepVoltage / 1000); Serial.print("V/");
	Serial.print(data.wakeupVoltage / 1000); Serial.print("V  ");
  Serial.print("SoC min/max: "); Serial.print(data.SoCmin); Serial.print("%/"); Serial.print(data.SoCmax); Serial.println('%');
  Serial.print("Relay: "); Serial.print((relay.enable) ? "enabled":"disabled"); Serial.print((relay.invert) ? ", inverted":"");
  Serial.printf(" Mode: %u", relay.function); Serial.printf(" State: o"); Serial.println((relayState) ? "n":"ff");
  Serial.print("Debug-Level: "); Serial.print(JbdBms::debugLevel, HEX); Serial.print(" "); Serial.println(data.testVal);
}


void Setup::check() // check sanity of the variables from eeprom
{
  if (data.deviceName[0] == 0 || strlen(data.deviceName) >= 20)	strcpy(data.deviceName, HOST_NAME);
  if (data.ntpServer[0] == 0 || strlen(data.ntpServer) >= 20)		strcpy(data.ntpServer, NTP_SERVER);
  if (data.mqttServer[0] == 0 || strlen(data.mqttServer) >= 20)	strcpy(data.mqttServer, MQTT_SERVER);
  if (data.mqttUser[0] == 0 || strlen(data.mqttUser) >= 20)		strcpy(data.mqttUser, MQTT_USER);
  if (data.mqttPass[0] == 0 || strlen(data.mqttPass) >= 20)		strcpy(data.mqttPass, MQTT_PASSWORD);
  if (data.mqttTopic[0] == 0 || strlen(data.mqttTopic) >= 20)		strcpy(data.mqttTopic, MQTT_TOPIC);
  if (data.mqttPort < 1024 || data.mqttPort >= 65530)				data.mqttPort = MQTT_PORT;
  if (data.mqttRefresh <= 1 || data.mqttRefresh >= 65530)			data.mqttRefresh = MQTT_REFRESH;
  if (data.mqttJson && !data.mqttJson) data.mqttJson = false;
  if (data.mqttTrigger[0] == 0 || strlen(data.mqttTrigger) >= 80)	strcpy(data.mqttTrigger, "");
  if (data.httpPort < 80 || data.httpPort >= 65530)				data.httpPort = HTTP_PORT;
  if (data.httpUser[0] == 0 || strlen(data.httpUser) >= 20)		strcpy(data.httpUser, HTTP_USER);
  if (data.httpPass[0] == 0 || strlen(data.httpPass) >= 20)		strcpy(data.httpPass, HTTP_PASSWORD);

  if (data.bmsName[0] == 0 || strlen(data.bmsName) >= 20)			strcpy(data.bmsName, BMS_NAME);
  	//float sleepVoltage = 26.4 * 1000; // 13.399 * 1000; // mV
  if (data.sleepVoltage < 12000 || data.sleepVoltage > 60000)		data.sleepVoltage = 3000 * NUMCELLS; // in mV
  if (data.wakeupVoltage < 12000 || data.wakeupVoltage > 60000)	data.wakeupVoltage = 3100 * NUMCELLS;
  if (data.wakeupEnable && !data.wakeupEnable)						data.wakeupEnable = false;
  if (data.SoCmin < 5 || data.SoCmin > 50)							data.SoCmin = 5;
  if (data.SoCmax < 50 || data.SoCmax > 100)						data.SoCmax = 95;
  if (data.bmsRefresh < 2 || data.bmsRefresh >= 65530)				data.bmsRefresh = BMS_REFRESH; // in s
  if (data.darkMode && !data.darkMode) data.darkMode = false;
  if (data.haDiscovery && !data.haDiscovery) data.haDiscovery = false;

  if (relay.enable && !relay.enable)				relay.enable = true;
  if (relay.invert && !relay.invert)				relay.invert = false;
  if (relay.failsafe && !relay.failsafe)			relay.failsafe = false;
  if (relay.function < 0 || relay.function > 10)	relay.function = 0;
  if (relay.compare < 0 || relay.compare > 1)		relay.compare = 0;
  if (relay.value < -100 || relay.value > 100)		relay.value = 0;
  if (relay.hysteresis < -100 || relay.hysteresis > 100) relay.hysteresis = 0;
}

// private
void Setup::checkConfig()
{
  if (data.config != configVersion) { // set default values
	data.config = configVersion;
	strcpy(data.deviceName, HOST_NAME);
	strcpy(data.ntpServer, NTP_SERVER);
	strcpy(data.mqttServer, MQTT_SERVER);
	strcpy(data.mqttUser, MQTT_USER);
	strcpy(data.mqttPass, MQTT_PASSWORD);
	strcpy(data.mqttTopic, MQTT_TOPIC);
	data.mqttPort = MQTT_PORT;
	data.mqttRefresh = MQTT_REFRESH;	// publish data to broker
	data.mqttJson = false;
	strcpy(data.mqttTrigger, MQTT_TRIGGER);
	data.httpPort = HTTP_PORT;
	strcpy(data.httpUser, HTTP_USER);
	strcpy(data.httpPass, HTTP_PASSWORD);
	strcpy(data.bmsName, BMS_NAME);
	data.sleepVoltage = 3000 * NUMCELLS;
	data.wakeupVoltage = 3200 * NUMCELLS;
	data.SoCmin = 5;
	data.SoCmax = 95;
	data.bmsRefresh = BMS_REFRESH;		// request data from bms
	data.wakeupEnable = false;
	relay.enable = true;
	relay.invert = false;
	relay.failsafe = false;
	relay.function = 0;
	relay.compare = 0;
	relay.value = 0.0;
	relay.hysteresis = 0.0;
	data.darkMode = false;
	data.haDiscovery = false;
	save();
	load();
  }
}

#endif // End __SETUP_H__
