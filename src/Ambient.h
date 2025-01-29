//#define ENABLE_AMBIENT		// Ambient service

#ifdef ENABLE_AMBIENT			// Ambient service
#include <Ambient.h>

unsigned int channelId = 1234;
String writeKey = "xxxxxxxxxxxxxx";
unsigned long lastAmbient = 0;
unsigned int ambientRefreshBase = 60 * 1000; // ms
unsigned int ambientRefresh = ambientRefreshBase;

WiFiClient ambientClient;			// WiFi client
Ambient ambient;
#endif

// ### loading configuration from config file ###
#ifdef ENABLE_AMBIENT
	  intVal = jsonConfig["ambient"]["channelId"];
	  if (intVal) channelId = intVal;
	  strVal = jsonConfig["ambient"]["writeKey"];
	  if (strVal) writeKey = strVal;
	  strVal = jsonConfig["ambient"]["refresh"];
	  if (strVal) ambientRefreshBase = intVal;			// in s
#endif

#ifdef ENABLE_AMBIENT
Setup
	// ### setup ambient
	ambient.begin(channelId, writeKey.c_str(), &ambientClient);	// init ambient channelID and key
	Serial.print("Ambient"); print_Setupdone();
#endif

#ifdef ENABLE_AMBIENT
Loop
 #ifdef ENABLE_AMBIENT
Sleep mode
	ambientRefresh = ambientRefreshBase * 10000;
 #endif
 #ifdef ENABLE_AMBIENT
	ambientRefresh = ambientRefreshBase * 1000;
 #endif
  if (millis() - lastAmbient >= ambientRefresh) {
	if (!WiFi.isConnected()) wifiConnect();
	ambient.set(1, Bms.basicData.Volts / 1000.0f);
	ambient.set(2, Bms.basicData.Amps / 1000.0f);
	ambient.set(3, Bms.cellData.CellDiff / 1.0f);
	ambient.set(4, (Bms.basicData.Temp1 + Bms.basicData.Temp2) / 2 / 10.0f);
	ambient.send();
	lastAmbient = millis();
  }
#endif
