/*
JbdBMS2MQTT Project
https://github.com/yash-it/JbdBMS2MQTT
*/
#ifndef __SETUP_H__
#define __SETUP_H__

#include <Arduino.h>
#include <EEPROM.h>
#include "JbdBms.h"

//#define SUPER_MINI
#ifdef SUPER_MINI
#define TX_PIN		21
#define RX_PIN		20
#define WAKEUP_PIN	4			// default pin assignment
#define RELAY_PIN	5
#define ESP_LED		2			// -> BLE_LED, WeMos D1 Mini ESP32, 34, 35 input only!
#define WIFI_LED	7
#define BLE_LED		8			//27 also set in lib\jbdBMS\MyClientCallback.cpp!!!
#define MQTT_LED	2
#else
#define WAKEUP_PIN	21			// default pin assignment
#define RELAY_PIN	5
#define ESP_LED		2			// -> BLE_LED, WeMos D1 Mini ESP32, 34, 35 input only!
#define WIFI_LED	2			//32
#define BLE_LED		25			//27 also set in lib\jbdBMS\MyClientCallback.cpp!!!
#define MQTT_LED	27
#define ERR_LED		32
#endif
#define EEPROM_SIZE 512
// Battery settings:
#define NUMCELLS		8		// 8 cells for 24V battery
//#define BMS_NAME		"BSJBD202210-24-0007"
#define BMS_NAME		"BSJBD202308-01-104"
#define BMS_ALIAS		"BMS0"	// used for mqtt topic
#define BMS_REFRESH		5		// 5s
#define RELAY_REFRESH	1		// interval in s for relay process
#define WAKEUP_DURATION	250		// duration in ms for wakeup

// HTTP settings:
#define IP_ADDRESS		"192.168.6.145"
#define GATEWAY			"192.168.6.250"
#define DNS_SERVER		"192.168.6.254"
#define SUBNET			"255.255.255.0"
#define HTTP_PORT		80
#define HTTP_USER		"admin"
#define HTTP_PASSWORD	"olpP"
#define HOST_NAME		"Jbd2MQTT"

#define NTP_SERVER		"de.pool.ntp.org"
// MQTT settings:
#define MQTT_SERVER		"192.168.6.190"
#define MQTT_PORT		1883
#define MQTT_USER		"fhem"
#define MQTT_PASSWORD	"olpP5"
#define MQTT_TOPIC		"/ESP32-BMS0"
#define MQTT_TRIGGER	"/Shelly.Relay1/cmnd/POWER1 1"

#define MQTT_TOPIC_ROOT	"/ESP32-BMS2/"		// this is where mqtt data will be pushed
#define MQTT_TOPIC_STAT MQTT_TOPIC_ROOT "stat/" // Tasmota like!
#define MQTT_TOPIC_TELE MQTT_TOPIC_ROOT "tele/"
#define MQTT_TOPIC_CMD  MQTT_TOPIC_ROOT "cmnd/"
#define MQTT_REFRESH 20		// maximum mqtt update frequency in seconds (2)

#define CONFIG_FILE "config.json"
#define HEADER_FILE "header.html"
#define FOOTER_FILE "footer.html"

//#define ARDUINOJSON_USE_DOUBLE 0
//#define ARDUINOJSON_USE_LONG_LONG 0

//#define SWVERSION "1.1"
// DON'T edit version here, place version number in platformio.ini (custom_prog_version) !!!
#define SOFTWARE_VERSION SWVERSION
#ifdef DEBUG
#undef SOFTWARE_VERSION
#define SOFTWARE_VERSION SWVERSION " " HWBOARD " " __DATE__ " " __TIME__
#endif

#ifdef DEBUG
#define DEBUG_BEGIN(...) DEBUG.begin(__VA_ARGS__)
#define DEBUG_END(...) DEBUG.end(__VA_ARGS__)
#define DEBUG_PRINT(...) DEBUG.print(__VA_ARGS__)
#define DEBUG_PRINTF(...) DEBUG.printf(__VA_ARGS__)
#define DEBUG_WRITE(...) DEBUG.write(__VA_ARGS__)
#define DEBUG_PRINTLN(...) DEBUG.println(__VA_ARGS__)
#define DEBUG_WEB(...) WebSerial.print(__VA_ARGS__)
#define DEBUG_WEBLN(...) WebSerial.println(__VA_ARGS__)
#else
#undef DEBUG_BEGIN
#undef DEBUG_PRINT
#undef DEBUG_PRINTF
#undef DEBUG_WRITE
#undef DEBUG_PRINTLN
#undef DEBUG_WEB
#undef DEBUG_WEBLN
#define DEBUG_BEGIN(...)
#define DEBUG_PRINT(...)
#define DEBUG_PRINTF(...)
#define DEBUG_WRITE(...)
#define DEBUG_PRINTLN(...)
#define DEBUG_WEB(...)
#define DEBUG_WEBLN(...)
#endif

typedef struct
{								// do not re-sort this struct
  unsigned int config;			// config version, if changed, previus config will be set to default
  char deviceName[21];			// device name ("name\0)
  char ntpServer[21];			// ntp server address
  char mqttServer[21];			// mqtt server address
  char mqttUser[21];			// mqtt username
  char mqttPass[21];			// mqtt password
  char mqttTopic[21];			// mqtt publish topic
  unsigned int mqttPort;		// mqtt port
  unsigned int mqttRefresh;		// mqtt refresh time in s
  bool mqttJson;				// switch between classic mqtt and json
  char mqttTrigger[81];			// MQTT data trigger path
  unsigned int httpPort;		// http port
  char httpUser[21];			// http basic auth username
  char httpPass[21];			// http basic auth password
  char bmsName[21];				// bms name
  unsigned int bmsRefresh;		// bms refresh time in s
  unsigned int sleepVoltage;	// sleep control (50% = 26.3V)
  unsigned int wakeupVoltage;	// 13399mV
  unsigned int SoCmin;			// 
  unsigned int SoCmax;			//
  bool wakeupEnable;			// use wakeup output?
  bool darkMode;				// flag for color mode: dark/light in webUI
  bool haDiscovery;				// HomeAssistant Discovery switch
  int testVal;
} dataStruct;

typedef struct
{
  bool enable;				// enable relais output?
  bool failsafe;			// relais failsafe mode | false - turn off, true - keep last state
  bool invert;				// invert relais output?
  byte function;			// function mode - 0 = Lowest Cell Voltage, 1 = Highest Cell Voltage, 2 = Pack Cell Voltage, 3 = Temperature
  byte compare;				// comparsion mode - 0 = higher or equal than, 1 = lower or equal than
  float value;				// value to compare to
  float hysteresis;			// hysteresis
} relayStruct;


class Setup // create instance with: Setup Settings;
{
  // change eeprom config version ONLY when new parameter is added and need reset the parameter
  unsigned int configVersion = 11;

  public:
	static dataStruct data;
	static relayStruct relay;
	void load(), save(), reset(), print(), check();
	bool ParseIPv4(uint32_t *addr, const char *str_p);

  private:
	void checkConfig();
};
#endif // End #ifndef __JBDBMS_H__
