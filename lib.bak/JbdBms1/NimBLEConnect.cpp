#include "NimBLEDevice.h"
 
// void setup() in Arduino
void app_main(void)  
{
  NimBLEDevice::init("");
  
  NimBLEScan *pScan = NimBLEDevice::getScan();
  NimBLEScanResults results = pScan->start(10);
  
  NimBLEUUID serviceUuid("ABCD");
  
  for(int i = 0; i < results.getCount(); i++) {
	NimBLEAdvertisedDevice device = results.getDevice(i);
	
	if (device.isAdvertisingService(serviceUuid)) {
	  NimBLEClient *pClient = NimBLEDevice::createClient();
	  
	  if (pClient->connect(&device)) {
		NimBLERemoteService *pService = pClient->getService(serviceUuid);
		if (pService != nullptr) {
		  NimBLERemoteCharacteristic *pCharacteristic = pService->getCharacteristic(charUUID_rx);
		  if (pCharacteristic != nullptr) {
			std::string value = pCharacteristic->readValue();
			// print or do whatever you need with the value
		  }
		}
	  } else {
	  // failed to connect
	  }
	  
	  NimBLEDevice::deleteClient(pClient);
	}
  }
}
