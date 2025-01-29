//
// new Version with NimBLE Library
//

#ifndef __JBDBMS_CPP__
#define __JBDBMS_CPP__

#include "JbdBms.h"

#define DEBUG

//---- global variables ----
ulong JbdBms::refresh = 5 * 1000;	// bms request intervall in ms
ulong JbdBms::lastRequested = 0;	// time of last requestet bms data
byte JbdBms::bmsCmd = PAUSE;		// no bms request task
byte JbdBms::mosfets = 0;
byte JbdBms::balance = 0;
byte JbdBms::debugLevel = DBG_BLE1;
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
int JbdBms::dataOk = 0;				// number of valid packets
int JbdBms::dataErr = 0;			// valid packet received
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
// protection codes:
#define BMS_STATUS_OK				0
#define BMS_STATUS_CELL_OVP			1
#define BMS_STATUS_CELL_UVP			2		///< Power off
#define BMS_STATUS_PACK_OVP			4
#define BMS_STATUS_PACK_UVP			8		///< Power off
#define BMS_STATUS_CHG_OTP			16
#define BMS_STATUS_CHG_UTP			32
#define BMS_STATUS_DSG_OTP			64		///< Power off
#define BMS_STATUS_DSG_UTP			128		///< Power off

#define BMS_STATUS_CHG_OCP			256
#define BMS_STATUS_DSG_OCP			512		///< Power off
#define BMS_STATUS_SHORT_CIRCUIT	1024	///< Power off
#define BMS_STATUS_AFE_ERROR		2048
#define BMS_STATUS_SOFT_LOCK		4096
#define BMS_STATUS_CHGOVERTIME		8192
#define BMS_STATUS_DSGOVERTIME		16384	///< Power off
#define BMS_POWER_OFF_ERRORS		0x46CA

static const char *const ErrorStr[16] {
	"",								// no error
	"cell overvoltage",
	"cell undervoltage",			///< Power off
	"pack overvoltage",
	"pack undervoltage",			///< Power off
	"charge overtemp",
	"charge undertemp",				///< Power off
	"discharge overtemp",
	"discharge undertemp",
	"charge overcurrent",
	"discharge overcurrent",		///< Power off
	"short circuit",				///< Power off
	"ic frontend error",
	"soft lock",
	"charge overtime",
	"discharge timeout close" };	///< Power off

String JbdBms::ErrorString() // error text for bms.html website
{ 
  uint16_t err = basicData.Protection;
  byte bit = 0;
  if (err != 0) {
	uint16_t mask = 0b0000000000000001;
	while (!(err & mask)) {
	  mask = mask<<1;
	  bit++;
	}
  }
  return String(ErrorStr[bit + 1]);
}


bool JbdBms::processBasicData(basicDataStruct *output, byte *data, unsigned int dataLen)
{
  if (data[0] + data[1] == 0) return false;				// voltage = 0?
  if (data[18] == 0 || data[21] == 0 || data[22] == 0) return false;	// num of cells or ntcs or software = 0? 
//if (data[21] != 8 && data[21] != 16) return false;		// 8 or 16 allowed
  if (data[21] % 4 != 0) return false;						// 4, 8 , 12 or 16 allowed
  output->Volts = (uint16_t)((data[0]<<8 | data[1]) * 10); // resolution 10mV -> convert to mV, eg 4895 -> 48950mV
#if false
  output->Amps =  ((int16_t)make_int(data[2], data[3])) * 10; // resolution 10mA, convert to mA
#else
  int16_t Amps = data[2]<<8 | data[3];
  if (data[2] & 0b10000000)									// negative?
	Amps = (~Amps) * -1;
  output->Amps = Amps * 10;							// resolution 10mA, convert to mA
#endif
  output->Watts = output->Volts * output->Amps / 1000;		// mW
  output->Cycles =   (uint16_t)(data[8]<<8 | data[9]);	// number of cycles
  output->Capacity = (uint16_t)(data[6]<<8 | data[7]);	// (23000 = 59D8H)
  output->SoC_Ah =   (uint16_t)(data[4]<<8 | data[5]);
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
  output->charge = fets & 1;			// Bit 0, 1 = on, 0 = off
  output->discharge = fets>>1 & 1;		// Bit 1
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
  byte _cmin = 0;							// cell number low and high
  byte _cmax = 0;
  for (byte c = 0; c < cells; c++) {		// go trough individual cells
	output->CellVolt[c] = (uint16_t)(data[c * 2]<<8 | data[c * 2 + 1]); // resolution 1mV
	_cellSum += output->CellVolt[c];
	if (output->CellVolt[c] > _cellMax) {
	  _cellMax = output->CellVolt[c];
	  _cmax = c + 1;
	}
	if (output->CellVolt[c] < _cellMin) {
	  _cellMin = output->CellVolt[c];
	  _cmin = c + 1;
	}
//	output->CellColor[c] = getPixelColorHsv(mapHue(output->CellVolt[c], c_cellAbsMin, c_cellAbsMax), 255, 255);
  }
  output->CellMin	= _cellMin;
  output->CellMax	= _cellMax;
  output->CellDiff	= _cellMax - _cellMin; // resolution 10mV -> convert to volts
  output->CellAvg	= _cellSum / cells;
  output->iMin		= _cmin;
  output->iMax		= _cmax;  
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
 if ((debugLevel & DBG_BLE1) && output->Software != 0) {
  	Serial.println(F(">> Hardware Data <<"));
	Serial.print(F("hardware: ")); Serial.print(output->Hardware);
	Serial.printf(" software: %d.%d\n", output->Software>>4, output->Software & 0x0F);
  }
  return true;
}

bool JbdBms::processBalanceData(basicDataStruct *output, byte *data, unsigned int dataLen)
{
  output->BalanceCtrlHigh = data[5];
  output->BalanceCtrlLow  = data[6];
  if (debugLevel & DBG_BLE1) {
	Serial.println(F(">> Balance Data <<"));
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
  bool debug = (debugLevel & DBG_BLE1) != 0;
  if (Checksum(packet) == rxChecksum) {
#ifdef DEBUG
	if (debug) Serial.print(F("packet ok, "));
#endif
	return true;
  } else {
#ifdef DEBUG
	if (debug) Serial.printf("packet invalid, expected value: %x\n", rxChecksum);
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
  bool debug = (debugLevel & DBG_BLE1) != 0;
  
  switch (pHeader->type) {		// decision based on packet type
	case BASICINFO: {			// responce code
	  if (debug) Serial.println(F("process basic data... "));
	  result = processBasicData(&basicData, data, dataLen);
#ifdef DEBUG
	  if (result && debug) printBasicData();
#endif
//	  showInfoLcd();
	  break;
	}
	case CELLINFO: {
	  if (debug) Serial.println(F("process cell data..."));
	  result = processCellData(&cellData, data, dataLen);
#ifdef DEBUG
	  if (result && debug) printCellData();
#endif
	  break;
	}
	case HWINFO: {
	  if (debug) Serial.println(F("process hardware data..."));
	  result = processHwData(&basicData, data, dataLen); // copy into data pack
	  break;
	}
	case BALANCE: {
	  if (debugLevel & DBG_BLE3) Serial.println(F("process balance data..."));
	  result = processBalanceData(&basicData, data, dataLen); // copy into data pack
	  break;
	}
	case MOSFET: {
	  if (debugLevel & DBG_BLE3) Serial.println(F("process MosFet control"));
	  result = true;
	  break;
	}
	default: {
	  result = false;
	  if (debug) Serial.printf("unsupported packet type: %d, ", pHeader->type); // 170 = $AA
	}
  }
  if (!result && debug) Serial.println("invalid packet received");
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
  static uint8_t packetbuff[44] = { 0x00 }; // 42 20 bytes per packet, size is important!!!
  static uint16_t packetSize = 0;
  bool retVal = false;
#ifdef DEBUG
  if (debugLevel & DBG_BLE2) hexDump(data, dataSize); // print subpacket
#endif
  if (packetstate == 0 && data[0] == STX) {		// probably got 1st subpacket
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
	  if (debugLevel & DBG_BLE2) hexDump(packet, packetSize); // print whole packet
#endif
	}
  }
  return retVal;		// true if packet is complete and valid
}

// BLE Callback function, called for each partial packet
void JbdBms::notifyCallback(BLERemoteCharacteristic *pBLERemoteCharacteristic, uint8_t *pData, size_t length, bool isNotify)
{ // this is called when BLE server sents data via notification
  newPacketReceived = collectPacket((char *)pData, length); // true if packet complete and valid
  if (newPacketReceived) dataErr = 0;
  else dataErr++;
}

//-----------------------------------------------
/*
void JbdBms::printHwData() // implemented in bmsRequestHwData()
*/

void JbdBms::printBasicData() // display basic data
{
  Serial.println(F(">> Basic Data <<"));
  Serial.printf("voltage: %.2fV  ", (float)basicData.Volts / 1000);
  Serial.printf("current: %.2fA\n", (float)basicData.Amps / 1000);
  Serial.printf("soc Ah: %.2fAh  ", (float)basicData.SoC_Ah / 100);
  Serial.printf("soc: %d%%  ", basicData.SoC);
  Serial.printf("cycles: %d\n", basicData.Cycles);
  Serial.printf("temp1: %.1f\u00B0C  ", (float)basicData.Temp[1] / 10); // 0xB0
  if (basicData.NumOfTemp > 1)
	Serial.printf("temp2: %.1f\u00B0C  ", (float)basicData.Temp[2] / 10);
  if (basicData.NumOfTemp > 2)
	Serial.printf("temp3: %.1f\u00B0C", (float)basicData.Temp[3] / 10);
  Serial.println();
  Serial.printf("balance code low: 0x%x  ", basicData.BalanceCodeLow);
  Serial.printf("balance code high: 0x%x\n", basicData.BalanceCodeHigh);
  Serial.printf("protection state: 0x%x ", basicData.Protection);
  if (basicData.Protection != 0) Serial.print(", ");
  Serial.println(ErrorString());
  Serial.printf("MosFet state: 0x%x  ", basicData.MosFet);
  Serial.print("charge: "); Serial.print((basicData.MosFet & 1) ? "on":"off");
  Serial.print(" discharge: "); Serial.println((basicData.MosFet>>1 & 1) ? "on":"off");
}


void JbdBms::printCellData() // display cell data
{
  Serial.println(F(">> Cell Data <<"));
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
//BLEScan *pBLEScan = NULL;
NimBLEAdvertisedDevice *JbdBms::bmsDevice = nullptr;

static NimBLEClient *pClient = nullptr;
static NimBLERemoteCharacteristic *pRemoteCharacteristic = nullptr;
static NimBLERemoteService *pRemoteService = nullptr;
//static NimBLERemoteDescriptor *pRemoteDescriptor = nullptr;

// the remote service we wish to connect to. Needs check/change when other BLE module as xiaoxiang used
static NimBLEUUID serviceUUID("0000ff00-0000-1000-8000-00805f9b34fb");
static NimBLEUUID charUUID_tx("0000ff02-0000-1000-8000-00805f9b34fb"); // m w
static NimBLEUUID charUUID_rx("0000ff01-0000-1000-8000-00805f9b34fb"); // m r

// Callback to process the results of the last scan or restart it
void scanEndedCallback(BLEScanResults results)
{
  Serial.println(F("end of scan"));
}


class ClientCallbacks : public NimBLEClientCallbacks
{
  void onConnect(NimBLEClient *pClient)
  {
	digitalWrite(BLE_LED, HIGH);
	Serial.print(F("connect... "));
	// After connection we should change the parameters if we don't need fast response times.
	// These settings are 150ms interval, 0 latency, 450ms timout.
	// Timeout should be a multiple of the interval, minimum is 100ms.
	// I find a multiple of 3-5 * the interval works best for quick response/reconnect.
	// Min interval: 120 * 1.25ms = 150, Max interval: 120 * 1.25ms = 150, 0 latency, 60 * 10ms = 600ms timeout
//	pClient->updateConnParams(120, 120, 0, 60);	// do we need this? https://github.com/espressif/esp-idf/issues/8303
  };

  void onDisconnect(NimBLEClient *pClient)
  {
	JbdBms::connected = false;
	JbdBms::doConnect = false;	// new!!!
	digitalWrite(BLE_LED, LOW);
	Serial.println(); Serial.print(JbdBms::data.Alias); Serial.print(": ");
	Serial.print(pClient->getPeerAddress().toString().c_str()); Serial.println(F(" disconnected, starting new scan"));
	NimBLEDevice::getScan()->start(5, scanEndedCallback);
  };

  bool onConnParamsUpdateRequest(NimBLEClient *pclient, const ble_gap_upd_params *params)
  {
	if (params->itvl_min < 24) {			// 1.25ms units
	  return false;
	} else if (params->itvl_max > 40) {	// 1.25ms units
	  return false;
	} else if (params->latency > 2) {		// Number of intervals allowed to skip
	  return false;
	} else if (params->supervision_timeout > 100) { // 10ms units
	  return false;
	}
	return true;
  }
};

// class to handle the callbacks when advertisments are received
class AdvertisedDeviceCallbacks: public NimBLEAdvertisedDeviceCallbacks
{
//AdvertisedDeviceCallbacks(BLEUUID serviceUUID) : serviceUUID(serviceUUID_), doConnect(false), doScan(false) { }

// scan for BLE servers and find the first one that advertises the service we are looking for
  void onResult(NimBLEAdvertisedDevice *advertisedDevice) // called for each advertising BLE server
  {														// only a reference to the advertised device is passed now
//	Serial.println(F("BLE advertised device found: "));
	Serial.println(String(advertisedDevice->toString().c_str())); // print Name, Address, manufacturer data, serviceUUID
	// we have found a device, let us now see if it contains the service we are looking for
	if (advertisedDevice->haveServiceUUID() && advertisedDevice->isAdvertisingService(serviceUUID)) {
//		&& (strcmp(advertisedDevice->getName().c_str(),settings.jbdbms_device_id) == 0))
	  NimBLEDevice::whiteListAdd(advertisedDevice->getAddress());
	  Serial.print(F("found BMS, "));
//	  if (strcmp(advertisedDevice->getName().c_str(), Setup::Settings.data.Name) == 0)
//	    Serial.print(F("2, "));
//	  pBLEScan->stop();
	  NimBLEDevice::getScan()->stop();			// stop scan before connecting
//	  bmsDevice = new BLEAdvertisedDevice(advertisedDevice);
	  JbdBms::bmsDevice = advertisedDevice;		// just save the reference now, no need to copy the object
	  JbdBms::doConnect = true;					// found bms server, ready to connect now
	}
  }
};

// create a single global instance of the callback class to be used by all clients
static ClientCallbacks ClientCallback;
//AdvertisedDeviceCallbacks *bleCallbacks;

void JbdBms::setup() // setup Bluetooth and scan for BLE devices
{
  pinMode(BLE_LED, OUTPUT);
  digitalWrite(BLE_LED, LOW);
  Serial.println(F("scanning Bluetooth..."));
  NimBLEDevice::init("ESP32-BMS");				// create the BLE Device, create new scan
  NimBLEDevice::setPower(ESP_PWR_LVL_P9);		// +9db
  // retrieve a scanner and set the callback we want to use to be informed when we have detected a new device
  NimBLEScan *pScan = NimBLEDevice::getScan();	// set pointer to BLEScan
//bleCallbacks = new AdvertisedDeviceCallbacks();
//pScan->setAdvertisedDeviceCallbacks(bleCallbacks);
  pScan->setAdvertisedDeviceCallbacks(new AdvertisedDeviceCallbacks());
  pScan->setInterval(100);		//1349, 100
  pScan->setWindow(99);			//449, 99		// less or equal setInterval value
  pScan->setFilterPolicy(BLE_HCI_SCAN_FILT_NO_WL); // default filter
  pScan->setActiveScan(true);					// active scan uses more power, but get results faster

  NimBLEScanResults foundDevices = pScan->start(5, false); // scan to run for 5 seconds
  Serial.printf("%d device(s), ", foundDevices.getCount());
  Serial.printf("%d BMS: ", NimBLEDevice::getWhiteListCount());
  for (auto i = 0; i < NimBLEDevice::getWhiteListCount(); ++i) {
    if (i > 1) Serial.print(", ");
	Serial.println(NimBLEDevice::getWhiteListAddress(i).toString().c_str());
  }
//pScan->clearResults();						// delete results from BLE scan buffer to release memory
  delay(2000);
}

#define MAX_RETRIES 10
// handles the provisioning of clients and connects / interfaces with the server
bool JbdBms::connect() // connect BME device, called from process()
{
//data.State = false;
//NimBLEClient *pClient = nullptr;
  // check if we have a client we should reuse first
  if (NimBLEDevice::getClientListSize()) {
	// Special case when we already know this device, we send false as the second argument in connect() 
	// to prevent refreshing the service database, this saves considerable time and power
	pClient = NimBLEDevice::getClientByPeerAddress(JbdBms::bmsDevice->getAddress());

//	Serial.print("\npC="); Serial.print(pClient->getPeerAddress().toString().c_str());
//	Serial.print(" "); Serial.println(NimBLEDevice::getClientListSize());

	if (pClient) {
	  Serial.print(F("reconnect to: "));
//	  Serial.print(String(bleCallbacks->bmsDevice->getAddress().toString().c_str()));
//	  data.Address = bleCallbacks->bmsDevice->getAddress().toString().c_str();
	  data.Address = JbdBms::bmsDevice->getAddress().toString().c_str();	// ("a4:c1:37:42:c5:e2")
	  Serial.print(data.Address); Serial.print(F("... "));
#if true // retry connection with delays
	  int retries = 0;
	  while (!pClient->connect(JbdBms::bmsDevice, false) & (retries < MAX_RETRIES)) {
		retries++;
		Serial.println(F("reconnect failed, trying again"));
		delay(2000);
	  }
	  if (retries > MAX_RETRIES) {
#else
	  if (!pClient->connect(JbdBms::bmsDevice, false)) {
#endif
		Serial.println(F("reconnect failed"));
		return false;
	  }
	  Serial.println(F(" reconnected"));

	} else { // original sample
	  // we don't already have a client that knows this device, we will check for a client that is disconnected that we can use
	  pClient = NimBLEDevice::getDisconnectedClient();

	  Serial.print("\npCdis="); Serial.println(pClient->getPeerAddress().toString().c_str());

	}
  }

  if (!pClient) {					// no client to reuse? create a new one
	if (NimBLEDevice::getClientListSize() >= NIMBLE_MAX_CONNECTIONS) {
	  Serial.println(F("max clients reached, no more connections available"));
	  return false;
	}
	pClient = NimBLEDevice::createClient();
	Serial.println(F("new client created"));	// "00:00:00:00:00:00"
	pClient->setClientCallbacks(&ClientCallback, false);
	// set initial connection parameters: These settings are 15ms interval, 0 latency, 120ms timout
	// these settings are safe for 3 clients to connect reliably, can go faster if you have less
	// connections. Timeout should be a multiple of the interval, minimum is 100ms
	// Min interval: 12 * 1.25ms = 15, Max interval: 12 * 1.25ms = 15, 0 latency, 51 * 10ms = 510ms timeout
	pClient->setConnectionParams(12, 12, 0, 51);
	// set how long we are willing to wait for the connection to complete (seconds), default is 30
	pClient->setConnectTimeout(5);
	data.Address = JbdBms::bmsDevice->getAddress().toString().c_str();	// ex. "a4:c1:37:42:c5:e2"
	Serial.print(F("pairing device: ")); Serial.print(data.Address); Serial.print(F("... "));

	if (!pClient->connect(JbdBms::bmsDevice)) {
	  // created a client but failed to connect, don't need to keep it as it has no data
	  NimBLEDevice::deleteClient(pClient);
	  pClient = nullptr;	// new!!!
	  Serial.println(F("failed to connect, deleted client"));
	  return false;
	}
  }
  if (!pClient->isConnected()) {
	if (!pClient->connect(JbdBms::bmsDevice)) {
	  Serial.println(F("failed to connect BMS"));
	  return false;
	}
  }
  Serial.print(F("connected to: ")); Serial.println(pClient->getPeerAddress().toString().c_str()); // wrong Address!!!
//Serial.print(" RSSI: "); Serial.println(pClient->getRssi());
/*
  // now we can read/write/subscribe the charateristics of the services we are interested in
  NimBLERemoteService *pRemoteService = nullptr;
  NimBLERemoteCharacteristic *pRemoteCharacteristic = nullptr;
//NimBLERemoteDescriptor *pRemoteDescriptor = nullptr;
*/
  pRemoteService = pClient->getService(serviceUUID);
  if (pRemoteService == nullptr) {
	Serial.print(F("wrong service UUID: ")); Serial.println(serviceUUID.toString().c_str());
	pClient->disconnect();
//	BLEDevice::deleteClient(pClient);
	return false;
  }
  data.ServiceUUID = serviceUUID.toString().c_str();	// save ServiceUUID
  Serial.print(F("service ok, "));

  // obtain a reference to the characteristic in the service of the remote BLE server
  pRemoteCharacteristic = pRemoteService->getCharacteristic(charUUID_rx);
  if (pRemoteCharacteristic == nullptr) {
	Serial.print(F("wrong characteristic UUID: ")); Serial.println(charUUID_rx.toString().c_str());
	pClient->disconnect();
//	BLEDevice::deleteClient(pClient);
	return false;
  }
//data.CharUUID = charUUID_rx.toString().c_str();	// save CharUUID
  Serial.print(F("characteristic ok, "));
  if (pRemoteCharacteristic->canRead()) {			// read the value of the characteristic
	Serial.print(pRemoteCharacteristic->readValue().c_str()); // (w)
  }

  if (pRemoteCharacteristic->canNotify()) {
	// registerForNotify() has been deprecated and replaced with subscribe() / unsubscribe()
	// subscribe parameter defaults are: notifications = true, notifyCallback = nullptr, response = false
	// unsubscribe parameter defaults are: response = false
//	pRemoteCharacteristic->registerForNotify(notifyCallback);
	if (!pRemoteCharacteristic->subscribe(true, notifyCallback)) {
	  pClient->disconnect();				// disconnect if subscribe failed
	  return false;
	}
  }
  JbdBms::connected = true;					// local vars
  JbdBms::doConnect = false;				// bms pairing ended
  data.State = true;
  digitalWrite(BLE_LED, HIGH);
  return true;
}


void JbdBms::disconnect() // does not work as intended, but automatically reconnected
{
  pClient->disconnect();
  JbdBms::connected = false;
  data.State = false;
  bmsCmd = PAUSE;
  dataOk = 0;						// no valid packets received
  Serial.println(F("disconnected from BMS"));
  digitalWrite(BLE_LED, LOW);
}


void JbdBms::process() // request data from BLE server, called in loop()
{
  static unsigned int requestCnt;		// HWINFO only after 10th loop
  // if the flag "doConnect" is true then we have scanned for and found the desired BLE Server with which we wish to connect
  // Now we connect to it.  Once we are connected we set the connected flag to be true.
//if (bleCallbacks->doConnect == true) {
  if (doConnect) { // pair device?
	if (connect()) {								// pairing ok?
//	  data.Name = bleCallbacks->bmsDevice->getName().c_str();
//	  data.RSSI = bleCallbacks->bmsDevice->getRSSI();
//	  data.TXPower = bleCallbacks->bmsDevice->getTXPower();
	  data.Name = JbdBms::bmsDevice->getName().c_str();
	  data.RSSI = JbdBms::bmsDevice->getRSSI();
	  data.TXPower = JbdBms::bmsDevice->getTXPower();

	  // *** root topic setting ***
	  strcpy(data.Alias, BMS_ALIAS); 				// set default alias (includes \0)
	  char bmsNum = '0';
	  if (data.Name[8] == '2') bmsNum = '1';
	  else if (data.Name[8] == '3') bmsNum = '2';
	  data.Alias[strlen(data.Alias) - 1] = bmsNum;	// set bms alias: BMS1, BMS2
	  Serial.print(F("connected to ")); Serial.print(data.Alias); Serial.print(": "); Serial.println(data.Name);
//	  JbdBms::doConnect = false;					// bms pairing ended
	  bmsCmd = CONNECT;								// activate bms requests
/*
	  // send MQTT message
//	  pRemoteCharactristic->writeValue(getdeviceData, 20);	// sending getdevice info
	  sendingtime = millis();
	  Serial.println("sending device Data");
	  String topic = mqttname + "/BLEconnection";
	  mqttClieent.publish(topic.c_str(),"connected");
*/
	  delay(200);
	}
  }
//JbdBms::doConnect = false;				// bms pairing ended (new) ???
  // if we are connected to a peer BLE Server, update the characteristic each time we are reached
  // with the current time since boot
  if (connected) {
	ulong ms = millis();
	if (ms - lastRequested >= refresh) { // every bms refresh time period
	  lastRequested = ms;

	  if (newPacketReceived && (dataErr == 0))
		dataOk++;						// used by mqtt publisch and notify ws clients
		else dataOk = 0;
	  Serial.print("#"); Serial.print(bmsCmd); Serial.print(newPacketReceived); Serial.print(dataErr);

	  switch (bmsCmd) {
		case BASICINFO: {
		  bmsRequestBasicData();		// get basic data
		  bmsCmd = CELLINFO;			// next get cell info
		  break;
		}
		case CELLINFO: {
		  bmsRequestCellData();
		  requestCnt++;
		  if (requestCnt % 10 == 0) bmsCmd = HWINFO;
		  else bmsCmd = BASICINFO;		// next get basic data
		  break;
		}
		case HWINFO: {
		  bmsRequestHwData();
 		  bmsCmd = BASICINFO;			// next get basic data
		  break;
		}
		case BALANCEINFO: {
		  bmsRequestBalanceData();
 		  bmsCmd = BASICINFO;			// next get basic data
		  break;
		}
		case BALANCE: {
		  bmsBalanceCtrl();
		  bmsCmd = BASICINFO;
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
		  bmsCmd = BASICINFO;			// actvates BleRequestData() 
		  break;
		}
		case PAUSE: {					// no bms request
		  break;
		}
//		default: {
//		  break;
//		}
	  }
	}
  }
}

//---------------------------- BMS server requests

void JbdBms::bmsSendCmd(uint8_t *data, uint32_t dataLen)
{
  pRemoteCharacteristic = pRemoteService->getCharacteristic(charUUID_tx);
  bool debug = ((debugLevel & DBG_BLE1) != 0 || bmsCmd == MOSFET || bmsCmd == BALANCE);
  if (pRemoteCharacteristic) {				// make sure it's not null
	pRemoteCharacteristic->writeValue(data, dataLen);
//	pRemoteCharacteristic->writeValue(newValue.c_str(), newValue.length());
#ifdef DEBUG
	if (debug) Serial.print(F("\nBMS request "));
  } else {
	if (debug) Serial.println(F("remote TX characteristic not found"));
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
  if (debugLevel & DBG_BLE1) Serial.print(F("basic data... "));
#endif
}

void JbdBms::bmsRequestCellData()	// request BMS Cell Data
{ //   DD  A5  04  00  FF  FC  77
  uint8_t packet[7] = { STX, READ, CELLINFO, 0x00, 0xFF, 0xFC, ETX };
  bmsSendCmd(packet, sizeof(packet));
#ifdef DEBUG
  if (debugLevel & DBG_BLE1) Serial.print(F("cell data... "));
#endif
}

void JbdBms::bmsRequestHwData()		// request BMS Hw Data
{ //   DD  A5  05  00  FF  FB  77
  uint8_t packet[7] = { STX, READ, HWINFO, 0x00, 0xFF, 0xFB, ETX };
  bmsSendCmd(packet, sizeof(packet));
#ifdef DEBUG
  if (debugLevel & DBG_BLE1) Serial.print(F("hardware data... "));
#endif
}

void JbdBms::bmsRequestBalanceData() // request BMS Balance Data
{ //   DD  A5  2D  02  FF  FC  77
  uint8_t packet[7] = { STX, READ, BALANCE, 0x02, 0xFF, 0xD3, ETX };
//packet[5] = bch; packet[6] = bcln;
  packet[6] = Checksum(packet);
//bmsSendCmd(packet, sizeof(packet));
#ifdef DEBUG
  if (debugLevel & DBG_BLE3) Serial.print(F("balance data... "));
#endif
}

void JbdBms::bmsMosfetCtrl()	// MosFet Control
{
/* mosfets, Bit 0 = charge, Bit 1 = discharge
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
  if (debugLevel & DBG_BLE3) {
	Serial.print("MosFet control " + String(mosfets) + ", sent: ");
	hexDump(packet, sizeof(packet));
  }
#endif
}

void JbdBms::setBMS(bool state)
{
  if (state)
	bmsCmd = CONNECT; else bmsCmd = PAUSE;
}

void JbdBms::setCharge(bool state)
{
  if (state) {
	mosfets = basicData.MosFet | 1;			// set bit0
	basicData.charge = true;
  } else {
	mosfets = basicData.MosFet & ~1;		// clear bit0
	basicData.charge = false;
  }
  bmsCmd = MOSFET; 
}

void JbdBms::setDischarge(bool state)
{
  if (state) {
	mosfets = basicData.MosFet | 2;			// set bit1
	basicData.discharge = true;
  } else {
	mosfets = basicData.MosFet & ~2;		// clear bit1
	basicData.discharge = false;
  }
  bmsCmd = MOSFET; 
}


void JbdBms::bmsBalanceCtrl()	// Balance Control
{
/* balance
	4  100b = balance = on
	2  010b = charge balance = on
	1  001b = switch on

Balance control 0, sent: dd 5a e1 02 00 07 ff 16 77 
Balance control 4, sent: dd 5a e1 02 00 03 ff 1a 77 
*/
  byte balanceByte = balance ^ 7;
  uint8_t packet[9] = { STX, WRITE, BALANCE, 0x02, 0x00, 0x00, 0xFF, 0x00, ETX };
  packet[5] = balanceByte;
  packet[7] = Checksum(packet);
//bmsSendCmd(packet, sizeof(packet)); // send to bms
#ifdef DEBUG
  if (debugLevel & DBG_BLE3) {
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
