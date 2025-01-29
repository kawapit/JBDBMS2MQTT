#ifndef __JBDBMS_CPP__
#define __JBDBMS_CPP__

#include "JbdBms.h"

//using namespace MyLOG;

#define DEBUG
#define MyDebug Serial

//const String JbdBms::TAG = "JbdBms";
//---- global variables ----
ulong JbdBms::refresh = 5 * 1000;	// bms request intervall in ms
ulong JbdBms::lastRequested = 0;	// last requestet bms data
char JbdBms::bmsNum = '0';			// used for alias and topic
byte JbdBms::bmsCmd = PAUSE;		// no bms request task
byte JbdBms::mosfets = 0;
byte JbdBms::balance = 0;
byte JbdBms::debugLevel = 3;
//const int32_t JbdBms::c_cellNominalVoltage = 3700;
//const uint16_t JbdBms::c_cellAbsMin = 3000;
//const uint16_t JbdBms::c_cellAbsMax = 4200;
//const int32_t JbdBms::c_packMaxWatt = 1250;
//const uint16_t JbdBms::c_cellMaxDisbalance = 1500;
// callbacks:
bool JbdBms::doScan = false;
bool JbdBms::doConnect = false;
bool JbdBms::connected = false;
bool JbdBms::newPacketReceived = false;
byte JbdBms::dataOk = 0;
byte JbdBms::dataErr = 0;
byte JbdBms::bleRetrys = 0;
bmsDataStruct	JbdBms::data;		// bms data sorage
basicDataStruct	JbdBms::basicData;	// basic data storage
cellDataStruct	JbdBms::cellData;	// cell data storage
//--------------------------------------------------------------

void JbdBms::setDebugLevel(byte level) { debugLevel = level; }

byte JbdBms::getDebugLevel() { return debugLevel; }

/*
int16_t JbdBms::make_int(int highbyte, int lowbyte) // turns two bytes into a single long integer
{
  int16_t result = highbyte<<8 | lowbyte;
  if (highbyte & 0b10000000) { // test for positive / negative with bit mask
	result = ~result;
	result *= -1;
  }
  return result;
}
*/
//-------------------------------------

bool JbdBms::processBasicData(basicDataStruct *output, byte *data, unsigned int dataLen)
{
  if (data[0] + data[1] == 0) return false;				// voltage = 0?
  if (data[18] + data[21] + data[22] == 0) return false;	// num of cells or ntcs and software = 0? 
//if (data[21] != 8 && data[21] != 16) return false;		// 8 or 16 allowed
  if (data[21] % 4 != 0) return false;						// 4, 8 , 12 or 16 allowed
  output->Volts = (uint32_t)((data[0]<<8 | data[1]) * 10); // resolution 10mV -> convert to mV, eg 4895 -> 48950mV
#if false
  output->Amps =  ((int32_t)make_int(data[2], data[3])) * 10; // resolution 10mA, convert to mA
#else
  int16_t Amps = (data[2]<<8 | data[3]) * 10;
  if (data[2] & 0b10000000)				// test for positive/negative with bit mask
	Amps = (~Amps) * -1;
  output->Amps = Amps;
#endif
  output->Watts = output->Volts * output->Amps / 1000;		// mW
  output->Cycles =   (uint16_t)(data[8]<<8 | data[9]);	// number of cycles
  output->Capacity = (uint16_t)(data[6]<<8 | data[7]);	// (23000 = 59D8H)
  output->SoC_Ah =   (uint16_t)(data[4]<<8 | data[5]);
//output->SoC_Wh = output->SoC_Ah * c_cellNominalVoltage / 1000000 * cellData.NumOfCells;
  output->SoC = data[19];
  output->Protection = (uint16_t)(data[16]<<8 | data[17]);
  output->Software = data[18];
  output->NumOfCells = data[21];
  byte ntcs = data[22];
  output->NumOfTemp = ntcs;
  uint16_t tempSum = 0;
  uint16_t temp;
#if true
  for (byte n = 0; n < ntcs; n++) {
    temp = (uint16_t)((data[n * 2 + 23]<<8 | data[n * 2 + 24]) - 2731);
	output->Temp[n + 1] = temp;
	tempSum += temp;
  }
#else
  temp = (uint16_t)((data[23]<<8 | data[24]) - 2731);
  output->Temp[1] = temp;
  tempSum = temp;
  if (ntcs > 1) {
    temp = (uint16_t)((data[25]<<8 | data[26]) - 2731);
	output->Temp[2] = temp;
	tempSum += temp;
  }
  if (ntcs > 2) {
	temp = (uint16_t)((data[27]<<8 | data[28]) - 2731);
	output->Temp[3] = temp;
	tempSum += temp;
  }
#endif
  output->Temp[0] = tempSum / ntcs; // average temp
  output->BalanceCodeLow  = data[12]<<8 | data[13];
  output->BalanceCodeHigh = data[14]<<8 | data[15];
  byte fets = data[20];
  output->MosFet = fets;
  output->charging = fets & 1;			// Bit 0, 1 = on
  output->discharging = fets>>1 & 1;	// Bit 1
  return true;
}

bool JbdBms::processCellData(cellDataStruct *output, byte *data, unsigned int dataLen)
{
  byte cells = basicData.NumOfCells;
//output->NumOfCells = dataLen / 2;
  if (cells == 0 || cells != dataLen / 2) return false; // data length/2 must be number of cells
  if (data[2] + data[3] == 0) return false;	// cell 1 or 2 0V?
  if (data[4] + data[5] == 0) return false;
  uint16_t _cellSum = 0;
  uint16_t _cellMin = 5000;		// 5V
  uint16_t _cellMax = 0;
  byte _imin = 0;							// cell number low and high
  byte _imax = 0;
  for (byte c = 0; c < cells; c++) {		// go trough individual cells
	output->CellVolt[c] = (uint16_t)(data[c * 2]<<8 | data[c * 2 + 1]); // resolution 1mV
	_cellSum += output->CellVolt[c];
	if (output->CellVolt[c] > _cellMax) {
	  _cellMax = output->CellVolt[c];
	  _imax = c + 1;
	}
	if (output->CellVolt[c] < _cellMin) {
	  _cellMin = output->CellVolt[c];
	  _imin = c + 1;
	}
//	output->CellColor[c] = getPixelColorHsv(mapHue(output->CellVolt[c], c_cellAbsMin, c_cellAbsMax), 255, 255);
  }
  output->CellMin	= _cellMin;
  output->CellMax	= _cellMax;
  output->CellDiff	= _cellMax - _cellMin; // resolution 10mV -> convert to volts
  output->CellAvg	= _cellSum / cells;
  output->iMin		= _imin;
  output->iMax		= _imax;  
  // cell median calculation
  uint16_t temp;
  uint16_t array[cells];
  for (byte c = 0; c < cells; c++) {
	array[c] = output->CellVolt[c];
  }
  for (byte c = 1; c <= cells; ++c) {	// sort data
	for (byte i = c + 1; i <= cells; ++i) {
	  if (array[c] > array[i]) {
		temp = array[c];
		array[c] = array[i];
		array[i] = temp;
	  }
	}
  }
  if (cells % 2 == 0) {			// compute median
	output->CellMedian = (array[cells / 2] + array[cells / 2 + 1]) / 2;
  } else {
	output->CellMedian = array[cells / 2 + 1];
  }
/* // for TFT version
  for (byte c = 0; c < cells; c++) {
	uint32_t disbal = abs(output->CellMedian - output->CellVolt[c]);
//	output->CellColorDisbalance[c] = getPixelColorHsv(mapHue(disbal, c_cellMaxDisbalance, 0), 255, 255);
  }
*/
  return true;
}

bool JbdBms::processHwData(basicDataStruct *output, byte *data, unsigned int dataLen)
{
  int i;
  for (i = 0; i < dataLen; i++) {
	output->Hardware[i] = char(data[i]);
  }
  output->Hardware[i] = 0; // Etx
 if (debugLevel > 2 && output->Software != 0) {
  	Serial.println(F(">> hardware data <<"));
	Serial.print(F("hardware: ")); Serial.print(output->Hardware);
	Serial.printf(" software: %d.%d\n", output->Software>>4, output->Software & 0x0F);
  }
  return true;
}

bool JbdBms::processBalanceData(basicDataStruct *output, byte *data, unsigned int dataLen)
{
  output->BalanceCtrlHigh = data[5];
  output->BalanceCtrlLow  = data[6];
  if (debugLevel > 2) {
	Serial.println(F(">> balance data <<"));
	Serial.print(F("balance enable ")); Serial.print((data[6]>>2 & 1) ? "on":"off");
	Serial.print(F(", charge balance "));  Serial.print((data[6]>>3 & 1) ? "on":"off");
	Serial.print(F(", switch ")); Serial.println((data[6] & 1) ? "on":"off");
  }
  return true;
}


byte JbdBms::Checksum(byte *packet) // LSB only
{
  BmsPacketHeaderStruct *pHeader = (BmsPacketHeaderStruct *)packet;
  int checksumLen = pHeader->dataLen + 2; // status + data len + data
//int checksumLen = packet[3] + 2;
  byte checksum = 0;
  for (int i = 0; i < checksumLen; i++) {
	checksum += packet[i + 2];		// skip header STX and command type
  }
//printf("checksum1: %x, ", checksum);
  checksum = ((checksum ^ 0xFF) + 1) & 0xFF;
//printf("checksum: %x\n", checksum);
  return checksum;
}


bool JbdBms::ChecksumOk(byte *packet) // check if packet is valid
{
  if (packet == nullptr) return false;
//BmsPacketHeaderStruct *pHeader = (BmsPacketHeaderStruct *)packet;
//if (pHeader->start != STX) return false;
  if (packet[0] != STX || packet[2] != 0) return false; // invalid packet
//int checksumLen = pHeader->dataLen + 2;
  int checksumLen = packet[3] + 2;		// status + datalen + data
  byte rxChecksum = packet[2 + checksumLen + 1];
  if (Checksum(packet) == rxChecksum) {
#ifdef DEBUG
	if (debugLevel > 2) Serial.print("packet ok, ");
#endif
	return true;
  } else {
#ifdef DEBUG
	if (debugLevel > 2) Serial.printf("packet invalid, expected value: %x\n", rxChecksum);
#endif
	return false;
  }
}


bool JbdBms::PacketOk(byte *packet) // process different packets from bms
{
  if (!ChecksumOk(packet)) return false;

  BmsPacketHeaderStruct *pHeader = (BmsPacketHeaderStruct *)packet;
  byte *data = packet + sizeof(BmsPacketHeaderStruct); // TODO Fix this ugly hack
  unsigned int dataLen = pHeader->dataLen;
  bool result = false;
  
  switch (pHeader->type) {		// decision based on packet type
	case BASICINFO: {			// responce code
	  if (debugLevel > 2) Serial.println(F("process basic data... "));
	  result = processBasicData(&basicData, data, dataLen);
#ifdef DEBUG
	  if (result && debugLevel > 2) printBasicData();
#endif
//	  showInfoLcd();
	  break;
	}
	case CELLINFO: {
	  if (debugLevel > 2) Serial.println(F("process cell data..."));
	  result = processCellData(&cellData, data, dataLen);
#ifdef DEBUG
	  if (result && debugLevel > 2) printCellData();
#endif
	  break;
	}
	case HWINFO: {
	  if (debugLevel > 2) Serial.println(F("process hardware data..."));
	  result = processHwData(&basicData, data, dataLen); // copy into data pack
	  break;
	}
	case BALANCE: {
	  if (debugLevel > 2) Serial.println(F("process balance data..."));
	  result = processBalanceData(&basicData, data, dataLen); // copy into data pack
	  break;
	}
	case MOSFET: {
	  if (debugLevel > 2) Serial.println(F("process MosFet control"));
	  result = true;
	  break;
	}
	default: {
	  result = false;
	  if (debugLevel > 2) Serial.printf("unsupported packet type: %d, ", pHeader->type); // 170 = $AA
	}
  }
  if (!result && debugLevel > 2) Serial.println("invalid packet received");
  if (result) {
	dataOk++;			// used for mqtt publish
	dataErr = 0;
  } else dataErr++;
  return result;				// true if processed packet is valid
}

//---------------------------------- get packets from BLE server and process them
/*
dd 03 00 26 0a 49 00 00 2b b0 59 d8 00 01 2e ea 00 00 00 00 
10 00 48 31 01 08 03 0b 4c 0b 45 0b 43 00 00 00 59 d8 2b b0 
00 00 f8 cc 77

dd 04 00 10 0c de 0c dd 0c dd 0c dd 0c dd 0c de 0c de 0c de 
f8 a4 77

dd 05 00 0f 42 53 32 30 32 33 30 38 2d 30 31 2d 31 30 34 fc 
dd 77 dd a5 05 00
*/
bool JbdBms::collectPacket(char *data, uint32_t dataSize) // reconstruct packet from BLE incomming data,
{															// called by notifyCallback function
  static uint8_t packetstate = 0; // 0 = empty, 1 = first half of packet received, 2 = second half of packet received
  static uint8_t packetbuff[42] = { 0x00 }; // 40 20 bytes per packet, size is important!!!
  static uint16_t packetSize = 0;
  bool retVal = false;
#ifdef DEBUG
  if (debugLevel == 4) hexDump(data, dataSize); // print subpacket
#endif
  if (packetstate == 0 && data[0] == STX) {	// probably got 1st subpacket
	for (uint8_t i = 0; i < dataSize; i++) {	// save subpacket
	  packetbuff[i] = data[i];
	}
	packetSize = dataSize;
	packetstate = 1;
  } else if (packetstate == 1) {
	for (uint8_t i = 0; i < dataSize; i++) {	// append subpacket
	  packetbuff[i + packetSize] = data[i];
	}
	packetSize += dataSize;
	if (data[dataSize - 1] == ETX) {			// probably got last subpacket
//	  packetstate = 2;
	  uint8_t packet[packetSize];
	  memcpy(packet, packetbuff, packetSize);	// packetbuf -> packet
	  retVal = PacketOk(packet);				// packet data valid ?
	  packetstate = 0;							// look for new packets
#ifdef DEBUG
	  if (debugLevel == 4) hexDump(packet, packetSize); // print whole packet
#endif
	}
  }
  return retVal;		// true if packet is complete and valid
}

// Callback function, called for each partial packet
void JbdBms::notifyCallback(BLERemoteCharacteristic *pBLERemoteCharacteristic, uint8_t *pData, size_t length, bool isNotify)
{ // this is called when BLE server sents data via notification
  newPacketReceived = collectPacket((char *)pData, length); // true if packet complete and valid
}

//-----------------------------------------------
/*
void JbdBms::printHwData() // implemented in bmsRequestHwData()
*/

void JbdBms::printBasicData() // display basic data
{
  Serial.print(">> Basic Data <<\n");
  Serial.printf("voltage: %.2fV  ", (float)basicData.Volts / 1000);
  Serial.printf("current: %.2fA\n", (float)basicData.Amps / 1000);
  Serial.printf("soc Ah: %.2fAh  ", (float)basicData.SoC_Ah / 100);
  Serial.printf("soc: %d%%  ", basicData.SoC);
  Serial.printf("cycles: %.0f\n",   basicData.Cycles);
  Serial.printf("temp1: %.1f\u00B0C  ", (float)basicData.Temp[1] / 10); // 0xB0
  if (basicData.NumOfTemp > 1)
	Serial.printf("temp2: %.1f\u00B0C  ", (float)basicData.Temp[2] / 10);
  if (basicData.NumOfTemp > 2)
	Serial.printf("temp3: %.1f\u00B0C", (float)basicData.Temp[3] / 10);
  Serial.println();
  Serial.printf("balance code low: 0x%x  ", basicData.BalanceCodeLow);
  Serial.printf("balance code high: 0x%x\n", basicData.BalanceCodeHigh);
  Serial.printf("protection status: 0x%x  ", basicData.Protection);
  Serial.printf("MosFet status: 0x%x  ", basicData.MosFet);
  Serial.print("charging: "); Serial.print((basicData.MosFet & 1) ? "on":"off");
  Serial.print(" discharging: "); Serial.println((basicData.MosFet>>1 & 1) ? "on":"off");
}


void JbdBms::printCellData() // display cell data
{
  Serial.print(">> Cell Data <<\n");
  Serial.printf("number of cells: %u\n", basicData.NumOfCells);
  for (byte c = 1; c <= basicData.NumOfCells; c++) {
	Serial.printf("cell%u", c);
    Serial.printf(": %.3fV  ", (float)cellData.CellVolt[c - 1] / 1000);
  }
  Serial.println();
  Serial.printf("cell%u max: %.3fV  ", cellData.iMax, (float)cellData.CellMax / 1000);
  Serial.printf("cell%u min: %.3fV  ", cellData.iMin, (float)cellData.CellMin / 1000);
  Serial.printf("difference: %.3fV\n", (float)cellData.CellDiff / 1000);
  Serial.printf("average: %.3fV  ", (float)cellData.CellAvg / 1000);
  Serial.printf("median: %.3fV\n", (float)cellData.CellMedian / 1000);
}


void JbdBms::hexDump(void *ptr, uint32_t buflen)
{
  unsigned char *buf = (unsigned char*)ptr;
  
  for (int i = 0; i < buflen; i++) {
	Serial.printf("%02x ", buf[i]);
//	Serial.print(buf[i], HEX); Serial.print(' ');
  }
  Serial.println();
}

//----------------------------- Bluetooth Stuff ------------------------------

// BLE pointers and constants:
BLEScan *pBLEScan = NULL;
BLEClient *pClient = NULL;
BLEAdvertisedDevice *bmsDevice = NULL;
BLERemoteCharacteristic *pRemoteCharacteristic;
BLERemoteService *pRemoteService;
// the remote service we wish to connect to. Needs check/change when other BLE module as xiaoxiang used
BLEUUID serviceUUID = BLEUUID("0000ff00-0000-1000-8000-00805f9b34fb");
BLEUUID charUUID_tx = BLEUUID("0000ff02-0000-1000-8000-00805f9b34fb"); // m w
BLEUUID charUUID_rx = BLEUUID("0000ff01-0000-1000-8000-00805f9b34fb"); // m r

// like Victron SW
// Callback to process the results of the last scan or restart it
void scanEndedCallback(BLEScanResults results)
{
  Serial.println("end of scan");
}


class ClientCallback : public BLEClientCallbacks
{
  void onConnect(BLEClient *pclient)
  {
	digitalWrite(BLE_LED, HIGH);
	Serial.print(F("connect, "));
	// After connection we should change the parameters if we don't need fast response times.
	// These settings are 150ms interval, 0 latency, 450ms timout.
	// Timeout should be a multiple of the interval, minimum is 100ms.
	// I find a multiple of 3-5 * the interval works best for quick response/reconnect.
	// Min interval: 120 * 1.25ms = 150, Max interval: 120 * 1.25ms = 150, 0 latency, 60 * 10ms = 600ms timeout
//	pClient->updateConnParams(120, 120, 0, 60);
  };

  void onDisconnect(BLEClient *pclient)
  {
	JbdBms::connected = false;
	digitalWrite(BLE_LED, LOW);
	Serial.print(F("disconnect, "));
	BLEDevice::getScan()->start(0, scanEndedCallback);
  };
/*
  bool onConnParamsUpdateRequest(BLEClient *pclient, const ble_gap_upd_params *params)
  {
	if (params->itvl_min < 24) {		// 1.25ms units
	  return false;
	} else if (params->itvl_max > 40) {// 1.25ms units
	  return false;
	} else if (params->latency > 2) {	// Number of intervals allowed to skip
	  return false;
	} else if (params->supervision_timeout > 100) { // 10ms units
	  return false;
	}
	return true;
  };
*/
};

// scan for BLE servers and find the first one that advertises the service we are looking for
class AdvertisedDeviceCallbacks: public BLEAdvertisedDeviceCallbacks
{
//AdvertisedDeviceCallbacks(BLEUUID serviceUUID_) : serviceUUID(serviceUUID_), doConnect(false), doScan(false) { }

  void onResult(BLEAdvertisedDevice advertisedDevice) // called for each advertising BLE server
  {
	Serial.println(F("BLE advertised device found: "));
	Serial.println(String(advertisedDevice.toString().c_str())); // print Name, Address, manufacturer data, serviceUUID
	// we have found a device, let us now see if it contains the service we are looking for
	if (advertisedDevice.haveServiceUUID() && advertisedDevice.isAdvertisingService(serviceUUID)) {
//		&& (strcmp(advertisedDevice->getName().c_str(),settings.jbdbms_device_id) == 0))
	  Serial.println(F("found BMS"));
	  pBLEScan->stop();
	  bmsDevice = new BLEAdvertisedDevice(advertisedDevice);
//	  bmsDevice = advertisedDevice;
	  JbdBms::doConnect = true;		// ready to connect now
	  JbdBms::doScan = true;
	} // found our server
  }
};

// BLE Callbacks:
static ClientCallback *bmsCallback;
static AdvertisedDeviceCallbacks *bleCallbacks; //??

void JbdBms::setup() // setup Bluetooth and scan for BLE devices
{
  pinMode(BLE_LED, OUTPUT);
  digitalWrite(BLE_LED, LOW);
  Serial.print(F("scanning Bluetooth... "));
  BLEDevice::init("ESP32-BMS");					// create the BLE Device, create new scan
  pClient = BLEDevice::createClient();			// create ble client
//Serial.print(" created client");
  // retrieve a Scanner and set the callback we want to use to be informed when we have detected a new device
  pBLEScan = BLEDevice::getScan();				// set pointer to BLEScan
#if false
  bleCallbacks = new AdvertisedDeviceCallbacks();
  pBLEScan->setAdvertisedDeviceCallbacks(bleCallbacks);
#else
  pBLEScan->setAdvertisedDeviceCallbacks(new AdvertisedDeviceCallbacks());
#endif
  pBLEScan->setInterval(100);	//1349
  pBLEScan->setWindow(99);		//449			// less or equal setInterval value
  pBLEScan->setActiveScan(true);				// active scan uses more power, but get results faster
#if true
  BLEScanResults foundDevices = pBLEScan->start(5, false);	// scan to run for 5 seconds
  Serial.print(foundDevices.getCount() + 1); Serial.print(" device(s) found, ");
//Serial.println(F("BLE scan done"));
  pBLEScan->clearResults();						// delete results from BLE scan buffer to release memory
#endif
  bleRetrys = 0;
  dataOk = 0;
  delay(2000);
}


bool JbdBms::connect() // connect BME device, called from process()
{
//data.State = false;
  Serial.print(F("pairing device: "));
//Serial.print(String(bleCallbacks->bmsDevice->getAddress().toString().c_str()));
//data.Address = bleCallbacks->bmsDevice->getAddress().toString().c_str();	// ex. "a4:c1:37:42:c5:e2"
  data.Address = bmsDevice->getAddress().toString().c_str();				// ex. "a4:c1:37:42:c5:e2"
  Serial.print(data.Address); Serial.print(F("... "));
//bmsCallback = new ClientCallback();
//pClient->setClientCallbacks(bmsCallback);
  pClient->setClientCallbacks(new ClientCallback());
//pClient->connect(bleCallbacks->bmsDevice);	// connect to the remote BLE Server
  pClient->connect(bmsDevice);				// connect to the remote BLE Server
  pClient->setMTU(517);						// set client to request maximum MTU from server (default is 23 otherwise)
//delay(200); // hope it helps against ->  lld_pdu_get_tx_flush_nb HCI packet count mismatch (0, 1)
/*
  // https://github.com/nkolban/esp32-snippets/issues/757
// bool success = pClient->connect(address, BLE_ADDR_TYPE_RANDOM);
  BLEAddress address = bleCallbacks->bmsDevice->getAddress();
  pClient->connect(address, BLE_ADDR_TYPE_RANDOM); // connect to the remote BLE Server
//pairing device: a4:c1:37:42:c5:e2... [ 52328][E][BLEClient.cpp:239] gattClientEventHandler(): Failed to connect, status=Unknown ESP_ERR error
  // if you pass BLEAdvertisedDevice instead of address, it will be recognized type of peer device address (public or private)
*/
  // obtain a reference to the service we are after in the remote BLE server
  pRemoteService = pClient->getService(serviceUUID);
  if (pRemoteService == nullptr) {
	Serial.print(F("wrong service UUID: ")); Serial.println(serviceUUID.toString().c_str());
//	pClient->disconnect();
	BLEDevice::deleteClient(pClient);
	return false;
  }
  data.ServiceUUID = serviceUUID.toString().c_str();	// save ServiceUUID
  Serial.print(F("service ok, "));
  // obtain a reference to the characteristic in the service of the remote BLE server
  pRemoteCharacteristic = pRemoteService->getCharacteristic(charUUID_rx);
  if (pRemoteCharacteristic == nullptr) {
	Serial.print(F("wrong characteristic UUID: ")); Serial.println(charUUID_rx.toString().c_str());
//	pClient->disconnect();
	BLEDevice::deleteClient(pClient);
	return false;
  }
  data.CharUUID = charUUID_rx.toString().c_str();		// save CharUUID
  Serial.print(F("characteristic ok, "));
  if (pRemoteCharacteristic->canRead()) {				// read the value of the characteristic
	std::string value = pRemoteCharacteristic->readValue();
	Serial.print(value.c_str()); // (w)
  }
  if (pRemoteCharacteristic->canNotify())
	pRemoteCharacteristic->registerForNotify(notifyCallback);
//bmsCallback->connected = true; // ???
//bleCallbacks->doConnect = false; // new
  JbdBms::connected = true;	// local var
//JbdBms::doConnect = false;
  data.State = true;
  digitalWrite(BLE_LED, HIGH);
  return true;
}

void JbdBms::disconnect() // does not work as intended, but automatically reconnected
{
  pClient->disconnect();
//bmsCallback->connected = false; // ???
  JbdBms::connected = false;
  data.State = false;
  bmsCmd = PAUSE;
  dataOk = 0;
  Serial.println(F("disconnected from BMS"));
  digitalWrite(BLE_LED, LOW);
}

void JbdBms::process() // request data from BLE server, called in loop
{
  static unsigned int requestCnt;		// HWINFO only after 10th loop
//if (bmsCmd == CONNECT && bmsCallback->connected == false) start(); // scan ble devices and reconnect (endless scan loop!!!)
  // if the flag "doConnect" is true then we have scanned for and found the desired
  // BLE Server with which we wish to connect.  Now we connect to it.
  // Once we are connected we set the connected flag to be true.
//if (bleCallbacks->doConnect == true) {
  if (doConnect) {
	if (connect()) {								// pairing ok?
//	  data.Name = bleCallbacks->bmsDevice->getName().c_str();
//	  data.RSSI = bleCallbacks->bmsDevice->getRSSI();
//	  data.TXPower = bleCallbacks->bmsDevice->getTXPower();
	  data.Name = bmsDevice->getName().c_str();
	  data.RSSI = bmsDevice->getRSSI();
	  data.TXPower = bmsDevice->getTXPower();
//	  refresh = Settings.data.bmsRefresh * 1000;		// set bms refresh rate in ms
//	  basicData.Volts = NUMCELLS * 3 * 1000;			// initalize pack voltage not to disconnect WiFi 
	  // *** root topic setting ***
	  strcpy(data.Alias, BMS_ALIAS); 					// set default alias
	  if (data.Name[8] == '2') bmsNum = '1';
	  else if (data.Name[8] == '3') bmsNum = '2';
	  data.Alias[strlen(data.Alias) - 1] = bmsNum;		// set bms alias: BMS1, BMS2
//	  Settings.data.mqttTopic[strlen(Settings.data.mqttTopic) - 1] = bmsNum; // also set root topic
//	  Serial.print("connected to "); Serial.print(char('0' + data.Num)); Serial.print(": ");
	  Serial.print(F("connected to ")); Serial.print(data.Alias);
	  Serial.print(": "); Serial.println(data.Name);
//	  bleCallbacks->doConnect = false;					// bms pairing ended
//	  JbdBms::doConnect = false;						// bms pairing ended
	  bmsCmd = CONNECT;									// activate bms requests
/*
	  // send MQTT message
//	  pRemoteCharacteristic->writeValue(getdeviceData, 20);	// sending getdevice info
	  sendingtime = millis();
	  Serial.println("sending device Data");
	  String topic = mqttname + "/BLEconnection";
	  mqttClient.publish(topic.c_str(),"connected");
*/
	}
  } else { // doConnect = false
//	if (!bmsCallback->connected) { // doconnect = false and connected = false
	if (!connected) { // doconnect = false and connected = false
	  Serial.println(F("failed to connect to BMS!"));
	  digitalWrite(BLE_LED, LOW);
	  bleRetrys++;
	  bmsCmd = PAUSE;						// pause bms requests
	  pBLEScan = BLEDevice::getScan(); 	// set pointer to BLEScan
	  pBLEScan->start(5, false);			// rescan bluetooth
//	  BLEDevice::getScan()->start(0);
	  dataOk = 0; dataErr = 0;
	  delay(1000);
	  return;
	}
	JbdBms::doConnect = false;						// bms pairing ended (new)
  }
  // if we are connected to a peer BLE Server, update the characteristic each time we are reached
  // with the current time since boot
//if (bmsCallback->connected) {
  if (connected) {
	bleRetrys = 0;
	ulong ms = millis();
	if (ms - lastRequested >= refresh) { // every bms refresh time period
	  lastRequested = ms;

	  Serial.print(":"); Serial.print(bmsCmd); Serial.print(newPacketReceived); Serial.print(dataErr); Serial.print(' ');

	  switch (bmsCmd) {
		case BASICINFO: {
		  bmsRequestBasicData();	// get basic data
		  bmsCmd = CELLINFO;		// next get cell info
		  break;
		}
		case CELLINFO: {
		  bmsRequestCellData();
		  if (requestCnt % 10 == 0) bmsCmd = HWINFO;
		  else bmsCmd = BASICINFO;	// next get basic data
		  break;
		}
		case HWINFO: {
		  bmsRequestHwData();
 		  bmsCmd = BASICINFO;		// next get basic data
		  break;
		}
		case MOSFET: {
		  bmsMosfetCtrl();
		  bmsCmd = BASICINFO;
		  break;
		}
		case DISCONNECT: {
		  disconnect();
		  bmsCmd = PAUSE;
		  break;
		}
		case CONNECT: {
		  bmsCmd = BASICINFO;	// actvates BleRequestData() 
		  break;
		}
		case PAUSE: {			// no bms request
		  break;
		}
//		default: {
//		  break;
//		}
	  }
	  requestCnt++;
	}
//} else if (bleCallbacks->doScan) { // reconnect
//	BLEDevice::getScan()->start(0); // this is just example to start scan after disconnect, most likely there is better way to do it
  }
}
//---------------------------- BMS server requests

void JbdBms::bmsSendCmd(uint8_t *data, uint32_t dataLen)
{
  pRemoteCharacteristic = pRemoteService->getCharacteristic(charUUID_tx);

  if (pRemoteCharacteristic) {
	pRemoteCharacteristic->writeValue(data, dataLen);
#ifdef DEBUG
	if (debugLevel > 2) Serial.print(F("\nBMS request "));
  } else {
	if (debugLevel > 2) Serial.println(F("remote TX characteristic not found"));
#endif
  }
  newPacketReceived = false; // set true by collectPackage
}


void JbdBms::bmsRequestBasicData()	// request BMS Basic Data
{ // header status command length data checksum footer
  //  DD  A5  03  00  FF  FD  77
  uint8_t packet[7] = { STX, READ, BASICINFO, 0x00, 0xFF, 0xFD, ETX };
  bmsSendCmd(packet, sizeof(packet));
#ifdef DEBUG
  if (debugLevel > 2) Serial.print(F("basic data... "));
#endif
}

void JbdBms::bmsRequestCellData()	// request BMS Cell Data
{ //   DD  A5  04  00  FF  FC  77
  uint8_t packet[7] = { STX, READ, CELLINFO, 0x00, 0xFF, 0xFC, ETX };
  bmsSendCmd(packet, sizeof(packet));
#ifdef DEBUG
  if (debugLevel > 2) Serial.print(F("cell data... "));
#endif
}

void JbdBms::bmsRequestHwData()		// request BMS Hw Data
{ //   DD  A5  05  00  FF  FB  77
  uint8_t packet[7] = { STX, READ, HWINFO, 0x00, 0xFF, 0xFB, ETX };
  bmsSendCmd(packet, sizeof(packet));
#ifdef DEBUG
  if (debugLevel > 2) Serial.print(F("hardware data... "));
#endif
}

void JbdBms::bmsRequestBalanceData() // request BMS Balance Data
{ //   DD  A5  2D  02  FF  FC  77
  uint8_t packet[7] = { STX, READ, BALANCE, 0x02, 0xFF, 0xD3, ETX };
//packet[5] = bch; packet[6] = bcln;
  packet[6] = Checksum(packet);
  bmsSendCmd(packet, sizeof(packet));
#ifdef DEBUG
  if (debugLevel > 2) Serial.print(F("balance data... "));
#endif
}

void JbdBms::bmsMosfetCtrl()	// MosFet Control
{
/* mosfets, Bit 0 = charging, Bit 1 = discharging
	0  00b = charge = off, discharge = off	DD 5A E1 02 00 03 FF 1A 77
	1  01b = charge = on,  discharge = off	DD 5A E1 02 00 02 FF 1B 77
	2  10b = charge = off, discharge = on	DD 5A E1 02 00 01 FF 1C 77
	3  11b = charge = on,  discharge = on	DD 5A E1 02 00 00 FF 1D 77
*/
  byte mosfetByte = mosfets ^ 3;
  uint8_t packet[9] = { STX, WRITE, MOSFET, 0x02, 0x00, 0x00, 0xFF, 0x00, ETX };
  packet[5] = mosfetByte;
  packet[7] = Checksum(packet);
//bmsSendCmd(packet, sizeof(packet)); // send to bms
#ifdef DEBUG
  if (debugLevel > 1) {
	Serial.print("MosFet control " + String(mosfets) + ", sent: ");
	hexDump(packet, sizeof(packet));
  }
#endif
}

void JbdBms::setCharging(bool state)
{
  if (state) {
	mosfets = basicData.MosFet | 1;			// set bit0
	basicData.charging = true;
  } else {
	mosfets = basicData.MosFet & ~1;		// clear bit0
	basicData.charging = false;
  }
  bmsCmd = MOSFET; 
}

void JbdBms::setDischarging(bool state)
{
  if (state) {
	mosfets = basicData.MosFet | 2;			// set bit1
	basicData.discharging = true;
  } else {
	mosfets = basicData.MosFet & ~2;		// clear bit1
	basicData.discharging = false;
  }
  bmsCmd = MOSFET; 
}


void JbdBms::bmsBalanceCtrl()	// Balance Control
{
/* balance
	4  100b = balance = on
	2  010b = charge balance = on
	1  001b = switch on
*/
  byte balanceByte = balance ^ 7;
  uint8_t packet[9] = { STX, WRITE, MOSFET, 0x02, 0x00, 0x00, 0xFF, 0x00, ETX };
  packet[5] = balanceByte;
  packet[7] = Checksum(packet);
//bmsSendCmd(packet, sizeof(packet)); // send to bms
#ifdef DEBUG
  if (debugLevel > 1) {
	Serial.print("Balance control " + String(balance) + ", sent: ");
	hexDump(packet, sizeof(packet));
  }
#endif
}

void JbdBms::setBalanceEnable(bool state)
{
  if (state) {
	balance = basicData.BalanceCodeLow | 4;		// set bit2
  } else {
	balance = basicData.BalanceCodeLow & ~4;	// clear bit2
  }
  bmsCmd = BALANCE; 
}

void JbdBms::setChargeBalance(bool state)
{
  if (state) {
	balance = basicData.BalanceCodeLow | 2;		// set bit1
  } else {
	balance = basicData.BalanceCodeLow & ~2;	// clear bit1
  }
  bmsCmd = BALANCE; 
}

#endif /* __JBDBMS_CPP__ */
