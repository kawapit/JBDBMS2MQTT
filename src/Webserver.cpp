/*
JbdBMS2MQTT Project
https://github.com/yash-it/JbdBMS2MQTT

For simpler handling websites are placed in spiffs and can be uploaded and deleted per webui.

Credits:

 based on softwarecrash: Daly2MQTT https://github.com/softwarecrash/Daly2MQTT
 
 included life view from JunYama: https://github.com/junyama/jbdBMS-BLE-ESP32
 who cuts code from: https://github.com/kolins-cz/Smart-BMS-Bluetooth-ESP32 and customized it for WebUI.

 included Spiffs file upload and delete from: https://github.com/har-in-air/ESP32_ASYNC_WEB_SERVER_SPIFFS_OTA
 which is a mashup of code from the following repositories, plus OTA firmware update feature
	https://github.com/smford/esp32-asyncwebserver-fileupload-example
	https://randomnerdtutorials.com/esp32-web-server-spiffs-spi-flash-file-system/
*/

#ifndef __WEBSERVER_CPP__
#define __WEBSERVER_CPP__

#include "WebServer.h"
#ifdef ENABLE_OTA
#include <ArduinoOTA.h>
#endif
//--------------------------------

// global variables:
//AsyncWebServer server(HTTP_PORT);		// Web server instance (port cannot be changed!)
AsyncWebServer *server = NULL;			// webserver (server is a pointer to the AsyncWebServer object)
//DNSServer dns;
//#ifdef ENABLE_SOCKETS
AsyncWebSocket socket("/ws");			// sockets instance
AsyncWebSocketClient *socketClient;
//#endif
static File SpiffsFile;
char headerTemplate[2048] = {'\0'};	// reading /header.html 2038 bytes
char footerTemplate[1580] = {'\0'};	// reading /footer.html 1505 bytes
// function prototypes:
void URLnotfound(AsyncWebServerRequest *request);
bool authenticated(AsyncWebServerRequest *request);

//===================================== Webserver Functions
// make size of files human readable
// source: https://github.com/CelliesProjects/minimalUploadAuthESP32
String fileSize(const size_t bytes)
{
  if (bytes < 1024) return String(bytes) + "Bytes";
  else if (bytes < (1024 * 1024)) return String(bytes / 1024.0) + "kB";
  else if (bytes < (1024 * 1024 * 1024)) return String(bytes / 1024.0 / 1024.0) + "MB";
  else return String(bytes / 1024.0 / 1024.0 / 1024.0) + "GB";
}

// list all of the files, if ishtml=true, return html rather than simple text
String directoryString(bool ishtml)		// also called from spiffs.html
{
  String returnText = "";
  File root = SPIFFS.open("/");
  File foundfile = root.openNextFile();
  if (ishtml) {
	returnText += "<table align='center'><tr><th align='left'>Name</th><th align='left'>Size</th><th></th><th></th></tr>";
  } else Serial.println(F(">> Files on SPIFFS <<"));
  while (foundfile) {
	if (ishtml) {
	  returnText += "<tr align='left'><td>" + String(foundfile.name()) + "</td><td>" + fileSize(foundfile.size()) + "</td>";
	  returnText += "<td><button class='btn btn-primary' onclick=\"DirectoryButtonHandler(\'" + String(foundfile.name()) + "\', \'download\')\">Download</button>";
	  returnText += "<td><button class='btn btn-primary' onclick=\"DirectoryButtonHandler(\'" + String(foundfile.name()) + "\', \'delete\')\">Delete</button></tr>";
	} else {
	  returnText += "F:/" + String(foundfile.name()) + " size: " + fileSize(foundfile.size()) + "\n";
	}
	foundfile = root.openNextFile();
  }
  if (ishtml) returnText += "</table>";
  root.close();
  foundfile.close();
  return returnText;
}


void readTemplate(char *array, const char *fileName) // save template file into array
{
//String fileName = "/";
//fileName += name;
  File fileHandle = SPIFFS.open(fileName, "r");
  Serial.print(F("reading ")); Serial.print(fileName);
  if (!fileHandle) {
	Serial.println(F(" not found!"));
	return;
  }
  uint16_t i = 0;
  while (fileHandle.available()) {
	array[i] = fileHandle.read();
#ifdef DEBUG
//	Serial.print(array[i]);			// use for debug
#endif
	i++;
  }
  array[i] ='\0'; // ETX
//Serial.print(array);				// use for debug
  fileHandle.close();
  Serial.print(' '); Serial.print(i); Serial.println("bytes");
  return;
}

/*
void jsonRequest(AsyncWebServerRequest *request)
{
  String JSON = F("[");
  for (int i = 0; i < num_keywords; i++) {
	JSON += "{\"id\":\"" + String(keywords[i]) + "\", \"value\":\"" + String(value[i]) + "\"},";
  }
  JSON += "{\"id\":\"end\", \"value\":\"end\"}";
  JSON += "]";
  request->send(200, "text/json", JSON);
}
*/
//-----------------------------------
/*
{"status":1,"temp":18.20,"temp1":18.60,"temp2":18.00,"temp3":18.00,"soc":43,"socAh":99.85,"current":0.00,"cycles":1,"voltage":26.31,
"mosfet":{"charge":1,"discharge": 1},
"cellVoltages":[3.29,3.29,3.29,3.29,3.29,3.29,3.29,3.29],
"cellMin":3.29,"cellMax":3.29,"cellDiff":0.00,"iMin":4,"iMax":1,
"cellBalances":[0,0,0,0,0,0,0,0],"cellMedian":3.289,"balance":0,"connected":1}
*/
String jsonRequest() // requested from website life.html
{
  String jsonStr = "";
  jsonStr.reserve(300);
  byte cells = Bms.basicData.NumOfCells;
  bool cellBalanceList[NUMCELLS];
//jsonStr += "{\"espVcc\":"; jsonStr += String((ESP.getVcc() / 1000.0) + 0.3);
  jsonStr += "{\"espVcc\":"; jsonStr += String("3.3");
  jsonStr += ",\"state\":"; jsonStr += String(Bms.data.State);
  jsonStr += ",\"connected\":"; jsonStr += String(Bms.connected);
  jsonStr += ",\"temp\":"; jsonStr += String(Bms.basicData.Temp[0]); // in 0.1deg
  jsonStr += ",\"temp1\":"; jsonStr += String(Bms.basicData.Temp[1]);
  if (Bms.basicData.NumOfTemp > 1)
	jsonStr += ",\"temp2\":"; jsonStr += String(Bms.basicData.Temp[2]);
  if (Bms.basicData.NumOfTemp > 2)
	jsonStr += ",\"temp3\":"; jsonStr += String(Bms.basicData.Temp[3]);
  jsonStr += ",\"soc\":"; jsonStr += String(Bms.basicData.SoC);
  jsonStr += ",\"socAh\":"; jsonStr += String(Bms.basicData.SoC_Ah / 100.0); // remaining energy
  jsonStr += ",\"current\":"; jsonStr += String(Bms.basicData.Amps);
  jsonStr += ",\"voltage\":"; jsonStr += String(Bms.basicData.Volts);
  jsonStr += ",\"cycles\":"; jsonStr += String(Bms.basicData.Cycles);
  jsonStr += ",\"mosfet\":{\"charge\":"; chargeState = Bms.basicData.MosFet & 1; // Bit 0
  jsonStr += String(chargeState);
  jsonStr += ",\"discharge\":"; dischargeState = Bms.basicData.MosFet & 1<<1; // Bit 1
  jsonStr += String(dischargeState);
  jsonStr += "},\"cellVoltages\":[";
  jsonStr += String(Bms.cellData.CellVolt[0]);
  for (byte c = 1; c < cells; c++) {
	jsonStr += ",";
	jsonStr += String(Bms.cellData.CellVolt[c]);	// in mV
  }
  jsonStr += "]";
  jsonStr += ",\"cellMin\":"; jsonStr += String(Bms.cellData.CellMin);
  jsonStr += ",\"cellMax\":"; jsonStr += String(Bms.cellData.CellMax);  
  jsonStr += ",\"cellDiff\":"; jsonStr += String(Bms.cellData.CellDiff);
  jsonStr += ",\"iMin\":"; jsonStr += String(Bms.cellData.iMin);
  jsonStr += ",\"iMax\":"; jsonStr += String(Bms.cellData.iMax);
  for (byte c = 0; c < cells; c++) {
	cellBalanceList[c] = Bms.basicData.BalanceCodeLow & 1<<c;
  }
  jsonStr += ",\"cellBalances\":["; jsonStr += String(cellBalanceList[0]);
  for (byte c = 1; c < cells; c++) {
	jsonStr += ",";
	jsonStr += String(cellBalanceList[c]);
  }
  jsonStr += "]";
  jsonStr += ",\"cellMedian\":"; jsonStr += String(Bms.cellData.CellMedian);
  jsonStr += ",\"balance\":"; jsonStr += String((Bms.basicData.BalanceCodeHigh == 0) ? 0:1);
  jsonStr += "}";
#ifdef DEBUG
//Serial.println(jsonStr);
#endif
  return jsonStr;
//failCodes
}

//===================================== Template Processor

String htmlProcessor(const String &var) // replace %SOMETHING% in webpage with dynamically generated string
{
 // (strings saved in flash memory)
  if (var == F("timestamp"))		return String(__DATE__) + " " + String(__TIME__); 
  else if (var == F("free"))		return fileSize((SPIFFS.totalBytes() - SPIFFS.usedBytes()));
  else if (var == F("used"))		return fileSize(SPIFFS.usedBytes());
  else if (var == F("total"))		return fileSize(SPIFFS.totalBytes());
  else if (var == F("flashSize"))	return String(ESP.getFreeSketchSpace()).c_str();
#if false
  else if (var == F("header"))		return (FPSTR(header));			// stored in flash
  else if (var == F("footer"))		return (FPSTR(footer));
#else
  else if (var == F("header"))		return String(headerTemplate).c_str(); // stored in memory
  else if (var == F("footer"))		return String(footerTemplate).c_str();
#endif
  else if (var == F("softwareVersion")) return (SOFTWARE_VERSION);
  else if (var == F("swVersion"))	return (SWVERSION);
#ifndef WEBSERIAL
  else if (var == F("webserial"))	return "display:none;"; // supress Webserial button in settings.html
#endif
  else if (var == F("bmsAlias"))	return Bms.data.Alias;
//else if (var == F("esp01"))		return String(ESP01).c_str(); // for ESP01 "display:none;"
  else if (var == F("debug"))		return String(JbdBms::debugLevel).c_str();
  else if (var == F("wakeupPin"))	return String(WAKEUP_PIN).c_str();
  else if (var == F("relayPin"))	return String(RELAY_PIN).c_str();
  else if (var == F("deviceName")) return Settings.data.deviceName;
  else if (var == F("httpPort"))	return String(Settings.data.httpPort).c_str();
  else if (var == F("httpUser"))	return Settings.data.httpUser;
  else if (var == F("httpPass"))	return Settings.data.httpPass;
  else if (var == F("mqttServer")) return Settings.data.mqttServer;
  else if (var == F("mqttPort"))	return String(Settings.data.mqttPort).c_str();
  else if (var == F("mqttUser"))	return Settings.data.mqttUser;
  else if (var == F("mqttPass"))	return Settings.data.mqttPass;
  else if (var == F("mqttTopic"))	return Settings.data.mqttTopic;
  else if (var == F("mqttRefresh")) return String(Settings.data.mqttRefresh).c_str();
  else if (var == F("mqttJson"))	return Settings.data.mqttJson ? "checked":"";
  else if (var == F("bmsRefresh"))	return String(Settings.data.bmsRefresh).c_str();
  else if (var == F("wakeup"))		return Settings.data.wakeupEnable ? "checked":"";
  
  else if (var == F("relayEnable"))	return Settings.relay.enable ? "checked":"";
  else if (var == F("relayInvert"))	return Settings.relay.invert ? "checked":"";
  else if (var == F("relayFailsave"))	return Settings.relay.failsafe ? "checked":"";
  else if (var == F("relayValue"))		return String(Settings.relay.value, 3).c_str();
  else if (var == F("relayHysteresis")) return String(Settings.relay.hysteresis, 3).c_str();
  else if (var == F("relayFunction"))	return String(Settings.relay.function).c_str();
  else if (var == F("relayCompare"))	return String(Settings.relay.compare).c_str();
  else if (var == F("mqttTrigger"))	return Settings.data.mqttTrigger;
  else if (var == F("sleepVoltage"))	return String(Settings.data.sleepVoltage).c_str();
  else if (var == F("wakeupVoltage"))	return String(Settings.data.wakeupVoltage).c_str();
  else if (var == F("socMin"))			return String(Settings.data.SoCmin).c_str();
  else if (var == F("socMax"))			return String(Settings.data.SoCmax).c_str();
  else if (var == F("theme"))			return Settings.data.darkMode ? "dark":"light";
  else if (var == F("darkMode"))		return Settings.data.darkMode ? "checked":"";
  else if (var == F("haDiscovery"))	return Settings.data.haDiscovery ? "checked":"";
  else // return something, else this if then else will crash in case calles without VAR set....
  return String("");
}

/*
void RootRequest(AsyncWebServerRequest *request)
{
  request->send(SPIFFS, "/bms.html", String(), false, htmlProcessor); // send main.htm with template processor function
}
*/

void URLnotfound(AsyncWebServerRequest *request)
{
#if false
  String message = "file not found\n";
  message += "URI: "+ request->url();
  message += "\nMethod: " + (request->method() == HTTP_GET) ? "GET":"POST";
  message += "\nArguments: " + request->args() + "\n"; 
  for (uint8_t i = 0; i < request->args(); i++) {
    message += " " + request->argName(i) + ": " + request->arg(i) + "\n";
  }
  request->send(404, "text/plain", message);
#else
  Serial.println("Client:" + request->client()->remoteIP().toString() + " " + request->url());
  request->send(404, "text/plain", "not found");
//request->send(418, "text/plain", "418 I'm a teapot");
#endif
}

// used by server->on functions to discern whether a user has the correct httpapitoken OR is authenticated by username and password
bool authenticated(AsyncWebServerRequest *request)
{
  const char *userName = Settings.data.httpUser;
  const char *userPass = Settings.data.httpPass;
  bool result = false;
//String logmessage = "client: " + request->client()->remoteIP().toString() + ' ' + request->url();
  if (Settings.data.httpUser[0] == 0 || Settings.data.httpPass[0] == 0) result = true; // no password
//else if (request->authenticate(Settings.data.httpUser, Settings.data.httpPass)) {
  else if (request->authenticate(userName, userPass)) {
//	Serial.println("is authenticated via username and password");
//	logmessage += " auth: success";
	result = true;
//} else {
//	logmessage += " auth: failed";
  }
//Serial.println(logmessage);
  return result;
}

/*
void uploadFile(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final)
{
  if (!index) request->_tempFile = SPIFFS.open("/" + filename, "w");
  if (len) request->_tempFile.write(data, len);
  if (final) {
	request->_tempFile.close();
	request->redirect("/files");
  }
}
*/
// handles non .bin file uploads to the SPIFFS directory
void uploadFile(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final)
{
  if (!authenticated(request))			// make sure authenticated before allowing upload
	request->requestAuthentication();
  if (!index) { // open the file on first call and store the file handle in the request object
	request->_tempFile = SPIFFS.open("/" + filename, "w");
	Serial.println("upload start: " + String(filename));
  }
  if (len) {							// stream the incoming chunk to the opened file
	request->_tempFile.write(data, len);
	Serial.println("writing file: " + String(filename) + " index=" + String(index) + " len=" + String(len));
  }
  if (final) {							// close the file handle as the upload is now done
	request->_tempFile.close();
	Serial.println("upload complete: " + String(filename) + ", size: " + String(index + len));
	request->redirect("/settings");
  }
}

// handles OTA firmware update
void updateFirmware(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final)
{
  if (!authenticated(request))					// make sure authenticated before allowing upload
	return request->requestAuthentication();
  if (!index) {
	Serial.println("OTA update start: " + String(filename));
	if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {	// start with max available size
	  Update.printError(Serial);
	}
  }
  if (len) {
	if (Update.write(data, len) != len) {		// flashing firmware to ESP
	  Update.printError(Serial);
	}      
	Serial.println("writing file: " + String(filename) + " index=" + String(index) + " len=" + String(len));
  }
  if (final) {
	if (Update.end(true)) {					// true to set the size to the current progress
	  Serial.println("OTA complete: " + String(filename) + ", size: " + String(index + len));
	} else {
	  Update.printError(Serial);
	}
	request->redirect("/");
  }
}


void FileUpload(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final)
{
  if (filename.endsWith(".bin") ) {
	updateFirmware(request, filename, index, data, len, final);
  } else {
	uploadFile(request, filename, index, data, len, final);
  }
}


void requestBody(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total)
{
  if (!index) Serial.printf("Bodystart: %u\n", total);
  Serial.printf("%s", (const char*)data);
  if (index + len == total) Serial.printf("Bodyend: %u\n", total);
}


int readFile_chunked(uint8_t *buffer, int maxLen)
{
//Serial.printf("MaxLen=%d\n", maxLen);
  if (!SpiffsFile.available()) {
	SpiffsFile.close();
	return 0;
  } else {
	int count = 0;
	while (SpiffsFile.available() && (count < maxLen)) {
	  buffer[count] = SpiffsFile.read();
	  count++;
	}
	return count;
  }
}

//------------------------------ Sockets

static bool newws = false;		// flag for new ws connection

void notifyClients()
{
  if (socketClient != nullptr && socketClient->canSend()) {
	bool debug = (JbdBms::debugLevel & DBG_WEB) != 0;
	if (debug && newws) Serial.print(F("sending data to WS")); // print only once
	newws = false;
	size_t len = measureJson(bmsJson);
	AsyncWebSocketMessageBuffer *buffer = socket.makeBuffer(len);
	if (buffer) {
	  serializeJson(bmsJson, (char *)buffer->get(), len + 1);
	  socketClient->text(buffer);
	}
//	if (debug) Serial.println(F("done"));
	if (debug) Serial.print('.');		// "socket", for each json packet
  }
}


void handleWebSocketMessage(void *arg, uint8_t *data, size_t len)
{
//typedef enum { WS_CONTINUATION, WS_TEXT, WS_BINARY, WS_DISCONNECT = 0x08, WS_PING, WS_PONG } AwsFrameType;
 AwsFrameInfo *info = (AwsFrameInfo*)arg;
  if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
	// the whole message is in a single frame and we got all of it's data
	data[len] = 0;
	if (strcmp((char*)data, "ping") != 0) {
//	  updateProgress = true;
	  bool debug = (JbdBms::debugLevel & DBG_WEB) != 0;
	  if (debug) Serial.printf("received WS-%s-message (%lluBytes): %s\n", (info->opcode == WS_TEXT) ? "text":"binary", info->len, (char*)data);
	  if (strcmp((char *)data, "wakebms") == 0) Wakeup(true);
	  else if (strcmp((char *)data, "charge_on") == 0)		Bms.setCharge(true);
	  else if (strcmp((char *)data, "charge_off") == 0)	Bms.setCharge(false);
	  else if (strcmp((char *)data, "discharge_on") == 0)	Bms.setDischarge(true);
	  else if (strcmp((char *)data, "discharge_off") == 0)	Bms.setDischarge(false);
	  else if (strcmp((char *)data, "balance_on") == 0)	Bms.setBalanceEnable(true);
	  else if (strcmp((char *)data, "balance_off") == 0)	Bms.setBalanceEnable(false);
	  else if (strcmp((char *)data, "relay_on") == 0)		relay = true;
	  else if (strcmp((char *)data, "relay_off") == 0)		relay = false;
	  else if (strcmp((char *)data, "bms_on") == 0)  Bms.setBMS(true);
	  else if (strcmp((char *)data, "bms_off") == 0) Bms.setBMS(false);
//	  mqtttimer = (Settings.data.mqttRefresh * 1000) * (-1); ???
	}
//	updateProgress = false;
  }
}

void processWebserver()
{
//socket.cleanupClients();			// clean unused client connections
//server->handleClient();			// ???
  getJsonDevice();
  getJsonData();
  getJsonRelay();
  notifyClients();
}


void cleanupClients() // called in main loop
{
  socket.cleanupClients();			// clean unused client connections
}


void onSocketEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len)
{
//typedef enum { WS_EVT_CONNECT, WS_EVT_DISCONNECT, WS_EVT_PONG, WS_EVT_ERROR, WS_EVT_DATA } AwsEventType;

  bool debug = (JbdBms::debugLevel & DBG_WEB) != 0;
  if (debug && (type != 4)) Serial.printf("WS client%u ", client->id());
  if (type == WS_EVT_CONNECT) {			// client connected
	socketClient = client;
	if (debug) Serial.println(F("connected"));
    client->ping();
	newws = true;
	processWebserver();					// send current data to client
  } else if (type == WS_EVT_DISCONNECT) {
	socketClient = nullptr;
	socket.cleanupClients();
	if (debug) Serial.println(F("disconnected"));
  } else if (type == WS_EVT_DATA) {	// data received ?
	handleWebSocketMessage(arg, data, len);

  } else if (type == WS_EVT_PONG) {	// pong message was received in response to a ping request maybe
	if (debug) Serial.printf("pong%u: %s\n", len, (len) ? (char*)data:"");
	newws = true;
  } else if (type == WS_EVT_ERROR) {	// error was received from the other end
	socketClient = nullptr;
	socket.cleanupClients();
	if (debug) Serial.printf("error%u: %s\n", *((uint16_t*)arg), (char*)data);
  }
//else if (debug) Serial.println(type, HEX); // "4"
}

 #ifdef ENABLE_OTA
//===================================== OTA Setup

void setupOTA()
{
  // ### setup OTA ###
//ArduinoOTA.setPort(3232);// Port defaults to 3232
  ArduinoOTA.setHostname("ESP32-BMS");		// Hostname defaults to ESP32-BMS-[MAC]
//ArduinoOTA.setPassword(Settings.data.httpPassword);// no authentication by default
  // password can be set with it's md5 value as well
  // MD5(admin) = 21232f297a57a5a743894a0e4a801fc3
//ArduinoOTA.setPasswordHash("21232f297a57a5a743894a0e4a801fc3");
  ArduinoOTA
	.onStart([]() {
	  String type;
	  if (ArduinoOTA.getCommand() == U_FLASH)
		type = "sketch";
	  else // U_SPIFFS
		type = "filesystem";
	  // if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
	  Serial.println("start updating " + type);
	})
	.onEnd([]() {
	  Serial.print("\nend\n");
	})
	.onProgress([](unsigned int progress, unsigned int total) {
	  Serial.printf("progress: %u%%\r", (progress / (total / 100)));
	})
	.onError([](ota_error_t error) {
	  Serial.printf("error[%u]: ", error);
	  if (error == OTA_AUTH_ERROR) Serial.print("auth");
	  else if (error == OTA_BEGIN_ERROR) Serial.print("begin");
	  else if (error == OTA_CONNECT_ERROR) Serial.print("connect");
	  else if (error == OTA_RECEIVE_ERROR) Serial.print("receive");
	  else if (error == OTA_END_ERROR) Serial.print("end");
	  Serial.println(" failed");
	});
  ArduinoOTA.begin();
}
#endif

//===================================== Webserver Setup and Configuration

void setupWebserver()
{
  if (!MDNS.begin(Settings.data.deviceName)) { // use http://esp32.local for web server page
	Serial.println(F("Error setting up MDNS responder!"));
	while (1) {
	  delay(1000);
	}
  }
  Serial.println(F("MDNS responder started"));

  // *** webserver setup ***
  Serial.println(F("configuring Webserver..."));
  server = new AsyncWebServer(Settings.data.httpPort);	// new pointer to Web server instance
//EspHtmlTemplateProcessor templateProcessor(&server);	// obsolet
//AsyncEventSource events("/events");

  readTemplate(headerTemplate, "/" HEADER_FILE);		// read templates
  readTemplate(footerTemplate, "/" FOOTER_FILE);

  if (WiFi.status() == WL_CONNECTED) {
	if (!SPIFFS.begin(true)) {
	  Serial.println(F("Error mounting SPIFFS"));
	  return;
	}
	// ### setup webAPIs ###
//	String hash = base64(Settings.data.httpUser + ":" + Settings.data.httpPass);
	// Catch-All Handlers:
	// any request that can not find a Handler that canHandle it ends in the callbacks below:
	server->onNotFound(URLnotfound);		// if url isn't found
	server->onFileUpload(FileUpload);		// run FileUpload function when any file is uploaded
	server->onRequestBody(requestBody);
//	server->on("/", RootRequest); // for example
	server->on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
//	  send(FS &fs, const String& path, const String& contentType=String(), bool download=false, AwsTemplateProcessor callback=nullptr);
//	  AsyncWebServerResponse *response = request->send(SPIFFS, "/main_t.html", String(), false, htmlProcessor); // error: void value not ignored as it ought to be ???
//	  if (!authenticated(request)) return request->requestAuthentication();
//	  AsyncWebServerResponse *response = request->beginResponse_P(200, "text/html", bms, htmlProcessor);
//	  request->send(response);
//	  request->send_P(200, "text/html", bms, htmlProcessor); // send main.htm with template processor function
	  request->send(SPIFFS, "/bms.html", String(), false, htmlProcessor); // send main.htm with template processor function
	});
	// from https://github.com/har-in-air/ESP32_ASYNC_WEB_SERVER_SPIFFS_OTA
	server->on("/spiffs", HTTP_GET, [](AsyncWebServerRequest *request) {
	  if (!authenticated(request)) return request->requestAuthentication();
	  request->send(SPIFFS, "/spiffs.html", String(), false, htmlProcessor);
	});
	server->on("/life", HTTP_GET, [](AsyncWebServerRequest *request) {
//	  request->send(SPIFFS, "/life.html", "text/html");			// without template processor
//	  AsyncWebServerResponse *response = request->beginResponse_P(200, "text/html", life);
//	  request->send(response);
	  request->send(SPIFFS, "/life.html",String(), false, htmlProcessor);
	});
//	server->on("/footer.js", HTTP_GET, [](AsyncWebServerRequest *request) {
//	  request->send(SPIFFS, "/footer.js", "text/javascript");
//	});
	// http://ESP32-BMS2/file name life.css
	server->on("/life.css", HTTP_GET, [](AsyncWebServerRequest *request) {
	  request->send(SPIFFS, "/life.css", "text/css");			// route to load life.css file
	});
	server->on("/style.css", HTTP_GET, [](AsyncWebServerRequest *request) { // stylesheet for bms.html
	  request->send(SPIFFS, "/style.css", "text/css");			// route to load style.css file
	});
	server->on("/favicon.ico", HTTP_GET, [](AsyncWebServerRequest *request) {
	  request->send(SPIFFS, "/favicon.ico");
	}); 
	server->on("/justgage/raphael.min.js", HTTP_GET, [](AsyncWebServerRequest *request) {
	  request->send(SPIFFS, "/raphael.min.js", "text/javascript");
	});
	server->on("/justgage/justgage.js", HTTP_GET, [](AsyncWebServerRequest *request) {
	  request->send(SPIFFS, "/justgage.js", "text/javascript");
	});
	server->on("/log", HTTP_GET, [](AsyncWebServerRequest *request) {
	  request->send(SPIFFS, "/log.txt");
	});
	server->on("/lifeJson", HTTP_GET, [](AsyncWebServerRequest *request) { // requested from life.html
	  request->send_P(200, "appication/json", jsonRequest().c_str());
/*
	  AsyncResponseStream *response = request->beginResponseStream("application/json");
	  serializeJson(bmsJson, *response);
	  request->send(response);
*/
	});
	server->on("/directory", HTTP_GET, [](AsyncWebServerRequest *request) { // called from spiffs.html
	  if (!authenticated(request)) return request->requestAuthentication();
	  request->send(200, "text/plain", directoryString(true)); // send directory as html
	});
	server->on("/file", HTTP_GET, [](AsyncWebServerRequest *request) {
	  if (!authenticated(request)) return request->requestAuthentication();
	  String logmessage = "client:" + request->client()->remoteIP().toString() + " " + request->url();
	  if (request->hasParam("name") && request->hasParam("action")) {
		const char *fileName = request->getParam("name")->value().c_str();
		const char *fileAction = request->getParam("action")->value().c_str();
		logmessage = "client: " + request->client()->remoteIP().toString() + " " + request->url() + "?name=" + String(fileName) + "&action=" + String(fileAction);
		if (!SPIFFS.exists(fileName)) {
		  Serial.println(logmessage + " ERROR: file does not exist");
		  request->send(400, "text/plain", "ERROR: file does not exist");
		} else {
		  Serial.println(logmessage + " file exists");
		  if (strcmp(fileAction, "download") == 0) {
			logmessage += " downloaded";
			SpiffsFile = SPIFFS.open(fileName, "r");
			int sizeBytes = SpiffsFile.size();
			Serial.println("large file, chunked download required");
			AsyncWebServerResponse *response = request->beginResponse("application/octet-stream", sizeBytes, [](uint8_t *buffer, size_t maxLen, size_t index) -> size_t {
			  return readFile_chunked(buffer, maxLen);
			});
			char szBuf[80];
			sprintf(szBuf, "attachment; filename=%s", &fileName[1]); // get past the leading '/'
			response->addHeader("Content-Disposition", szBuf);
			response->addHeader("Connection", "close");
			request->send(response);
		  } else if (strcmp(fileAction, "delete") == 0) {
			logmessage += " deleted";
			SPIFFS.remove(fileName);
			request->send(200, "text/plain", "deleted File: " + String(fileName));
		  } else {
			logmessage += " ERROR: invalid action param supplied";
			request->send(400, "text/plain", "ERROR: invalid action param supplied");
		  }
		  Serial.println(logmessage);
		}
	  } else {
		request->send(400, "text/plain", "ERROR: name and action params required");
	  }
	});
	server->on("/settings", HTTP_GET, [](AsyncWebServerRequest *request) {
	  if (!authenticated(request)) return request->requestAuthentication();
//	  AsyncWebServerResponse *response = request->beginResponse_P(200, "text/html", settings, htmlProcessor);
//	  request->send(response);
//	  request->send_P(200, "text/html", settings, htmlProcessor); // send settings.html with template processor function
	  request->send(SPIFFS, "/settings.html", String(), false, htmlProcessor);
	});
	server->on("/settingsedit", HTTP_GET, [](AsyncWebServerRequest *request) {
	  if (!authenticated(request)) return request->requestAuthentication();
	  request->send(SPIFFS, "/settings_edit.html", String(), false, htmlProcessor);
	});
	server->on("/settingssave", HTTP_POST, [](AsyncWebServerRequest *request) {
	  if (!authenticated(request)) return request->requestAuthentication(); // get data from webpace
	  strncpy(Settings.data.deviceName, request->arg("_deviceName").c_str(), 20);
	  Settings.data.httpPort = request->arg("_httpPort").toInt();
	  strncpy(Settings.data.httpUser, request->arg("_httpUser").c_str(), 20);
	  strncpy(Settings.data.httpPass, request->arg("_httpPass").c_str(), 20);
	  strncpy(Settings.data.mqttServer, request->arg("_mqttServer").c_str(), 20);
	  Settings.data.mqttPort = request->arg("_mqttPort").toInt();
	  strncpy(Settings.data.mqttUser, request->arg("_mqttUser").c_str(), 20);
	  strncpy(Settings.data.mqttPass, request->arg("_mqttPass").c_str(), 20);
	  strncpy(Settings.data.mqttTopic, request->arg("_mqttTopic").c_str(), 20);
	  Settings.data.mqttRefresh = request->arg("_mqttRefresh").toInt() < 5 ? 5:request->arg("_mqttRefresh").toInt(); // prevent lower numbers
	  Settings.data.mqttJson = (request->arg("_mqttJson") == "true") ? true:false;
	  strncpy(Settings.data.mqttTrigger, request->arg("_mqttTrigger").c_str(), 40);
	  Settings.data.bmsRefresh = request->arg("_bmsRefresh").toInt() < 5 ? 5:request->arg("_bmsRefresh").toInt();
//	  Settings.data.deviceQuantity = request->arg("_deviceQuanttity").toInt() <= 0 ? 1 : request->arg("_deviceQuanttity").toInt();
	  Settings.data.darkMode = (request->arg("_darkMode") == "true") ? true:false;
	  Settings.data.sleepVoltage = request->arg("_sleepVoltage").toInt();
	  Settings.data.wakeupVoltage = request->arg("_wakeupVoltage").toInt();
	  Settings.data.SoCmin = request->arg("_socMin").toInt();
	  Settings.data.SoCmax = request->arg("_socMax").toInt();
	  Settings.relay.enable = (request->arg("_relayEnable") == "true") ? true:false;
	  Settings.relay.invert = (request->arg("_relayInvert") == "true") ? true:false;
	  Settings.relay.failsafe = (request->arg("_relayFailsafe") == "true") ? true:false;
	  Settings.relay.enable = (request->arg("_relayEnable") == "true") ? true:false;
	  Settings.relay.function = request->arg("_relayFunction").toInt();
	  Settings.relay.compare = request->arg("_relayCompare").toInt();
	  Settings.relay.value = request->arg("_relayValue").toInt();
	  Settings.relay.hysteresis = request->arg("_relayHysteresis").toInt();
	  Settings.print();
	  Settings.save();
	  Settings.load();
	  request->redirect("/settingsedit");
//	  request->redirect("/reboot");
	});
	server->on("/set", HTTP_GET, [](AsyncWebServerRequest *request) {
	  if (!authenticated(request)) return request->requestAuthentication();
	  AsyncWebParameter *p = request->getParam(0);
	  int intVal = p->value().toInt();
	  Serial.print(p->name().c_str()); Serial.print(": "); Serial.println(intVal);
	  if (p->name() == "teleperiod") {				// in s
		if (intVal > 2 && intVal < 3600) {
		  Settings.data.mqttRefresh = intVal;
		  refresh = intVal * 1000;					// set mqtt refresh in ms
		}
		Settings.data.mqttJson = true;				// also enable json
		Serial.print(Settings.data.mqttRefresh); Serial.print("s\n");
#ifdef MQTT_ENABLE
		mqttClient.publish(fullTopic(topicBuffer, "stat/ESP/teleperiod"), String(Settings.data.mqttRefresh).c_str(), false);
#endif
	  } else if (p->name() == "bmsperiod") {		// in s
		if (intVal > 2 && intVal < 7200) {
		  Settings.data.bmsRefresh = intVal;
		  Bms.refresh = intVal * 1000;				// set bms refresh in ms
		}
		Bms.bmsCmd = CONNECT;						// also activate bms requests
		Serial.print(Bms.refresh / 1000); Serial.print("s\n");
#ifdef MQTT_ENABLE
		mqttClient.publish(fullTopic(topicBuffer, "stat/BMS/bmsperiod"), String(Bms.refresh / 1000).c_str(), false);
#endif
	  } else if (p->name() == "bmsreset") {
		Serial.print("reset BMS");
		if (intVal == 1) {
//		  Bms.setBmsReset();
		}
	  } else if (p->name() == "bmswakeup") {
	    if (intVal == 1) Wakeup(true);

	  } else if (p->name() == "charge") {
		Bms.setCharge(intVal);
	  } else if (p->name() == "discharge") {
		Bms.setDischarge(intVal);
	  } else if (p->name() == "balance") {
		if (intVal == 0 || intVal == 1) balance = intVal;
		else if (p->value() == "on") balance = true;
		else if (p->value() == "off") balance = false;
		Bms.setBalanceEnable(balance);
#ifdef MQTT_ENABLE
		mqttClient.publish(fullTopic(topicBuffer, "stat/BMS/balance"), (balance) ? "on":"off", false);
#endif
	  } else if (p->name() == "socmin") {
		if (intVal >= 0 && intVal <= 100) Settings.data.SoCmin = intVal;
	  } else if (p->name() == "socmax") {
		if (intVal >= 0 && intVal <= 100) Settings.data.SoCmax = intVal;
	  } else if (p->name() == "relay") {
		if (intVal == 0 || intVal == 1) relay = intVal;
		else if (p->value() == "on") relay = true;
		else if (p->value() == "off") relay = false;
//	  } else if (p->name() == "ha") HAtrigger = true;
	  }
	  request->send(200, "text/plain", "message received");
	});
	server->on("/connect", HTTP_GET, [](AsyncWebServerRequest *request) { // life.html
	  Bms.bmsCmd = CONNECT;
	  request->send_P(200, "text/plain", "connect OK");
	});
	server->on("/disconnect", HTTP_GET, [](AsyncWebServerRequest *request) {
	  Bms.bmsCmd = DISCONNECT;
	  request->send_P(200, "text/plain", "disconnect OK");
	});
	server->on("/confirmreset", HTTP_GET, [](AsyncWebServerRequest *request) {
	  if (!authenticated(request)) return request->requestAuthentication();
	  AsyncWebServerResponse *response = request->beginResponse(SPIFFS, "/confirm_reset.html", "text/html", false, htmlProcessor);
	  request->send(response);
	});
    server->on("/reset", HTTP_GET, [](AsyncWebServerRequest *request) {
	  if (!authenticated(request)) return request->requestAuthentication();
	  AsyncWebServerResponse *response = request->beginResponse(200, "text/plain", "Device is erasing...");
	  response->addHeader("Refresh", "15; url=/");
	  response->addHeader("Connection", "close");
	  request->send(response);
	  delay(1000);
//	  Settings.reset();
//	  ESP.eraseConfig();
//	  ESP.reset();
	});
	server->on("/reboot", HTTP_GET, [](AsyncWebServerRequest *request) { // called from spiffs.html (examples)
	  String logmessage = "Client:" + request->client()->remoteIP().toString() + " " + request->url();
	  request->send(SPIFFS, "/reboot.html", String(), false, htmlProcessor);
	  restart = true;
	  restartTime = millis();
	});
	// HTTP basic authentication
	server->on("/login", HTTP_GET, [](AsyncWebServerRequest *request) {
	  if (!authenticated(request)) return request->requestAuthentication();
	  request->send(200, "text/plain", "Login success!");
	});
	// visiting this page will cause you to be logged out
	server->on("/logout", HTTP_GET, [](AsyncWebServerRequest *request) {
	  if (!authenticated(request)) return request->requestAuthentication();
	  request->send(SPIFFS, "/logout.html", String(), false, htmlProcessor);
	});
	// presents a "you are now logged out webpage
	server->on("/logged-out", HTTP_GET, [](AsyncWebServerRequest *request) { // called from spiffs.html
	  Serial.println("Client:" + request->client()->remoteIP().toString() + " " + request->url());
	  request->send(SPIFFS, "/logout.html", String(), false, htmlProcessor);
	});
	server->on("/upload-file", HTTP_GET, [](AsyncWebServerRequest *request) {
	  String html = "<body><div><form method='post' action='/upload-file'><input type='file'><button>send</button></form></div></body>";
	  request->send(200, "text/html", html);
	});
/*
	server->on("/upload-file", HTTP_POST, [](AsyncWebServerRequest* request) {
	  AsyncWebServerResponse* response = request->beginResponse(200, "text/html", "hello world");
	  response->addHeader("Connection", "close");
	  request->send(response);
	} uploadFile);
*/
  // simple Firmware Update Form
	server->on("/update-simple", HTTP_GET, [](AsyncWebServerRequest *request) {
	  request->send(200, "text/html", "<form method='POST' action='/update' enctype='multipart/form-data'><input type='file' name='update'><input type='submit' value='Update'></form>");
	});
	server->on("/update", HTTP_POST, [](AsyncWebServerRequest *request) {
	  restart = !Update.hasError();
	  AsyncWebServerResponse *response = request->beginResponse(200, "text/plain", restart ? "OK":"FAIL");
	  response->addHeader("Connection", "close");
	  request->send(response);
	  }, [](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
		if (!index) {
		  Serial.printf("Update start: %s\n", filename.c_str());
//		  Update.runAsync(true); // 'class UpdateClass' has no member named 'runAsync'
		  if (!Update.begin((ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000)) Update.printError(Serial);
		}
		if (!Update.hasError()) {
		  if (Update.write(data, len) != len) Update.printError(Serial);
		}
		if (final) {
		  if (Update.end(true)){
		  Serial.printf("Update success: %uB\n", index + len);
		} else {
		  Update.printError(Serial);
		}
	  }
	});
	server->on("/heap", HTTP_GET, [](AsyncWebServerRequest *request) {
      request->send(200, "text/plain", String(ESP.getFreeHeap()));
	});
	server->on("/ip", HTTP_GET, [](AsyncWebServerRequest *request) {
	  request->send(200, "text/plain", "ok");
//	  Serial.print("Received request from client with IP: ");
	  Serial.println(request->client()->remoteIP());
	});
	server->on("/error", HTTP_GET, [](AsyncWebServerRequest *request) {
      request->send(200, "text/plain", String(error));
	});

/*
	MDNS.addService("http", "tcp", HTTP_PORT);
	if (MDNS.begin(Settings.data.deviceName)) {
	  Serial.println("mDNS running...");
	  MDNS.update();
	}
*/
	// JSON body handling with ArduinoJson for life.html
	AsyncCallbackJsonWebHandler *lifeHandler = new AsyncCallbackJsonWebHandler("/mosfetCtrl", [](AsyncWebServerRequest *request, JsonVariant &json) {
#ifdef DEBUG
	  Serial.println(F("/mosfetCtrl called"));
#endif
	  JsonObject jsonObj = json.as<JsonObject>();
	  String jsonStr;
	  serializeJson(jsonObj, jsonStr);
#ifdef DEBUG
	  Serial.println("posted json: " + jsonStr);
#endif
	  Bms.bmsCmd = MOSFET;			// MosFet Control
	  Bms.mosfets = (byte)jsonObj["chargeState"] + (byte)jsonObj["dischargeState"]<<1;
//	  request->send(200, "application/json", "{\"message\": \"OK\"}");
	  // ArduinoJson advanced response
	  AsyncJsonResponse *response = new AsyncJsonResponse();
	  JsonObject root = response->getRoot();
	  root["deviceName"] = Settings.data.deviceName;
	  root["dischargeState"] = dischargeState;
	  root["chargeState"] = chargeState;
	  response->setLength();
	  request->send(response);
	});
	server->addHandler(lifeHandler);
//#ifdef ENABLE_SOCKETS
	server->addHandler(&socket); // new
	socket.onEvent(onSocketEvent);
//#endif
	AsyncStaticWebHandler *cacheHandler = &server->serveStatic("/", SPIFFS, "/").setCacheControl("max-age=600");

	server->serveStatic("/", SPIFFS, "/");			// attach filesystem root at URL /
//	server->serveStatic("/", SPIFFS, "/").setCacheControl("max-age=31536000");
    server->serveStatic("/", SPIFFS, "/").setDefaultFile("bms.html");	// http:// ESP32-BMS2 also working
	server->serveStatic("/", SPIFFS, "/").setTemplateProcessor(htmlProcessor); // specify template processor callback
#if false
	server->serveStatic("/", SPIFFS, "/spiffs.html", "max-age=86400");
//	server->serveStatic("/index.html", SPIFFS, "/index.htm", "max-age=86400");
	server->serveStatic("/favicon.ico", SPIFFS, "/favicon.ico", "max-age=86400");
	server->serveStatic("/life.css", SPIFFS, "/life.css", "max-age=86400");
	server->serveStatic("/styles.css", SPIFFS, "/styles.css", "max-age=86400");
	server->serveStatic("/raphael.min.js", SPIFFS, "/raphael.min.js", "max-age=86400");
	server->serveStatic("/justgage.js", SPIFFS, "/justgage.js", "max-age=86400");
#endif
	Serial.print(F("starting Webserver: ")); Serial.print(IP_ADDRESS);
	Serial.print(':'); Serial.println(Settings.data.httpPort);
	server->begin();
  }
}

#endif /* __WEBSERVER_CPP__ */
