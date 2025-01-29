//File WiFiManager.h:

#include <ESPAsyncWiFiManager.h>

bool saveConfig = false;
AsyncWebServer server(80);
DNSServer dns;


void saveConfigCallback()
{
  Serial.println(F("should save config"));
  saveConfig = true;
}

void initSettings()
{
  AsyncWiFiManagerParameter custom_mqtt_server("mqttServer", "MQTT Server", NULL, 32);
  AsyncWiFiManagerParameter custom_mqtt_user("mqttUser", "MQTT User", NULL, 32);
  AsyncWiFiManagerParameter custom_mqtt_pass("mqttPass", "MQTT Password", NULL, 32);
  AsyncWiFiManagerParameter custom_mqtt_topic("mqttTopic", "MQTT Topic", "/ESP32-BMS1", 32);
  AsyncWiFiManagerParameter custom_mqtt_triggerpath("mqttTrigger", "MQTT Data Trigger", NULL, 80);
  AsyncWiFiManagerParameter custom_mqtt_port("mqttPort", "MQTT Port", "1883", 5);
  AsyncWiFiManagerParameter custom_mqtt_refresh("mqttRefresh", "MQTT Send Interval", "300", 4);
  AsyncWiFiManagerParameter custom_device_name("deviceName", "Device Name", "ESP32-BMS", 32);

  AsyncWiFiManager wm(&server, &dns);
  wm.setDebugOutput(false);				// disable wifimanager debug output
  wm.setMinimumSignalQuality(20);		// filter weak wifi signals
//wm.setConnectTimeout(15);				// how long to try to connect for before continuing
  wm.setConfigPortalTimeout(120);		// auto close configportal after n seconds
  wm.setSaveConfigCallback(saveConfigCallback);

  wm.addParameter(&custom_mqttServer);
  wm.addParameter(&custom_mqttUser);
  wm.addParameter(&custom_mqttPass);
  wm.addParameter(&custom_mqttTopic);
  wm.addParameter(&custom_mqttTrigger);
  wm.addParameter(&custom_mqttPort);
  wm.addParameter(&custom_mqttRefresh);
  wm.addParameter(&custom_deviceName);

  bool apRunning = wm.autoConnect("ESP32-BMS-AP");
  // save settings if wifi setup is fire up
  if (saveConfig) {
	strncpy(Settings.data.mqttServer, custom_mqttServer.getValue(), 40);
	strncpy(Settings.data.mqttUser, custom_mqttUser.getValue(), 40);
	strncpy(Settings.data.mqttPassword, custom_mqttPass.getValue(), 40);
	Settings.data.mqttPort = atoi(custom_mqttPort.getValue());
	strncpy(Settings.data.deviceName, custom_deviceName.getValue(), 40);
	strncpy(Settings.data.mqttTopic, custom_mqttTopic.getValue(), 40);
	Settings.data.mqttRefresh = atoi(custom_mqttRefresh.getValue());
	strncpy(Settings.data.mqttTriggerPath, custom_mqttTrigger.getValue(), 80);
	Settings.save();
	ESP.reset();
  }

  mqttclient.setServer(Settings.data.mqttServer, Settings.data.mqttPort);
  DEBUG_PRINTLN(F("MQTT Server config loaded"));
  mqttclient.setCallback(mqttcallback);
  //  check is WiFi connected
  if (!apRunning) {
	DEBUG_PRINTLN(F("failed to connect to WiFi or hit timeout"));
  } else {
//	deviceJson["IP"] = WiFi.localIP(); // grab the device ip
//	bms.Init(); // init the bms driver
//	bms.callback(processData);

	// rebuild the py script and webserver to chunked response for faster react, example here
	// https://github.com/helderpe/espurna/blob/76ad9cde5a740822da9fe6e3f369629fa4b59ebc/code/espurna/web.ino
	// https://stackoverflow.com/questions/66717045/espasyncwebserver-chunked-response-inside-processor-function-esp32-esp8266
	server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
	  if (strlen(Settings.data.httpUser) > 0 && !request->authenticate(Settings.data.httpUser, Settings.data.httpPass))
		return request->requestAuthentication();
	  AsyncWebServerResponse *response = request->beginResponse_P(200, "text/html", HTML_MAIN, htmlProcessor);
	  request->send(response);
	});
