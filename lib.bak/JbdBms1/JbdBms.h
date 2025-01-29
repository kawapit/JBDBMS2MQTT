#ifndef __JBDBMS_H__
#define __JBDBMS_H__

#include <Arduino.h>
//#include "Debug.h"
//#include "Setup.h"	// (BMS_REFRESH)
#include "BLEDevice.h"
#include "BLEClient.h"
//#include "MyAdvertisedDeviceCallbacks.h"
//#include "MyClientCallback.h"

#define BLE_LED 2
#define BMS_ALIAS "BMS0"
#define BMS1_ADDRESS	E0:9F:2A:FD:2E:60
#define BMS2_ADDRESS	A4:C1:37:42:C5:E2

// BMS control codes:
#define STX			0xDD
#define ETX			0x77
#define READ		0xA5
#define WRITE		0x5A

#define PAUSE		0		// no bms requests
#define CONNECT		1		// connect bms, activate requests
#define DISCONNECT	2		// disconnect bms
#define BASICINFO	3		// read basic information and status
#define CELLINFO	4		// read battery cell voltage
#define HWINFO		5		// read the protection board hardware version number
#define BALANCE		0x2D
#define MOSFET		0xE1

// BMS protection state (Protection)
// This method return the uint16_t value
// If this value is not 0, then BMS detected some errors. You can check this value on the mask by using the following error values:
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
#define BMS_STATUS_SHORT_CIRCOUT	1024	///< Power off
#define BMS_STATUS_AFE_ERROR		2048
#define BMS_STATUS_SOFT_LOCK		4096
#define BMS_STATUS_CHGOVERTIME		8192
#define BMS_STATUS_DSGOVERTIME		16384	///< Power off
#define BMS_POWER_OFF_ERRORS		0x46CA
// Some errors lead to power down (labeled "Power off").
// if you want to read them a device that is powered from the battery, it will not work

typedef struct {
	byte start;						// Stx = 0xDD
	byte type;						// read/write
	byte status;					// 0 = ok
	byte dataLen;					// length of data
} BmsPacketHeaderStruct;

typedef struct {
	bool State;						// true = paired
	String Name;					// bms name
	char Alias[5];					// BMSx
	String Address;					// bms address (a4:c1:37:42:c5:e2)
	String ServiceUUID;
	String CharUUID;
	int RSSI;
	byte TXPower;
} bmsDataStruct;

typedef struct {
	char Hardware[16];				// bms hardware version (BS202308-01-104)
	uint8_t	Software;				// bms software version (0.0)
	uint16_t Volts;					// voltage in mV (0..65.535mV)
	int32_t Amps;					// current in mA
	int32_t Watts;					// power in mW
	uint16_t Cycles;				// cycles
	uint16_t Capacity;				// capacity in Ah*100
	uint16_t SoC_Ah;				// remaining capacity in Ah*100
//	uint32_t SoC_Wh;				// Wh
	uint8_t SoC;					// state of charge in %
	uint8_t NumOfCells;				// number of cells
	uint8_t NumOfTemp;				// number of NTCs
	uint8_t MosFet, charging, discharging;
//	uint16_t Temp1, Temp2, Temp3;	// temperatures in 0.1deg
	uint16_t Temp[4];				// temp average, temp1..3
	uint16_t BalanceCodeLow, BalanceCodeHigh;
	uint8_t BalanceCtrlLow, BalanceCtrlHigh;
	uint16_t Protection;			// protection state
//	uint8_t SoCmin, SoCmax;
} basicDataStruct;

typedef struct {					// all in mV (0..65.535mV)
	uint16_t CellVolt[8];			// max 16 cell battery, cell 1 has index 0 :-/
	uint16_t CellMax, CellMin;		// maximal, minimal voltage in cells
	uint16_t CellDiff;				// difference between highest and lowest
	uint16_t CellAvg;				// average voltage in cells
	uint16_t CellMedian;
	byte iMax, iMin;				// cell number with lowest/highest voltage
//	uint32_t CellColor[15];
//	uint32_t CellColorDisbalance[15]; // green cell == median, red/violet cell => median + c_cellMaxDisbalance
} cellDataStruct;

//extern Setup Settings;			// Settings point to Setup instance

class JbdBms
{
  private:
	static ulong lastRequested;
	static void bmsRequestBasicData();
	static void bmsRequestCellData();
	static void bmsRequestHwData();
	static void bmsRequestBalanceData();
	static void bmsMosfetCtrl();
	static void bmsBalanceCtrl();
	static bool connect();
	static void bmsSendCmd(uint8_t *data, uint32_t dataLen);

//	static BLEClient *pClient;
//	static BLEAdvertisedDevice *bmsDevice;	// new
//	static BLERemoteCharacteristic *pRemoteCharacteristic;
//	static BLERemoteService *pRemoteService;

	static bool processBasicData(basicDataStruct *output, byte *data, unsigned int dataLen);
	static bool processCellData(cellDataStruct *output, byte *data, unsigned int dataLen);
	static bool processHwData(basicDataStruct *output, byte *data, unsigned int dataLen);
	static bool processBalanceData(basicDataStruct *output, byte *data, unsigned int dataLen);
	static byte Checksum(byte *packet);				// calc checksum
	static bool ChecksumOk(byte *packet);			// check if packet crc is valid
	static bool PacketOk(byte *packet);				// check if single packet from bms
	static bool collectPacket(char *data, uint32_t dataSize); // reconstruct packet from BMS incomming data, called by notifyCallback function
	static void notifyCallback(BLERemoteCharacteristic *pBLERemoteCharacteristic, uint8_t *pData, size_t length, bool isNotify);

  public:
	static ulong refresh;
	static char bmsNum;			// used for alias and topic
	static byte bmsCmd;			// parameter for bms request task
	static byte mosfets;		// parameter for mosfet control
	static byte balance;		// parameter for balance control
	static byte debugLevel;
/*
	static const int32_t c_cellNominalVoltage; // mV
	static const uint16_t c_cellAbsMin;
	static const uint16_t c_cellAbsMax;
	static const int32_t c_packMaxWatt;
	static const uint16_t c_cellMaxDisbalance;
*/
//	static MyAdvertisedDeviceCallbacks *bleCallbacks;
//	static MyClientCallback *bmsCallback;
//	static BLEUUID serviceUUID;
//	static BLEUUID charUUID_tx;
//	static BLEUUID charUUID_rx;

	static bool connected;				// bms connected
	static bool doConnect;
	static bool doScan;
	static bool newPacketReceived;
	static byte bleRetrys;
	static byte dataOk;					// bms data ok
	static byte dataErr;				// bms data not ok

	static bmsDataStruct data;
	static basicDataStruct basicData;	// here shall be the latest data got from BMS
	static cellDataStruct cellData;		// here shall be the latest data got from BMS

	static void setup();
	static void disconnect();			// does not work as intended, but automatically reconnected
	static void process();

	void setCharging(bool state), setDischarging(bool state),
		 setBalanceEnable(bool state), setChargeBalance(bool state);
	byte getDebugLevel();
	void setDebugLevel(byte level);
	static void printBasicData();		// debug all data to uart
	static void printCellData();		// debug all data to uart
	static void hexDump(void *ptr, uint32_t buflen);
};
#endif /* __JBDBMS_H__ */
