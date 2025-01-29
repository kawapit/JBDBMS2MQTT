/*
JbdBMS2MQTT Project
https://github.com/yash-it/JbdBMS2MQTT
*/

#ifndef __MQTT_CPP__
#define __MQTT_CPP__

#include "Mqtt.h"
//--------------------------------
// instances:
WiFiClient espClient;
PubSubClient mqttClient(espClient);
// global variables:
unsigned int refresh = MQTT_REFRESH * 1000;// set default mqtt publishing intervall
bool firstPublish = false;			// true after successfull mqtt connection to broker
//bool HATrigger = false;
//bool HAAutoTrigger = false;
int mqttErr = 0;
bool vccErr = false;		// test Vcc error
bool bmsErr = false;

WORD_ALIGNED_ATTR char topicBuffer[64];
//char jsonBuffer[1024];
#define JSON_BUFFER 2048
StaticJsonDocument<JSON_BUFFER> bmsJson;					// main Json
JsonObject deviceJson = bmsJson.createNestedObject("ESP");	// basic device data
JsonObject packJson  = bmsJson.createNestedObject("BAT");	// battery package data
JsonObject cellsJson = bmsJson.createNestedObject("CELLS");	// nested data for cell voltages
JsonObject tempsJson = bmsJson.createNestedObject("TEMP");	// nested data for bms temps
JsonObject relayJson = bmsJson.createNestedObject("REL");	// nested data for relay

//-----------------------------------
char *fullTopic(char *bufPtr, char const *prefix)
{
  const char *pTopic = Settings.data.mqttTopic; // get the main topic ("/ESP32-BMS0")
  // if handling more devices, number has to be set here!
//char bmsNum = char(Bms.data.Alias[strlen(Bms.data.Alias) - 1]); // new char is bms number [3]
  strcpy(bufPtr, pTopic);
//strcat(bufPtr, Num);				// no need to append character 
//size_t len = strlen(bufPtr);
//bufPtr[strlen(bufPtr) - 1] = bmsNum; // replace number
//bufPtr[len + 1] = '\0';			// append character
  strcat(bufPtr, "/");
  strcat(bufPtr, prefix);
  return bufPtr;
}


char *float3_str(char *bufPtr, int intVal)	// converts int values into a float string (3 decimals)
{
  snprintf(bufPtr, 8, "%.3f", float(intVal / 1000.0f));
  return bufPtr;
}

char *float2_str(char *bufPtr, int intVal)	// converts int values into a float string (2 decimals)
{
  snprintf(bufPtr, 8, "%.2f", float(intVal / 1000.0f));
  return bufPtr;
}

char *float1_str(char *bufPtr, int intVal)	// converts int values into a float string (1 decimal)
{
  snprintf(bufPtr, 8, "%.1f", float(intVal / 1000.0f));
  return bufPtr;
}

char *float0_str(char *bufPtr, int intVal)	// converts int values into a float string (0 decimals)
{
  snprintf(bufPtr, 6, "%.0f", float(intVal / 1000.0f));
  return bufPtr;
}


char *temp_str(char *bufPtr, int intVal)
{
  snprintf(bufPtr, 6, "%.1f", float(intVal / 10.0f));
  return bufPtr;
}

char *int_str(char *bufPtr, int intVal)
{
  snprintf(bufPtr, 5, "%d", intVal);
  return bufPtr;
}

/* not testet, negativ value without decimals (power)
char *intn_str(char *bufPtr, int intVal)
{
  snprintf(bufPtr, 5, "%+d", intVal / 1000.0f);
  return bufPtr;
}
*/

void mqttPublishBMSData() // json style
{
//{"BMS_Alias":"BMS2","BMS_Name":"BSJBD202308-01-104","BMS_Address":"a4:c1:37:42:c5:e2","BMS_ServiceUUID":"0000ff00-0000-1000-8000-00805f9b34fb",
// "BMS_RSSI":-91,"BMS_TXPower":157,"BMS_refresh":5,"BMS_cmd":4}

  if (JbdBms::debugLevel & DBG_MQTT) Serial.println(F("publishing BMS Data..."));
  String payload = "{";
  payload.reserve(230);
  payload +=    "\"BMS_Alias\":\"";		payload += Bms.data.Alias;
  payload += "\",\"BMS_Name\":\"";		payload += Bms.data.Name;
  payload += "\",\"BMS_Address\":\"";	payload += Bms.data.Address;
  payload += "\",\"BMS_ServiceUUID\":\""; payload += Bms.data.ServiceUUID;
//payload += "\",\"BMS_CharUUID\":\"";	payload += Bms.data.CharUUID;
  payload += "\",\"BMS_RSSI\":";		payload += Bms.data.RSSI; // no "
  payload +=   ",\"BMS_TXPower\":";		payload += Bms.data.TXPower; // no "
  payload +=   ",\"BMS_refresh\":";		payload += Bms.refresh / 1000; // no "
  payload +=   ",\"BMS_cmd\":";			payload += Bms.bmsCmd; // no "
  payload += "}";
  if (JbdBms::debugLevel & DBG_MQTT) Serial.println(payload);	// string is ok
  mqttClient.publish(fullTopic(topicBuffer, "stat/BMS"), String(payload).c_str(), false);
//mqttClient.publish(fullTopic(topicBuffer, "stat/BMS/Name"), String(Bms.data.Name).c_str(), false);
}


int WifiGetRssiAsQuality(int rssi) // from Tasmota
{
  int quality = 0;
  if (rssi <= -100) {
	quality = 0;
  } else if (rssi >= -50) {
	quality = 100;
  } else {
	quality = 2 * (rssi + 100);
  }
  return quality;
}
/*
//snprintf_P(mqtt_data, sizeof(mqtt_data), PSTR("%s,\""Wifi"\":{\""AP"\":%d,\""SSId"\":\"%s\",\""RSSI"\":%d,\""APMac"\":\"%s\"}}"),
//mqtt_data, Settings.sta_active + 1, Settings.sta_ssid[Settings.sta_active], WifiGetRssiAsQuality(WiFi.RSSI()), WiFi.BSSIDstr().c_str());

ESP-OBIS/tele/STATE {"Time":"2023-12-17T17:13:39","Uptime":"20T07:27:59","UptimeSec":1754879,"Heap":20,"SleepMode":"Dynamic","Sleep":50,"LoadAvg":19,"MqttCount":19,"Wifi":{"AP":1,"SSId":"yash-it","BSSId":"34:31:C4:1D:BF:86","Channel":7,"Mode":"11n","RSSI":50,"Signal":-75,"LinkCount":1,"Downtime":"0T00:00:03"}}
ESP-OBIS/tele/SENSOR {"Time":"2023-12-17T17:13:39","SML":{"NetId":"1EBZ0100120512","Ein":25231.368,"Eout":70.945,"P":174.3,"P1":117.5,"P2":31.7,"P3":25.1,"U1":0.0,"U2":0.0,"U3":0.0}}
  nt":19,"Wifi":{"AP":1,"SSId":"yash-it","BSSId":"34:31:C4:1D:BF:86","Channel":7,"Mode":"11n","RSSI":50,"Signal":-75,"LinkCount":1,"Downtime":"0T00:00:03"}}
Sonoff-SPM/tele/SENSOR {"Time":"2023-12-17T17:36:00","ESP32":{"Temperature":53.9},"SPM":{"Energy":[0.0,0.0,0.0,0.0],"Yesterday":[0.0,0.0,0.0,0.0],"Today":[0.0,0.0,0.0,0.0],
"ActivePower":[0.0,0.0,0.0,0.0],"ApparentPower":[0.0,0.0,0.0,0.0],"ReactivePower":[0.0,0.0,0.0,0.0],"Factor":[0.00,0.00,0.00,0.00],"Voltage":[233.7,0.0,0.0,0.0],"Current":[0.00,0.00,0.00,0.00]},"TempUnit":"C"}
*/
// publish WiFi and constant data (IP, AP, SSId, RSSI, Signal and capacity, cells)
void mqttPublishESPData() // json style
{
//{"Time":"2024-02-10T13:36:40","UptimeSec":1498,"ESP_IP":"192.168.6.145","ESP_SSId":"yash-it","ESP_BSSId":"34:31:C4:1D:BF:86",
//"ESP_Channel":7,"ESP_Mode":"11n","ESP_RSSI":-73,"ESP_Signal":54}

  if (JbdBms::debugLevel & DBG_MQTT) Serial.println(F("publishing ESP Data..."));
  String payload = "{";
  payload.reserve(190);
  char timeStr[20];
  DateTimeParts p = DateTime.getParts();
  snprintf(timeStr, sizeof(timeStr), "%04d-%02d-%02dT%02d:%02d:%02d",
				p.getYear(), p.getMonth(), p.getMonthDay(), p.getHours(), p.getMinutes(), p.getSeconds()); // (2023-12-17T17:13:39)
  payload +=    "\"Time\":\"";		payload += timeStr;
  payload += "\",\"UptimeSec\":";	payload += String(millis() / 1000);	// no "
  payload +=   ",\"ESP_IP\":\"";	payload += String(WiFi.localIP().toString().c_str());
//payload += "\",\"ESP_AP\":1";
  payload += "\",\"ESP_SSId\":\"";	payload += WiFi.SSID().c_str();
  payload += "\",\"ESP_BSSId\":\"";	payload += WiFi.BSSIDstr().c_str();
  payload += "\",\"ESP_Channel\":"; payload += String(WiFi.channel());	// no "
  payload +=   ",\"ESP_Mode\":\"";//payload += String(WiFi.mode().c_str());
  payload += "11n";
  payload += "\",\"ESP_RSSI\":";	payload += String(WiFi.RSSI());	// no "
  payload +=   ",\"ESP_Signal\":";	payload += WifiGetRssiAsQuality(WiFi.RSSI());
  payload += "}";
  if (JbdBms::debugLevel & DBG_MQTT) Serial.println(payload);	// string is ok
  mqttClient.publish(fullTopic(topicBuffer, "stat/ESP"), String(payload).c_str(), false);
  char strBuf[5] = ""; // 0000
  snprintf(strBuf, 4, "%.0f", Bms.basicData.Capacity / 100.0);
  mqttClient.publish(fullTopic(topicBuffer, "stat/BAT/capacity"), strBuf, true); 
  snprintf(strBuf, 2, "%d", Bms.basicData.NumOfCells);
  mqttClient.publish(fullTopic(topicBuffer, "stat/BAT/cells"), strBuf, true); 
}

// publish only if forced or data has changed more than minDiff
#define ABS_DIFF(a, b) (a > b ? a-b : b-a)
void mqtt_publish_float3(const char* topic, int newValue, int oldValue, int minDiff, bool force)
{
  char strBuf[8] = ""; // "-00.000"		// cell voltages
  if (force || ABS_DIFF(newValue, oldValue) > minDiff) {
	snprintf(strBuf, sizeof(strBuf), "%.3f", float(newValue / 1000.0f));
	mqttClient.publish(topic, strBuf, false);
  }
}

void mqtt_publish_float2(const char* topic, int newValue, int oldValue, int minDiff, bool force)
{
  char strBuf[8] = ""; // "-000.00"		// voltage in V
  if (force || ABS_DIFF(newValue, oldValue) > minDiff) {
	snprintf(strBuf, sizeof(strBuf), "%.2f", float(newValue / 1000.0f));
	mqttClient.publish(topic, strBuf, false);
  }
}

void mqtt_publish_float1(const char* topic, int newValue, int oldValue, int minDiff, bool force)
{
  char strBuf[8] = ""; // "-0000.0"		// current in A, capacity in Ah
  if (force || ABS_DIFF(newValue, oldValue) > minDiff) {
	snprintf(strBuf, sizeof(strBuf), "%.1f", float(newValue / 1000.0f));
	mqttClient.publish(topic, strBuf, false);
  }
}

void mqtt_publish_float0(const char* topic, int newValue, int oldValue, int minDiff, bool force)
{
  char strBuf[6] = ""; // "-0000"		// power in W
  if (force || ABS_DIFF(newValue, oldValue) > minDiff) {
	snprintf(strBuf, sizeof(strBuf), "%.0f", float(newValue / 1000.0f));
	mqttClient.publish(topic, strBuf, false);
  }
}

void mqtt_publish_temp(const char* topic, int newValue, int oldValue, bool force)
{
  char strBuf[6] = ""; // "-00.0"	// temps
  if (force || newValue != oldValue) {		// if difference is 0.1deg for temps
	snprintf(strBuf, sizeof(strBuf), "%.1f", float(newValue / 10.0f));
	mqttClient.publish(topic, strBuf, false);
  }
}

void mqtt_publish_int(const char* topic, int newValue, int oldValue, bool force)
{
  char strBuf[5] = ""; // "00000"
  if (force || newValue != oldValue) {		// no min. difference check for soc, cycles, etc.
	snprintf(strBuf, sizeof(strBuf), "%d", newValue);
	mqttClient.publish(topic, strBuf, false);
  }
}
/* not used
void mqtt_publish_string(const char* topic, const char* newValue, const char* oldValue, bool force)
{
  if (force || strcmp(newValue, oldValue) != 0) {
	mqttClient.publish(topic, newValue, false);
  }
}

mqtt_publish_fets(const char* topic, const char* newValue, const char* oldValue, bool force)
{
if (force || strcmp(newValue, oldValue) != 0)
	mqttClient.publish(topic, (newValue == 1) ? "on":"off", false);
}
*/

void mqttPublishBasicData(const basicDataStruct &oldbasicData, bool forceUpdate) // if true - we will send all data regardless if it's the same
{
  if (JbdBms::debugLevel & DBG_MQTT) Serial.println(F("publishing Basic Data..."));
  if (Settings.data.mqttJson) { // json style
	char value[8] = "";
	String payload = "{";
	payload.reserve(256);
	payload +=  "\"voltage\":";	payload += float2_str(value, Bms.basicData.Volts);
	payload += ",\"current\":";	payload += float1_str(value, Bms.basicData.Amps);
	payload += ",\"power\":";	payload += float0_str(value, Bms.basicData.Watts);
	payload += ",\"soc\":";		payload += int_str(value, Bms.basicData.SoC);
	payload += ",\"soc_Ah\":";	payload += float1_str(value, Bms.basicData.SoC_Ah * 10);
	payload += ",\"cycles\":";	payload += int_str(value, Bms.basicData.Cycles);
	payload += ",\"temp\":";	payload += temp_str(value, Bms.basicData.Temp[0]);
	payload += ",\"temp1\":";	payload += temp_str(value, Bms.basicData.Temp[1]);
	if (Bms.basicData.NumOfTemp > 1) {
	  payload += ",\"temp2\":";	payload += temp_str(value, Bms.basicData.Temp[2]); }
	if (Bms.basicData.NumOfTemp > 2) {
	  payload += ",\"temp3\":";	payload += temp_str(value, Bms.basicData.Temp[3]); }
	payload += ",\"fets\":";		payload += int_str(value, Bms.basicData.MosFet);
	payload += ",\"charge\":";		payload += int_str(value, Bms.basicData.charge);
	payload += ",\"discharge\":";	payload += int_str(value, Bms.basicData.discharge);
	payload += ",\"balance1\":";	payload += int_str(value, Bms.basicData.BalanceCodeLow);
	payload += ",\"balance2\":";	payload += int_str(value, Bms.basicData.BalanceCodeHigh);
	payload += ",\"protection\":";	payload += int_str(value, Bms.basicData.Protection);
	payload += "}";
	mqttClient.publish(fullTopic(topicBuffer,"tele/BAT"), String(payload).c_str(), false);
	if (JbdBms::debugLevel & DBG_MQTT) Serial.println(payload);
  } else { // old style
	mqtt_publish_float2(fullTopic(topicBuffer, "tele/BAT/voltage"), Bms.basicData.Volts,	oldbasicData.Volts, 0, forceUpdate); // in mV
	mqtt_publish_float1(fullTopic(topicBuffer, "tele/BAT/current"), Bms.basicData.Amps,	oldbasicData.Amps , 100, forceUpdate); // in mA
	mqtt_publish_float0(fullTopic(topicBuffer,  "tele/BAT/power"),	Bms.basicData.Watts,	oldbasicData.Watts, 0, forceUpdate); // in mW
	mqtt_publish_int(fullTopic(topicBuffer,    "tele/BAT/soc"),		Bms.basicData.SoC,		oldbasicData.SoC, forceUpdate);		  // in %
	mqtt_publish_float1(fullTopic(topicBuffer, "tele/BAT/soc_Ah"),	Bms.basicData.SoC_Ah * 10, oldbasicData.SoC_Ah * 10, 100, forceUpdate); // in mAh
	mqtt_publish_int(fullTopic(topicBuffer,   "tele/BAT/cycles"),	Bms.basicData.Cycles,	oldbasicData.Cycles, forceUpdate);
	mqtt_publish_temp(fullTopic(topicBuffer,  "tele/BAT/temp"),		Bms.basicData.Temp[0],	oldbasicData.Temp[0], forceUpdate);
	mqtt_publish_temp(fullTopic(topicBuffer,  "tele/BAT/temp1"),	Bms.basicData.Temp[1],	oldbasicData.Temp[1], forceUpdate);
	if (Bms.basicData.NumOfTemp > 1)
	  mqtt_publish_temp(fullTopic(topicBuffer, "tele/BAT/temp2"),	Bms.basicData.Temp[2],	oldbasicData.Temp[2], forceUpdate);
	if (Bms.basicData.NumOfTemp > 2)
	  mqtt_publish_temp(fullTopic(topicBuffer, "tele/BAT/temp3"),	Bms.basicData.Temp[3],	oldbasicData.Temp[3], forceUpdate);
	mqtt_publish_int(fullTopic(topicBuffer, "tele/BAT/fets"),		Bms.basicData.MosFet,			oldbasicData.MosFet, forceUpdate);
	mqtt_publish_int(fullTopic(topicBuffer, "tele/BAT/charge"),		Bms.basicData.charge,			oldbasicData.charge, forceUpdate);
	mqtt_publish_int(fullTopic(topicBuffer, "tele/BAT/discharge"),	Bms.basicData.discharge,		oldbasicData.discharge, forceUpdate);
	mqtt_publish_int(fullTopic(topicBuffer, "tele/BAT/balance1"),	Bms.basicData.BalanceCodeLow,	oldbasicData.BalanceCodeLow, forceUpdate);
	mqtt_publish_int(fullTopic(topicBuffer, "tele/BAT/balance2"),	Bms.basicData.BalanceCodeHigh,	oldbasicData.BalanceCodeHigh, forceUpdate);
	mqtt_publish_int(fullTopic(topicBuffer, "tele/BAT/protection"), Bms.basicData.Protection,		oldbasicData.Protection, forceUpdate);
  }
}


void mqttPublishCellData(const cellDataStruct &oldcellData, bool forceUpdate) // if true - we will send all data regardless if it's the same
{
  if (JbdBms::debugLevel & DBG_MQTT) Serial.println(F("publishing Cell Data..."));
  byte cellmax = Bms.basicData.NumOfCells;
  if (Settings.data.mqttJson) { // json style
	char value[8] = "";
	String payload = "{";
	payload.reserve(256);
	for (byte cell = 0; cell < cellmax; cell++) {
	  uint16_t oldValue = oldcellData.CellVolt[cell];
	  uint16_t newValue = Bms.cellData.CellVolt[cell];
	  if (forceUpdate || newValue != oldValue) {	// cell voltage changed?
		payload += "\"cell";
		if (cell < 10)								// 16 cell battery?
		  payload += char('1' + cell);				// cell1..cell9
		else
		  payload += "1" + char('0' + cell - 10);	// cell10..cell16
		payload += "\":";
		snprintf(value, sizeof(value), "%.3f", float(newValue / 1000.0f)); // ex. 3.234
		payload += value;
		payload += ",";
	  }
	}
	payload +=  "\"min\":";  payload += float3_str(value, Bms.cellData.CellMin);
	payload += ",\"max\":";  payload += float3_str(value, Bms.cellData.CellMax);
	payload += ",\"diff\":"; payload += float3_str(value, Bms.cellData.CellDiff);
	payload += ",\"imin\":"; payload += int_str(value, Bms.cellData.iMin);
	payload += ",\"imax\":"; payload += int_str(value, Bms.cellData.iMax);
	payload += "}";
	if (JbdBms::debugLevel & DBG_MQTT) Serial.println(payload); // string is ok
//	boolean mqttPublish(const char* topic, const char* String(payload).c_str(), false);
	mqttClient.publish(fullTopic(topicBuffer, "tele/CELLS"), String(payload).c_str(), false);
  } else { // old style
	char topic[] = "tele/CELLS/cell0"; // \0 is added as last byte
	for (byte c = 0; c < cellmax; c++) {
	  topic[sizeof(topic) - 2] = char('1' + c);
	  mqtt_publish_float3(fullTopic(topicBuffer, topic), Bms.cellData.CellVolt[c], oldcellData.CellVolt[c], 0, forceUpdate); // all in mV
	}
	mqtt_publish_float3(fullTopic(topicBuffer, "tele/CELLS/min"),  Bms.cellData.CellMin, oldcellData.CellMin, 0, forceUpdate);
	mqtt_publish_float3(fullTopic(topicBuffer, "tele/CELLS/max"),  Bms.cellData.CellMax, oldcellData.CellMax, 0, forceUpdate);
	mqtt_publish_float3(fullTopic(topicBuffer, "tele/CELLS/diff"), Bms.cellData.CellDiff, oldcellData.CellDiff, 0, forceUpdate);
	mqtt_publish_int(fullTopic(topicBuffer, "tele/CELLS/imin"),   Bms.cellData.iMin, oldcellData.iMin, forceUpdate);
	mqtt_publish_int(fullTopic(topicBuffer, "tele/CELLS/imax"),   Bms.cellData.iMax, oldcellData.iMax, forceUpdate);
  }
}


void mqttPublishRelay()
{
  mqttClient.publish(fullTopic(topicBuffer, "tele/ESP/relay"), (relayState) ? "on":"off", true); 
}

//===================================== new Json method to generate payload
/*
Client (null) received PUBLISH (d0, q0, r0, m0, '/ESP32-BMS2/tele', ... (772 bytes)) /ESP32-BMS2/tele {
"ESP":{"vcc":"3.3","version":"1.2","flash":4096,"sketch":1223,"free":1920,"rssi":-70,"frequency":240,"freeheap":100,
 "jsonmem":1484,"jsoncap":2048,"runtime":299,"mqttJson":0},
"BAT":{"state":"online","alias":"BMS2","name":"BSJBD202308-01-104","voltage":25.89,"current":-3.159,"power":670,"soc":27,"soc_Ah":63.21,
 "cycles":7,"charge":1,"discharge":1,"balance":0,"protection":0,"failcodes":"","relay":0,"manual":1},
"CELLS":{"cells":8,"imax":1,"imin":3,"max":3.238,"min":3.236,"diff":0.002,"vmax":"3.6","vmin":"2.6",
 "cell1":3.238,"bal1":0,"cell2":3.237,"bal2":0,"cell3":3.236,"bal3":0,"cell4":3.238,"bal4":0,"cell5":3.238,"bal5":0,"cell6":3.238,"bal6":0,"cell7":3.237,"bal7":0,"cell8":3.237,"bal8":0},
"TEMP":{"temp":"18.5","temp1":"19.1","temp2":"18.4","temp3":"18.2"},
"REL":{"active":0,"manual":0,"invert":0,"function":0}}
*/

void getJsonDevice()
{
  if (vccErr) deviceJson[F("vcc")] = 2.5; // test alert message
  else deviceJson[F("vcc")] = 3.3; // (ESP.getVcc() / 1000.0) + 0.3;
  deviceJson[F("version")] = SOFTWARE_VERSION;
  deviceJson[F("flash")]  = ESP.getFlashChipSize() / 1024;
  deviceJson[F("sketch")] = ESP.getSketchSize() / 1024;
  deviceJson[F("free")] = ESP.getFreeSketchSpace() / 1024;
  deviceJson[F("rssi")] = WiFi.RSSI();
//#ifdef BMS_DEBUG
  deviceJson[F("frequency")] = ESP.getCpuFreqMHz();
//deviceJson[F("realflashsize")] = ESP.getFlashChipRealSize() / 1024;
  deviceJson[F("freeheap")] = ESP.getFreeHeap() / 1024;
//deviceJson[F("heapfragmentation")] = ESP.getHeapFragmentation() / 1024;
//deviceJson[F("freeblockSize")] = ESP.getMaxFreeBlockSize() / 1024;
  deviceJson[F("jsonmem")] = bmsJson.memoryUsage();
  deviceJson[F("jsoncap")] = bmsJson.capacity();
  deviceJson[F("runtime")] = millis() / 1000;
//deviceJson[F("wsclients")] = ws.count();
  deviceJson[F("mqttJson")] = (Settings.data.mqttJson) ? 1:0;
//#endif
}

void getJsonData()	// updated over socket event from website: bms.html
					// order is not constant!
{
  char value[8] = "";
  packJson[F("state")] = (Bms.data.State) ? "online":"offline";
  packJson[F("alias")] = String(Bms.data.Alias);			// convert array to string
  packJson[F("name")]  = Bms.data.Name;
//packJson[F("heartbeat")] = Bms.basicData.Heartbeat;
  packJson[F("voltage")] = Bms.basicData.Volts / 1000.0;	// in V
  packJson[F("current")] = Bms.basicData.Amps / 1000.0;	// in A
  packJson[F("power")]   = Bms.basicData.Watts / 1000.0;	// in W
  packJson[F("soc")]     = Bms.basicData.SoC;
  packJson[F("soc_Ah")]  = Bms.basicData.SoC_Ah / 100.0;
  packJson[F("cycles")]  = Bms.basicData.Cycles;
  packJson[F("charge")]    = Bms.basicData.charge;
  packJson[F("discharge")] = Bms.basicData.discharge;
  packJson[F("balance")]     = (Bms.basicData.BalanceCodeLow != 0) ? 1:0;
  if (bmsErr) packJson[F("protection")] = 4;	// test webui alert
  else packJson[F("protection")]  = Bms.basicData.Protection;
  packJson[F("failcodes")]   = JbdBms::ErrorString(); // from JbdBms instance
  packJson[F("cells")] = Bms.basicData.NumOfCells;
  packJson[F("imax")]  = Bms.cellData.iMax;
  packJson[F("imin")]  = Bms.cellData.iMin;
  packJson[F("max")]   = Bms.cellData.CellMax / 1000.0;
  packJson[F("min")]   = Bms.cellData.CellMin / 1000.0;
  packJson[F("diff")]  = Bms.cellData.CellDiff / 1000.0;
  packJson[F("vmax")]  = 3.5;			// used by cell barchart
  packJson[F("vmin")]  = 2.5; 
  for (size_t c = 0; c < Bms.basicData.NumOfCells; c++) {
	cellsJson["cell" + String(c + 1)] = Bms.cellData.CellVolt[c] / 1000.0; // in V
//} // bms.html reads in this order!
//for (size_t c = 0; c < Bms.basicData.NumOfCells; c++) {
	cellsJson["bal" + String(c + 1)] = (Bms.basicData.BalanceCodeLow && 1<<c) ? 1:0;
  }
/*
  packJson[F("cell_hVt")]  = Bms.basicData.maxCellThreshold1 / 1000; vmax
  packJson[F("cell_lVt")]  = Bms.basicData.minCellThreshold1 / 1000; vmin
  packJson[F("cell_hVt2")] = Bms.basicData.maxCellThreshold2 / 1000;
  packJson[F("cell_lVt2")] = Bms.basicData.minCellThreshold2 / 1000;

  packJson[F("pack_hVt")]  = Bms.basicData.maxPackThreshold1 / 10;
  packJson[F("pack_lVt")]  = Bms.basicData.minPackThreshold1 / 10;
  packJson[F("pack_hVt2")] = Bms.basicData.maxPackThreshold2 / 10;
  packJson[F("pack_lVt2")] = Bms.basicData.minPackThreshold2 / 10;
*/
  tempsJson[F("temp")] = temp_str(value, Bms.basicData.Temp[0]);
  for (size_t n = 0; n < Bms.basicData.NumOfTemp; n++) {
	tempsJson["temp" + String(n + 1)] = temp_str(value, Bms.basicData.Temp[n + 1]);
  }
/*
  for (size_t d = 0; d < bms.dsData.numOfDS; d++) {	// DS temp sensors
	if (tempSens.getAddress(tempDeviceAddress, d)) {
	  tempJson["DS18B20" + String(d + 1)] = tempSens.getTempC(tempDeviceAddress);
	}
	tempJson["dstemp" + char('1' + d)] = temp_str(value, Bms.dsData.DSTemp[d] / 10.0);
  }
*/
}

void getJsonRelay()
{
  relayJson[F("state")] = (relayState) ? 1:0;
  relayJson[F("manual")] = (Settings.relay.enable && Settings.relay.function == 0) ? 1:0;
  relayJson[F("invert")] = (Settings.relay.invert) ? 1:0;
  relayJson[F("function")] = Settings.relay.function;
}
//===================================== MQTT Callback, handle subscribed topics

boolean isValidNumber(String str)
{
  for (byte i = 0; i < str.length(); i++)
	if (!isDigit(str.charAt(i))) return false;
  return true;
}

// https://www.survivingwithandroid.com/esp32-mqtt-client-publish-and-subscribe/
void mqttCallback(char* topic, byte* payload, unsigned int length) // mqtt callback
{
  digitalWrite(MQTT_LED, HIGH);
// extract suffix string
//size_t suffixLength = strlen(topic) - sizeof(MQTT_TOPIC_CMD) + 1;
  size_t prefixLength = strlen(Settings.data.mqttTopic) + 6; // ex. "/ESP32-BMS0/cmnd/"
  size_t suffixLength = strlen(topic) - prefixLength;
  String strSuffix = "";

  for (size_t i = 0; i < suffixLength; i++) {	// get suffix as string
//	strSuffix += (char)topic[sizeof(MQTT_TOPIC_CMD) - 1 + i];
	strSuffix += (char)topic[prefixLength + i]; // "/ESP32-BMS0/"
  }
/*
  char strSuffix[10];							// get suffix as array
  if (suffixLength > sizeof(strSuffix)) suffixLength = sizeof(strSuffix);
  for (size_t i = 0; i < suffixLength; i++) {
	strSuffix[i] = (char)topic[sizeof(MQTT_TOPIC_CMD) - 1 + i];
  }
  strSuffix[suffixLength] = 0;	// EoT

  String strPayload = "";			// convert to string
  for (int i = 0; i < length; i++) {
	strPayload += (char)payload[i];
  }
*/
  char strPayload[length + 1];		// convert to array
  memcpy(strPayload, payload, length);
  strPayload[length] = 0;	// EoT
//Serial.print(F("MQTT message: ")); Serial.print(topic); Serial.print(" "); Serial.print(strPayload);
//Serial.print(F(" payload: ")); Serial.print(strSuffix); Serial.print(' '); // test
  Serial.print(F("MQTT payload: ")); Serial.print(strSuffix); Serial.print('=');

//String topicStr = mqttClient.messageTopic();		// save topic
  int intPayload;					// check if payload is int and convert it to int
  if (isValidNumber(strPayload)) intPayload = atoi(strPayload);
  char value[4] = "";

  if (strSuffix == "power") {		// activate bms requests, also mqtt messages
	Bms.bmsCmd = (intPayload) ? CONNECT:DISCONNECT;
	mqttClient.publish(fullTopic(topicBuffer, "stat/BMS/state"), (intPayload) ? "connected":"disconnected", false);

  } else if (strSuffix == "charge") {
	Bms.setCharge(bool(intPayload));
	Serial.println((intPayload) ? "on":"off");
	mqttClient.publish(fullTopic(topicBuffer, "tele/BAT/charge"), int_str(value, intPayload), false);

  } else if (strSuffix == "discharge") {
	Bms.setDischarge(bool(intPayload));
	Serial.println((intPayload) ? "on":"off");
	mqttClient.publish(fullTopic(topicBuffer, "tele/BAT/discharge"), int_str(value, intPayload), false);

  } else if (strSuffix == "balance") {
	Bms.setBalanceEnable(bool(intPayload));
	Serial.println((intPayload) ? "on":"off");
	mqttClient.publish(fullTopic(topicBuffer, "tele/BAT/balance"), int_str(value, intPayload), false);

  } else if (strSuffix == "relay") {
	relay = (bool)intPayload;
	Serial.println((relay) ? "on":"off");
	mqttClient.publish(fullTopic(topicBuffer, "tele/ESP/relay"), int_str(value, relay), false);

  } else if (strSuffix == "teleperiod") {
	if (intPayload > 2 && intPayload < 3600) {
	  Settings.data.mqttRefresh = intPayload;
	  refresh = intPayload * 1000;				// set mqtt refresh in ms
	}
//	Settings.data.mqttJson = true;				// also enable json
	Serial.print(Settings.data.mqttRefresh); Serial.println('s');
	mqttClient.publish(fullTopic(topicBuffer, "stat/ESP/teleperiod"), int_str(value, Settings.data.mqttRefresh), false);

  } else if (strSuffix == "bmsperiod") {
	if (intPayload > 1 && intPayload < 7200) {	//   2sec - 2hours ($1C20)
	  Settings.data.bmsRefresh = intPayload;
 	  Bms.refresh = intPayload * 1000;			// set bms refresh in ms
	}
	Bms.bmsCmd = BASICINFO;						// also activate bms requests
	Serial.print(Settings.data.bmsRefresh); Serial.println('s');
	mqttClient.publish(fullTopic(topicBuffer, "stat/BMS/bmsperiod"), int_str(value, Settings.data.bmsRefresh), false);

  } else if (strSuffix == "wakeup") {
	Settings.data.wakeupEnable = (bool)intPayload;	// set wakeup
	Serial.println((intPayload) ? "on":"off");
	Wakeup(intPayload);

  } else if (strSuffix == "socmin") {
	if (intPayload >= 5 && intPayload <= 50) {	// 5 < Soc < 50
	  Settings.data.SoCmin = intPayload;
	}
	Serial.print(Settings.data.SoCmin); Serial.println('%');
	mqttClient.publish(fullTopic(topicBuffer, "stat/BAT/socmin"), int_str(value, Settings.data.SoCmin), false);

  } else if (strSuffix == "socmax") {
	if (intPayload >= 80 && intPayload <= 100) { // 80 < Soc < 100
	  Settings.data.SoCmax = intPayload;
	}
	Serial.print(Settings.data.SoCmax); Serial.println('%');
	mqttClient.publish(fullTopic(topicBuffer, "stat/BAT/socmax"), int_str(value, Settings.data.SoCmax), false);

//---------------------------- Tests
  } else if (strSuffix == "debug") {				// set debug level and some tests
	Serial.println(intPayload);
    if (intPayload < 10) {
	  const byte DebugMask[6] = { 0,
				DBG_BLE1 | DBG_BLE3,				// 1: bms requests
				DBG_BLE1 | DBG_BLE2,				// 2: with bms rx packets hexdump
				DBG_BLE1 | DBG_BLE2 | DBG_BLE3,		// 3:
				DBG_MQTT | DBG_BLE3,				// 4: with bms tx packets (mosfet and balance control)
				DBG_WEB  | DBG_BLE3 };				// 5: web sockets

	  JbdBms::debugLevel = DebugMask[intPayload];
	  mqttClient.publish(fullTopic(topicBuffer, "stat/ESP/debug"), int_str(value, intPayload), false);
	}
	else if (intPayload == 71) vccErr = !vccErr;			// test Vcc error
	else if (intPayload == 72) {							// test bms error
	  bmsErr = !bmsErr;
	  Bms.basicData.Protection = (bmsErr) ? 4:0;
	}
	else if (intPayload == 86) Settings.data.mqttJson = !Settings.data.mqttJson; // toggle json
	else if (intPayload == 87) Settings.data.darkMode = !Settings.data.darkMode; // toggle darkmode

	else if (intPayload == 80) Settings.relay.enable = !Settings.relay.enable;
	else if (intPayload == 81) Settings.relay.invert = !Settings.relay.invert;
	else if (intPayload == 82) Settings.relay.function = intPayload;
	else if (intPayload == 83) {					// 13 = 
	}
	else if (intPayload == 88) strcpy(Settings.data.httpPass, ""); // reset password
	else if (intPayload == 89) strcpy(Settings.data.httpPass, "pass"); // set password

	else if (intPayload == 90) Settings.print();			// display settings
	else if (intPayload == 91) {
	  Serial.println(); Serial.println(htmlProcessor(String("header"))); // display header var
	} else if (intPayload == 92) {
	  Serial.println(); Serial.println(htmlProcessor(String("footer"))); // display footer var
	} else if (intPayload == 93) {
	  Serial.println(); Serial.println(jsonRequest());		// display life json string
	} else if (intPayload == 94) {
	  Serial.println(); serializeJson(bmsJson, Serial);	// display WS json string
	} else if (intPayload == 99) {							// 99 = save current settings!
	  Settings.save();
	  Settings.load();
	  Settings.print();
	}
	digitalWrite(MQTT_LED, LOW);
  } else if (strSuffix == "restart") {
	restart = true;
	restartTime = millis();
  } else Serial.print("?\n");
}
//===================================== MQTT Setup and Configuration

void setupMQTTClient()
{
  pinMode(MQTT_LED, OUTPUT);
  digitalWrite(MQTT_LED, LOW);
  refresh = Settings.data.mqttRefresh * 1000;	// set mqtt refresh rate in ms
//mqttClient.setServer(MQTT_SERVER, MQTT_PORT);
  mqttClient.setServer(Settings.data.mqttServer, Settings.data.mqttPort);
  mqttClient.setKeepAlive(15);					// default 15s
  mqttClient.setBufferSize(256);				// default 256Bytes
  mqttClient.setCallback(mqttCallback);			// set mqtt callback function
  mqttErr = 0;
//print_Setupdone("MQTT");
}

//===================================== MQTT Process

bool connectMQTT()
{
  char bmsNum = Bms.data.Alias[strlen(Bms.data.Alias) - 1]; // copy bms number from alias to topic
  if (bmsNum == '0') return false;		// BMS pairing failed?
  Settings.data.mqttTopic[strlen(Settings.data.mqttTopic) - 1] = bmsNum; // set root topic
  Serial.println(fullTopic(topicBuffer, "stat/test"));
  String clientId = Bms.data.Alias + String(random(0xffff), HEX); // connect and publish last will
  if (mqttClient.connect(clientId.c_str(), Settings.data.mqttUser, Settings.data.mqttPass, fullTopic(topicBuffer, "tele/LWT"), 1, true, "offline")) {
	digitalWrite(MQTT_LED, HIGH);
	mqttErr = 0;
	mqttClient.setServer(Settings.data.mqttServer, Settings.data.mqttPort);
	Serial.print(F("connected to MQTT server: ")); Serial.print(Settings.data.mqttServer);
	Serial.print(':'); Serial.println(Settings.data.mqttPort);
	mqttClient.publish(fullTopic(topicBuffer, "tele/LWT"), "online", true); // LWT online message must be retained!
	Serial.println(F("subscribing topics"));
	if (Settings.data.mqttJson) {			// subscribe json

	} else {								// classic mqtt subscription
#if true
	  static const char *const TopicList[11] = {
		"power","charge","discharge","relay","wakeup",
		"teleperiod","bmsperiod",
		"socmin","socmax",
		"debug","restart" };
	  const char *rTopic = Settings.data.mqttTopic;	// get the root topic ("/ESP32-BMSx")
	  for (auto i = 0; i < sizeof(TopicList) / sizeof(*TopicList); i++) {
	    const char *pTopic = TopicList[i];
		strcpy(topicBuffer, rTopic);		// root topic
		strcat(topicBuffer, "/cmnd/");
		strcat(topicBuffer, pTopic);
		mqttClient.subscribe(topicBuffer);
	  }
#else
	  mqttClient.subscribe(fullTopic(topicBuffer, "cmnd/power"));	// ... and resubscribe
	  mqttClient.subscribe(fullTopic(topicBuffer, "cmnd/charge"));
	  mqttClient.subscribe(fullTopic(topicBuffer, "cmnd/discharge"));
	  mqttClient.subscribe(fullTopic(topicBuffer, "cmnd/balance"));
	  mqttClient.subscribe(fullTopic(topicBuffer, "cmnd/relay"));
	  mqttClient.subscribe(fullTopic(topicBuffer, "cmnd/wakeup"));
	  mqttClient.subscribe(fullTopic(topicBuffer, "cmnd/teleperiod"));
	  mqttClient.subscribe(fullTopic(topicBuffer, "cmnd/bmsperiod"));
	  mqttClient.subscribe(fullTopic(topicBuffer, "cmnd/socmin"));
	  mqttClient.subscribe(fullTopic(topicBuffer, "cmnd/socmax"));
	  mqttClient.subscribe(fullTopic(topicBuffer, "cmnd/debug"));
	  mqttClient.subscribe(fullTopic(topicBuffer, "cmnd/restart"));
#endif
	}
	mqttPublishRelay();

	if (Settings.data.mqttTrigger[0] != 0) {
	  Serial.print(F("subscribing trigger: ")); Serial.print(Settings.data.mqttTrigger); Serial.println();
	  mqttClient.subscribe(Settings.data.mqttTrigger);
	}
	digitalWrite(MQTT_LED, LOW);
//	lastConnected = millis();
//	lastPublished = lastConnected; // wait with publishing!
	return true;
  } else {	// not connected
	digitalWrite(MQTT_LED, LOW);
	mqttErr++;
	firstPublish = false;
//	lastConnected = 0;		// force mqtt server reconnect
	Serial.println(F("failed to connect to MQTT server"));
	return false;
  }
}


void processMQTT() // like PylontechMonitoring, called in loop
{
//debugLevel = Bms.getDebugLevel();
  if (Settings.data.mqttServer[0] == 0) return; // MQTT not configured

  static ulong lastConnected = 0;		// connection to mqtt server in ms
  static ulong lastReconnected = 0;		// last reconnection to mqtt server in ms
  static ulong lastPublished = 0;		// last published message in ms

  ulong ms = millis();
  // first let's make sure we are connected to mqtt
  // check connection on startup and every min, if disconnected reconnect
  // only if BMS and MQTT client connected, roottopic will be ok
  if (!mqttClient.connected() && (lastConnected == 0 || ms - lastConnected > 5 * 60 * 1000)) { // check reconnect every 5 minutes
//	if (ms - lastReconnected > 5000) { // try all 5s
	Serial.println(F("lost MQTT connection, try to reconnect"));
	mqttClient.disconnect();
	delay(1000);
	setupMQTTClient();
	if (connectMQTT()) {
	  lastConnected = ms;
	  lastPublished = ms;				// wait with publishing!
	} else {
	  lastConnected = 0;				// force mqtt server reconnect
	  delay(1000);
	}
  }
  if (ms - lastPublished > 2 * 60 * 1000 || mqttErr > 20) { // reboot after 2min
//	mqttErr = 0;
	Serial.print(F("\nMQTT disconnected over allowed limit, ")); Serial.println(mqttErr);
	mqttErr = 0;
	restart = true;
	restartTime = ms - 500;
	return;
  }
  // next: read data from battery and send via MQTT (but only once per TELE_PERIOD seconds)
  if (mqttClient.connected() && ms - lastPublished >= refresh) { // time for new publish?
	lastPublished = ms;
	// check bms connection, bms request loop activ and bms data valid for some cycles
	if (Bms.connected && JbdBms::bmsCmd != PAUSE && JbdBms::dataOk > 5) { // BMS connected, requests activ and data ok?
	  if (JbdBms::debugLevel & DBG_WEB) Serial.println("*"); // "publish"

	  static basicDataStruct oldbasicData;		// this is the last state we sent to MQTT, 
	  static cellDataStruct oldcellData;		// used to prevent sending the same data over and over again
	  static unsigned int publishCnt = 0;

	  digitalWrite(MQTT_LED, HIGH);
	  bool forceUpdate = (publishCnt % 10 == 0); // publish all data every 10th call
	  mqttPublishBasicData(oldbasicData, forceUpdate);
	  mqttPublishCellData(oldcellData, forceUpdate);
	  if (forceUpdate) {
		mqttPublishESPData();					// also publish ESP and BMS data
		mqttPublishBMSData();
		getJsonDevice();						// new json publish
		getJsonData();
		getJsonRelay();
		mqttClient.beginPublish(fullTopic(topicBuffer, "tele"), measureJson(bmsJson), false);
		serializeJson(bmsJson, mqttClient);
		mqttClient.endPublish();
	  }
	  digitalWrite(MQTT_LED, LOW);
	  publishCnt++;
	  // update old basic and cell data
	  memcpy(&oldbasicData, &Bms.basicData, sizeof(basicDataStruct));
	  memcpy(&oldcellData, &Bms.cellData, sizeof(cellDataStruct));
	  firstPublish = true;
	} else if (Bms.connected && Bms.bmsCmd == PAUSE) {
	  mqttClient.publish(fullTopic(topicBuffer, "stat/BMS/state"), "paired", false);
	  mqttPublishESPData();						// publish ESP and BMS data
	  mqttPublishBMSData();

	} else if (!Bms.connected) {
	  mqttClient.publish(fullTopic(topicBuffer, "stat/BMS/state"), "disconnected", false);
	  mqttPublishESPData();						// publish ESP data
	  Bms.setup();
	} else
	  mqttClient.publish(fullTopic(topicBuffer, "stat/BMS/state"), "connected", false);
  } // no MQTT connection
  mqttClient.loop();					// check if we have something to read from MQTT
}

#endif /* __MQTT_CPP__ */
