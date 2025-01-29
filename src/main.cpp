/*
-----------
BMS routines inspired by repositories from Miroslav Kolinsky 2019  https://www.kolins.cz
	https://github.com/kolins-cz/Smart-BMS-Bluetooth-ESP32
who was heavily inspired by
	https://github.com/bres55/Smart-BMS-arduino-Reader
thanks to Petr Jenik for big parts of code
thanks to Milan Petrzilka
---
webserver and -pages, websockets heavily inspired by repositories from:
	https://github.com/softwarecrash
The design corresponds most closely to that of tasmota and is very professional.
Also some other ideas like flash config management, relay addon, led error blinking
In this code the webpages are not stored in flash but in SPIFFS to simplify updates
and templates are copied into memory.

Life webpage inspired by a chinese guy
	https://github.com/kolins-cz/Smart-BMS-Bluetooth-ESP32
and customized it for WebUI.

SPIFFS code based on examples from:
	https://github.com/har-in-air/ESP32_ASYNC_WEB_SERVER_SPIFFS_OTA
which is a mashup of functionality from the following repositories:
	https://github.com/smford/esp32-asyncwebserver-fileupload-example
	https://randomnerdtutorials.com/esp32-web-server-spiffs-spi-flash-file-system/



The repo is PlatformIO Project form.

Partitions:
# Name,   Type, SubType, Offset,  Size, Flags
nvs,      data, nvs,     0x9000,  0x5000,	0xDFFF
otadata,  data, ota,     0xE000,  0x2000,	0xFFFF
app0,     app,  ota_0,   0x10000, 0x1E0000,	0x1EFFFF 1920kB
app1,     app,  ota_1,   0x1F0000,0x1E0000,	0x3CFFFF
spiffs,   data, spiffs,  0x3D0000,0x30000,	0x3FFFFF 192kB
*/

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiMulti.h>
#include <LittleFS.h>
//#include <SPIFFS.h>			// SPI Flash File System
//#include <Update.h>
//#include <ESPmDNS.h>
//#include <AsyncTCP.h> //new
//#include <ESPAsyncWebServer.h>
//#include <AsyncJson.h>
//#include <ArduinoJson.h>
#include <ESPDateTime.h>
#define LittleFS SPIFFS			// if using LittleFS.h
#define CONFIG_LITTLEFS_FOR_IDF_3_2
#define ARDUINOJSON_USE_DOUBLE 0
#define ARDUINOJSON_USE_LONG_LONG 0

#include "Debug.h"
#include "Setup.h"
#include "JbdBms.h"

#define DEBUG
//#define ENABLE_SLEEP			// disable WiFi on low battery voltage
#define ENABLE_MQTT				// MQTT service
#define ENABLE_WEBSERVER
#define ENABLE_SOCKETS
//#define ENABLE_OTA			// over the air update
#define EEPROM_SETTINGS			// get settings from EEPROM

#include "Webserver.h"
#ifdef ENABLE_MQTT
#include "Mqtt.h"
#endif

/*
Debug Levels:
	0	no display
	1	display Basic, Cell and Hardware Data
	2	display also BMS packets and Basic, Cell and Hardware Data
	3	display MQTT Json sent and received Messages
	4	display Socket Messages
*/
// BMS/BLE Settings:
Setup Settings;			// Settings point to Setup instance
JbdBms Bms;				// Bms is a pointer to JbdBms instance

// global variables:
ulong restartTime = 0;				// delayed ESP restart
ulong lastWakeup = 0;
 // don't run immediately after boot, wait for first intervall
RTC_DATA_ATTR int bootcount = 0;
byte error = 1;
int wifiErr = 0;
bool restart = false;				// restart required, per mqtt, http or error
bool saveConfig = false;			// save settings required after change
bool workerCanRun = true;
//ADC_MODE(ADC_VCC); ???
bool relay = false;					// requested state of relay
bool relayState = false; 			// current state of relay, true = on, false = off
bool chargeState, dischargeState;	// current state of charge/discharge from bms
bool balance;

//void wifiScan();
//void wifiConnect();
bool Wakeup(bool wakeIt);
bool processRelay();
void processLED();
//-------------------------------------
//static const String TAG = "main";

WiFiMulti wifiMulti;		// WiFi Multi Instance

//const uint32_t connectTimeoutMs = 10000;

// local functions definitions
/*
ulong currentSec()
{
  static unsigned int wrapCnt = 0;
  static ulong lastVal = 0;
  ulong currentVal = millis();
  if (currentVal < lastVal) wrapCnt++;
  lastVal = currentVal;
  ulong seconds = currentVal / 1000;
  // millis will wrap each 50 days, as we are interested only in seconds, let's keep the wrap counter
  return (wrapCnt * 4294967) + seconds;
}
*/
uint32_t ChipId()
{
#ifdef ESP32
  uint32_t chipid = 0;
  for (int i = 0; i < 17; i = i + 8) {
	chipid |= ((ESP.getEfuseMac()>>(40 - i)) & 0xFF)<<i;
  }
  return chipid;
#else
  return ESP.getChipId();
#endif
}


void setupDateTime()
{
  Serial.print("setup time... ");
  // setup this after wifi connected, you can use custom timeZone, server and timeout
  DateTime.setTimeZone("TZ_Europe_Berlin"); // UTC+1
//DateTime.setServer(ntpServer);
  DateTime.setServer(Settings.data.ntpServer);
  DateTime.begin(15 * 1000); // timeout 15s
  if (DateTime.isTimeValid()) {
	DateTimeParts p = DateTime.getParts();
	Serial.printf(" %04d-%02d-%02d %02d:%02d:%02d\n",
		p.getYear(), p.getMonth() + 1, p.getMonthDay(), p.getHours(), p.getMinutes(), p.getSeconds());
//	Serial.printf("time: %ld\n", DateTime.now());
	Serial.println(F("time setup ok"));
  } else {
	Serial.println(F("failed to get time from NTP server"));
	error = 4;
  }
}


void wifiScan()
{
  Serial.print(F("scanning WiFi... "));
  int n = WiFi.scanNetworks();
  Serial.print(F("done, "));
  if (n == 0) {
	Serial.println(F("no networks found"));
  } else {
	Serial.print(n); Serial.println(F(" networks found:"));
	for (int i = 0; i < n; ++i) { // print SSID and RSSI for each network found
	  Serial.print(i + 1); Serial.print(": ");
	  Serial.print(WiFi.SSID(i));
	  Serial.print(F(" rssi: ")); Serial.print(WiFi.RSSI(i)); Serial.print(F(" type: "));
#if false
	  Serial.print((WiFi.encryptionType(i) == WIFI_AUTH_OPEN) ? " " : "*\n"); // short version
#else
	  switch (WiFi.encryptionType(i)) {
		case WIFI_AUTH_OPEN:
		  Serial.print("open\n");
		  break;
		case WIFI_AUTH_WEP:
		  Serial.print("WEP\n");
		  break;
		case WIFI_AUTH_WPA_PSK:
		  Serial.print("WPA\n");
		  break;
		case WIFI_AUTH_WPA2_PSK:
		  Serial.print("WPA2\n");
		  break;
		case WIFI_AUTH_WPA_WPA2_PSK:
		  Serial.print("WPA+WPA2\n");
		  break;
		case WIFI_AUTH_WPA2_ENTERPRISE:
		  Serial.print("WPA2-EAP\n");
		  break;
		case WIFI_AUTH_WPA3_PSK:
		  Serial.print("WPA3\n");
		  break;
		case WIFI_AUTH_WPA2_WPA3_PSK:
		  Serial.print("WPA2+WPA3\n");
		  break;
		case WIFI_AUTH_WAPI_PSK:
		  Serial.print("WAPI\n");
		  break;
		default:
		  Serial.print("unknown\n");
	  }
	}
#endif
	delay(10);
  }
  WiFi.scanDelete();		// delete the scan result to free memory
}


void wifiConnect()
{
  Serial.print(F("connecting WiFi..."));
//WiFi.persistent(true); // fix wifi save bug, writes data into flash!
  WiFi.hostname(Settings.data.deviceName);
  WiFi.mode(WIFI_STA);
//esp_wifi_set_ps(WIFI_PS_NONE);
  // if the connection to the stongest hotstop is lost, it will connect to the next network on the list
#if true
  if (wifiMulti.run(10*1000) == WL_CONNECTED) { // timeout 10s
#else
  // scan for Wi-Fi networks, and connect to the strongest of the networks above
  for (int i = 0; i < 40;i++) {
	delay(250);
	if (wifiMulti.status() != WL_CONNECTED) Serial.write('.');// wait for the Wi-Fi to connect
	else break;
  }
  if (wifiMulti.status() == WL_CONNECTED) {
#endif
	WiFi.persistent(false);
	WiFi.setAutoReconnect(true);
//	WiFi.setAutoReconnect = true;
	Serial.print(F(" connected to ")); Serial.print(WiFi.SSID());
	Serial.print(F("  rssi: ")); Serial.print(WiFi.RSSI());
	Serial.print(F("  IP: ")); Serial.println(WiFi.localIP());
	digitalWrite(WIFI_LED, HIGH);
  } else {
	Serial.println(F("not connected!"));
	digitalWrite(WIFI_LED, LOW);
  }
  WiFi.scanDelete();		// delete the scan result to free memory
}

//===================================== Setup

bool resetCounter(bool count)
{
  if (count) {
	esp_reset_reason_t reset_reason = esp_reset_reason();

	if (reset_reason == 6) {
	  if (bootcount >= 10 && bootcount < 20) {
//		bootcount = 0;
		Settings.reset();
//		ESP.eraseConfig();
//		ESP.reset();
	  } else {
		bootcount++;
	  }
	} else {
	  bootcount = 0;
	}
  } else {
	bootcount = 0;
  }
  return true;
}


void print_Setupdone(String text)
{
  Serial.print(text); Serial.println(F(" setup done!"));
}


void setup()
{
  Serial.begin(115200);						// standard hardware serial port
  Serial.printf("\n\nBinary compiled on %s at %s\n", __DATE__, __TIME__);

  pinMode(ERR_LED, OUTPUT);					// I/O setup
  digitalWrite(ERR_LED, LOW);
  pinMode(WIFI_LED, OUTPUT);				// onboard LED
  digitalWrite(WIFI_LED, LOW);
//pinMode(BLE_LED, OUTPUT);					// jbdBMS:setup()
//digitalWrite(BLE_LED, LOW);
//pinMode(MQTT_LED, OUTPUT);				// setupMQTTClient()
//digitalWrite(MQTT_LED, LOW);
//pinMode(ESP_LED, OUTPUT);
//digitalWrite(ESP_LED, LOW);
  pinMode(WAKEUP_PIN, OUTPUT);
  pinMode(RELAY_PIN, OUTPUT);

  resetCounter(true);
#ifdef EEPROM_SETTINGS
  Settings.load();
  Settings.print();
#endif
/*
  pinMode(Settings.data.wakeupPin, OUTPUT);
  pinMode(Settings.data.relayPin, OUTPUT);
  digitalWrite(Settings.data.relayPin, OFF);
  digitalWrite(Settings.data.wakeupPin, Settings.data.wakeupEnable);
*/
  // ### loading configuration from config file ###
  Serial.print(F("mounting SPIFFS... "));
  if (!SPIFFS.begin(true)) {
	Serial.println(F("failed!"));
	error = 5;
	return;
  } else {
	Serial.println(F("done!"));
	Serial.print(F("free: ")); Serial.println(fileSize((SPIFFS.totalBytes() - SPIFFS.usedBytes())));
	Serial.print(F("used: ")); Serial.println(fileSize(SPIFFS.usedBytes()));
	Serial.print(F("total: ")); Serial.println(fileSize(SPIFFS.totalBytes()));
	Serial.print(directoryString(false));
  }
  StaticJsonDocument<768> jsonConfig;			// allocate the JSON document (stack)
// size can be checked at https://arduinojson.org/v7/assistant/
// also read settings from json config file (744 bytes)
//String fileName = "/";
//fileName += name;
  Serial.println("reading config file: /" CONFIG_FILE);
  File fileHandle = SPIFFS.open("/" CONFIG_FILE, "r");
  if (fileHandle) {
	String jsonString = fileHandle.readStringUntil('\n');
	Serial.println(jsonString);
	fileHandle.close();
	// deserialize the JSON document
	DeserializationError error = deserializeJson(jsonConfig, jsonString);
	// test if parsing succeeds
	if (error) {
	  Serial.print(F("deserialize Json failed: ")); Serial.println(error.f_str());
	} else { // get settings from json config file
	  unsigned int intVal;
	  const char *strVal;
#ifdef ENABLE_MQTT
	  strVal = jsonConfig["mqtt"]["server"];			// mqtt settings
	  if (strVal) strcpy(Settings.data.mqttServer, strVal);
	  intVal = jsonConfig["mqtt"]["port"];
	  if (intVal) Settings.data.mqttPort = intVal;
	  strVal = jsonConfig["mqtt"]["user"];
	  if (strVal) strcpy(Settings.data.mqttUser, strVal);
	  strVal = jsonConfig["mqtt"]["pass"];
	  if (strVal) strcpy(Settings.data.mqttPass, strVal);
	  intVal = jsonConfig["mqtt"]["refresh"];
	  if (intVal) Settings.data.mqttRefresh = intVal;	// in s
#endif
	  strVal = jsonConfig["bms"]["name"];				// bms settings
	  if (strVal) strcpy(Settings.data.bmsName, strVal);
	  intVal = jsonConfig["bms"]["refresh"];
	  if (intVal) Settings.data.bmsRefresh = intVal;	// in s
	  intVal = jsonConfig["bms"]["sleepVoltage"];
	  if (intVal) Settings.data.sleepVoltage = intVal;
	  intVal = jsonConfig["bms"]["wakeupVoltage"];
	  if (intVal) Settings.data.wakeupVoltage = intVal;
	  // add list of wifi networks frpm json config file
	  for (int i = 0; i < jsonConfig["wifi"].size(); i++) { // get Wifi settings from json config file
		wifiMulti.addAP(jsonConfig["wifi"][i]["ssid"], jsonConfig["wifi"][i]["pass"]);
	  }
	}
  } else Settings.check();				// set default values from EEPROM

  // ### setup WiFi ###
  WiFi.mode(WIFI_STA);
  WiFi.hostname(Settings.data.deviceName);
#ifdef EEPROM_SETTINGS
  bool result;
  IPAddress local_IP;	result = local_IP.fromString(IP_ADDRESS);
  IPAddress gateway;	result = gateway.fromString(GATEWAY);
  IPAddress subnet;		result = subnet.fromString(SUBNET);
  IPAddress dns;		result = dns.fromString(DNS_SERVER);
  if (!WiFi.config(local_IP, gateway, subnet, dns)) {
#else
  if (!WiFi.config(IP_ADDRESS, GATEWAY, SUBNET, DNS_SERVER)) {
#endif
	Serial.println(F("failed to configure!"));
  }
  wifiScan();		// ssid and password from config.json
  wifiConnect();
  print_Setupdone("WiFi");
  digitalWrite(WIFI_LED, LOW);
//if (wm.autoConnect("JBDBMS2MQTT-AP");
  if (WiFi.status() == WL_CONNECTED) {
	digitalWrite(WIFI_LED, HIGH);
	delay(500);
  // ### setup time and date ###
	setupDateTime();						// setup DateTime
	delay(500);
#ifdef ENABLE_MQTT
	// ### setup MQTT ###
	setupMQTTClient();	// Mqtt.cpp
	delay(500);
	print_Setupdone("MQTT");
#endif
#ifdef ENABLE_OTA
	setupOTA();			// (Webserver.cpp)
	delay(500);
	print_Setupdone("OTA");
#endif
	// ### setup Webserver ###
	setupWebserver();	// (Webserver.cpp)
	delay(500);
  } // wifi connected
 // ### setup bluetooth ###
  Bms.refresh = Settings.data.bmsRefresh * 1000; // set bms refresh rate in ms
  Bms.basicData.Volts = NUMCELLS * 3 * 1000;	// initalize pack voltage not to disconnect WiFi
  strcpy(Bms.data.Alias, BMS_ALIAS); 			// set default alias ("BMS0")
  Bms.setup();									// scan BLE devices
  print_Setupdone("BLE");
  resetCounter(false);

  JbdBms::debugLevel = DBG_WEB | DBG_BLE3;

} // end setup()

//===================================== Main Loop

ulong lastLoop = 0;

void loop()
{
  static unsigned int loopCnt = 0;
#ifdef ENABLE_OTA
  ArduinoOTA.handle();
#endif
  Bms.process();						// request bms data, connects/reconnects bms
  if (JbdBms::dataErr > 100) {
    JbdBms::dataErr = 0;				// one message is enough
	Serial.print(F("BMS errors over allowed limit, "));
	restart = true;
	restartTime = millis() - 500;
  }
//Settings.data.mqttTopic[strlen(Settings.data.mqttTopic) - 1] = Bms.bmsNum; // set root topic
#ifdef ENABLE_SLEEP
  if (Bms.basicData.Volts <= Settings.data.sleepVoltage && WiFi.isConnected()) {
	Serial.println("disconnecting WiFi, battery Voltage: " + String(Bms.basicData.Volts) + " <= " + String(Settings.data.sleepVoltage));
	WiFi.disconnect(true);
	digitalWrite(WIFI_LED, LOW);
 #ifdef ENABLE_MQTT
	refresh = Settings.data.mqttRefresh * 10000;
 #endif
  }
  if (Bms.basicData.Volts > Settings.data.wakeupVoltage && !WiFi.isConnected()) {
	wifiConnect();
	Serial.println("woke up and WiFi reconnected, battery Voltage: " + String(Bms.basicData.Volts) + " > " + String(Settings.data.wakeupVoltage));
 #ifdef ENABLE_MQTT
	refresh = Settings.data.mqttRefresh * 1000;
 #endif
  }
#endif
  Wakeup(false);
  processLED();							// handle led
  processRelay();						// handle relay
  resetCounter(false);
  if (restart && millis() >= (restartTime + 500)) { // check restart
	Serial.println(F("restarting ESP..."));
	delay(1000);
	ESP.restart();
  }
  if (WiFi.status() == WL_CONNECTED) {	// make sure wifi is in the right mode
	if ((loopCnt % 20 == 0) && JbdBms::dataOk > 5) { // slow down web updates
//	  socket.cleanupClients();
	  cleanupClients();					// clean unused ws client connections
//	  server.handleClient();			// ???
//	  MDNS.update();
	  processWebserver();				// notify ws clients
	}
#ifdef ENABLE_MQTT
	processMQTT();						// handle mqtt, reconnects mqtt-server
 #ifdef ENABLE_HOMEASSISTENT
	if (HAtrigger || HAautotrigger) {	// handle ha discovery
	  if (sendHAdiscovery()) {
		HAtrigger = false;
		HAautotrigger = false;
	  }
	}
 #endif
#endif
  } else {
	wifiErr++;
	Serial.println(F("WiFi disconnected, try to reconnect..."));
	WiFi.disconnect();
	WiFi.reconnect();						// reconnect WiFi ???
	delay(1000);
	if (WiFi.status() != WL_CONNECTED) {
	  wifiScan();							// ssid and password from config.json
	  wifiConnect();
	}
  }
  if (wifiErr > 20) {
	wifiErr = 0;
	Serial.print(F("WiFi disconnected over allowed limit, "));
	restart = true;
	restartTime = millis() - 500;
  }
  loopCnt++;
} // end loop()

//===================================== Wakeup, Relay and LED Process

bool Wakeup(bool wakeup) // don't run immediately after boot, wait for first intervall
{
  if (wakeup) {
 	digitalWrite(WAKEUP_PIN, !digitalRead(WAKEUP_PIN));
	lastWakeup = millis();
	Serial.println(F("Wakeup activated!"));
  }
  if (lastWakeup != 0 && millis() - lastWakeup > WAKEUP_DURATION && lastWakeup != 0) {
	digitalWrite(WAKEUP_PIN, !digitalRead(WAKEUP_PIN));
	lastWakeup = 0;
	Serial.println(F("Wakeup deactivated!"));
  }
  return true;
}

#define ON	1	// true
#define OFF	0	// false

bool processRelay()
{
  static ulong lastSwitched;

  bool state = relayState;
  if (Settings.relay.enable && millis() - lastSwitched > RELAY_REFRESH * 1000) {
	lastSwitched = millis();
	byte mode = Settings.relay.function;
	if (mode > 0) {
	  if (Bms.bmsCmd == PAUSE || Settings.relay.failsafe) return false;
	  // now compare depending on the mode
	  float val = 0.0;
	  // read the value to compare to depending on the mode
	  if (mode == 1) val = Settings.data.SoCmin;				// Mode 1 - SocMin
	  else if (mode == 2) val = Settings.data.SoCmax;			// Mode 2 - SocMax
	  else if (mode == 3) val = Bms.cellData.CellMin / 1000;	// Mode 3 - lowest Cell Voltage
	  else if (mode == 4) val = Bms.cellData.CellMax / 1000;	// Mode 4 - highest Cell Voltage
	  else if (mode == 5) val = Bms.basicData.Volts / 100;		// Mode 5 - pack Voltage
	  else if (mode == 6) val = Bms.basicData.Temp[0];			// Mode 6 - temperature
	  mode = Settings.relay.compare;
	  if (mode) {			// higher or equal than
		// check if value is already true so we have to use hysteresis to switch off
		if (val >= Settings.relay.value) state = true;		// check if value is greater than
		else if (val < Settings.relay.value - Settings.relay.hysteresis) state = false;
	  } else {					// lower or equal than
		// check if value is already true so we have to use hysteresis to switch off
		if (val <= Settings.relay.value) state = true;		// check if value is less than
		else if (val > Settings.relay.value + Settings.relay.hysteresis) state = false; // use hystersis to switch off
	  } 
	} else {												// Mode 0 - manual per WEB or MQTT
	  state = relay;		// manual
	}
  }
  if (Settings.relay.invert) state = !state;				// inverted?
  if (relayState != state) {								// current state of relay
	digitalWrite(RELAY_PIN, state);
	relayState = state;										// save current state
	Serial.print("relay "); Serial.println((state) ? "on":"off");
	mqttPublishRelay();
  }
  return true;
}
/*
  Blinking LED = Relais Off
  Waveing LED = Relais On
  every 5 seconds:
  1x all ok - Working
  2x no BMS Connection
  3x no MQTT Connection
  4x no WiFi Connection
  5x no Time
  6x SPIFFS error
*/
#define BLINKING_PERIOD	5000	// all 5s
#define BLINK_TIME		100		// 100ms

void processLED()
{
  static ulong lastBlinking = 0;
  static ulong cycleMillis = 0;
//static byte error = 0;
  static byte led = OFF;

  ulong ms = millis();
  if (error == 0 && ms - lastBlinking > BLINKING_PERIOD) {
	if (WiFi.status() != WL_CONNECTED) error = 4;
	else if (!mqttClient.connected() && Settings.data.mqttServer[0] != 0) error = 3;
	else if (!Bms.connected) error = 2;
	else if (WiFi.status() == WL_CONNECTED) error = 1; // ok
  }
  if (error > 0) {
	if (ms - cycleMillis > BLINK_TIME) {
	  if (led == OFF) {
		led = ON;
	  } else {
		led = OFF;
		error--;
	  }
	  cycleMillis = ms;
	  if (error == 0) lastBlinking = ms;
	}
  }
  digitalWrite(ERR_LED, led);
}
// End main.cpp
