/**
 * @file Debug.h
 *
 * MySensors Library 2.0
 *
 * small monitor functions header
 *
 * last modified: 14.08.16
 *
 * Commands:
 *
 * ra [pin]		-> read analog pins
 * rd [pin]		-> read digital pins
 * rb [addr]	-> read EEPROM byte
 * rw [addr]	-> read EEPROM word
 * rp [num]		-> read parameter 0..5
 * wd pin value	-> write digital pin
 * wa pin value	-> write analog pin
 * wb addr data	-> write EEPROM byte
 * ww addr data	-> write EEPROM word
 * wp num value   -> write parameter
 * de addr size	-> dump EEPROM
 * df addr size	-> dump FLASH
 * dr addr size	-> dump RAM
 * t			-> display time
 * p			-> display parameters
 * p1 node value
 * l			-> display lan configuration
 * a mode		-> set mode, 1 = automatic
 * m			-> manual
 * d level		-> debug level
 * ts			-> seconds
 * tm period	-> mqtt period
 * tb period	-> bms period
 * pr			-> read parameter from eeprom
 * pw			-> write current parameter to eeprom
 * z			-> erase parameters
 *
 */
#ifndef __DEBUG_H__
#define __DEBUG_H__

#include <Arduino.h>
#include <EEPROM.h>
#include <ESPDateTime.h>
#include "Setup.h"
#include "JbdBms.h"

#define CTRL(x) (x - 64)
#define DEL 127
#define CR  0x0D	// '\r'
#define LF  0x0A	// '\n'
#define BS  8


extern Setup Settings;			// Settings point to Setup instance
//extern JbdBms Bms;				// Bms is a pointer to JbdBms class
/*
namespace MyLOG
{
  extern bool DISABLE_LOGD;
  void LOGD(String tag, String text);
}
*/
//-------------------
//#include <SimpleMessageSystem.h>	// include SimpleMessageSystem library

// Functions:
bool commandAvailable(void); // if the message has been terminated, returns true
// WARNING: if you make a call to commandAvailable() it will discard any previous message!

char commandChar1();		// If a word is available, returns it as a char. If no word is available it will return 0
char commandChar2();
// WARNING: if you send something like "foo", it will return 'f' and discard "oo".
int getInt();				// If a word is available, returns it as an integer. If no word is available it will return 0


typedef enum {
  ERR, RAM, FLASH, ROM, TIMER, ERROR, REGS, IRQS, WATCHES, RAMAVAIL, IDLE
} state_t;

// function prototypes:
void serialEventRun();			// process incoming message from terminal
void CmdRead(void),				// read pins (analog or digital)
	 CmdWrite(void),			// write pin/memory
	 CmdTone(void),
	 CmdDump(void),
	 CmdParameters(void),
	 eraseVars(void);
byte getByte(uint16_t addr, state_t type);
void dump(void const* start, uint16_t size, state_t type);

/*
void initParameters(void);					// load parameters from eeprom into parameters array
int loadParameter(byte idx);				// load parameter from eeprom
void saveParameter(byte idx, int value);// save parameter in eeprom

void SerialprintNodeId(),
	 SerialprintParameter(byte idx),
	 SerialprintParameters(void),
	 SerialprintMenu(void),
	 SerialprintByte(byte x),
	 SerialprintWord(int x);

void dumpRam(void const* start, int size),
	 dumpEeprom(void const* start, int size);

void dump(void const* start, uint16_t size, state_t type);

// function prototypes:
void SerialprintTime(void),	// utility function for digital clock display: prints preceding colon and leading 0
	 SerialprintTimeDate(void);
*/
#endif /* __DEBUG_H__ */
