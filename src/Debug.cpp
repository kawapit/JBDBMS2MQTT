/**
 * @file Debug.cpp
 *
 * MySensors Library 2.0
 *
 * small monitor functions
 *
 * last modified: 19.08.16
 *
 */
#ifndef __DEBUG_CPP__
#define __DEBUG_CPP__

#include "Debug.h"
/*
bool MyLOG::DISABLE_LOGD = false;

void MyLOG::LOGD(String tag, String text)
{
  if (!DISABLE_LOGD) {
	Serial.print("[" + DateTime.toString() + "] ");
	Serial.print(tag + ": "); Serial.println(text);
  }
}
*/

// the size of the incomming message buffer (Arduino 0004 has a serial in buffer of 64 bytes)
#define COMMAND_BUFFER_SIZE 32 //64

// local variables:
typedef struct {
  char string[COMMAND_BUFFER_SIZE];
  byte idx;
} lineBuffer;

static lineBuffer command;			// input buffer for command interpreter

static char *stringPtr = command.string;
static char *tokenPtr = NULL;


bool tokenAvailable(void)
{
  tokenPtr = strtok_r(stringPtr, " ", &stringPtr); // split using space
  if (tokenPtr != NULL) return true;
  return false;
}


char commandChar1() // returns 1st character of command line
{
  if (tokenAvailable()) return tokenPtr[0];
  return 0;
}


char commandChar2() // returns 2nd character of command line
{
  return tokenPtr[1];
}

/*
ulong htoi(char *a, unsigned int len)
{
  ulong val = 0;

  for (int i = 0; i < len; i++)
	if (a[i] <= '9')
	  val += (a[i] - '0') * (1<<(4 * (len - 1 - i)));
	else
	  val += (a[i] - '7') * (1<<(4 * (len - 1 - i)));
  return val;
}
*/
 // take a hex string and convert it to a 32bit number (max 8 hex digits)
ulong htoi(char *hex)
{
  ulong val = 0;

  while (*hex) {
	// get current character then increment
	char byte = *hex++;
	// transform hex character to the 4bit equivalent number, using the ascii table indexes
	if (byte >= '0' && byte <= '9') byte = byte - '0';
	else if (byte >= 'a' && byte <='f') byte = byte - 'a' + 10;
	else if (byte >= 'A' && byte <='F') byte = byte - 'A' + 10;
	// shift 4 to make space for new digit, and add the 4 bits of the new digit
	val = (val<<4) | (byte & 0xF);
  }
  return val;
}


int getInt(void) // returns token of command line as integer
{
  if (tokenAvailable()) {
	if ((char)tokenPtr[0] == '$')
	  return (int)strtol(++tokenPtr, (char **)NULL, 16);
//	  return htoi(++tokenPtr);
	else
	  return atoi(tokenPtr);
  }
  return 0;
}


bool commandAvailable(void)
{
  int size = 0;
  while (Serial.available()) {
	const char cha = Serial.read();
	if (command.idx < COMMAND_BUFFER_SIZE - 1) {
	  if (cha == DEL) {					// ignore Del
	  } else if (cha == LF) {			// ignore Lf
	  } else if (cha == BS) {			// destructive backspace: remove last character
		if (command.idx > 0) {
		  Serial.print("\010 \010");
		  command.string[--command.idx] = 0;
		}
	  } else if (cha == CTRL('R')) {	// Ctrl-R retypes the line
		Serial.print("\r\n");
		Serial.print(command.string);
	  } else if (cha == CTRL('U')) {	// Ctrl-U deletes the entire line and starts over
		Serial.println();
		memset(command.string, 0, sizeof(command.string));
		command.idx = 0;
		// if newline then command is complete
	  } else if (cha == CR) {			// Cr
		size = command.idx;
		command.string[command.idx] = 0;// add string terminator and prepare for the next input
		command.idx = 0;
		stringPtr = command.string;
		return true;
	  } else {							// add it to the commandstring
		command.string[command.idx++] = cha;
		Serial.print(cha);				// echo character
	  }
	} else {
	  Serial.println("error!");
	  command.idx = 0;
	  break;
	}
  }
  return false;				// 1 = command available
}

/*

//struct parameterConfig_t { uint8_t parameter[5]; };
extern parameterConfig_t parameters;		// parameter array

extern const char menuCmds[];
extern const char menuUnits[];
extern const char* const menuText[];
*/

void SerialprintTime(void)
{
  DateTimeParts p = DateTime.getParts();
  Serial.printf("%02d:%02d:%02d\n", p.getHours(), p.getMinutes(), p.getSeconds());
}


void SerialprintTimeDate(void)
{
  DateTimeParts p = DateTime.getParts();
  Serial.printf(" %04d-%02d-%02d %02d:%02d:%02d\n",
	p.getYear(), p.getMonth(), p.getMonthDay(), p.getHours(), p.getMinutes(), p.getSeconds());
}

// ************* handle serial receive ***************
//
void serialEventRun()				// process incoming message from terminal
{
  if (commandAvailable() > 0) {	// checks to see if the message is complete and erases any previous messages

	switch (commandChar1()) {		// gets the first token as a character
	case 'r':						// read ?
	  CmdRead();					// call the read function
	  break;						// break from the switch
	case 'w':						// write ?
	  CmdWrite();					// call the write function
	  break;
case 'd':							// dump ?
//	  CmdDump();					// call the dump function
	  break;
#ifdef SENSOR_NETWORK
	case 'p':						// parameter?
	  Serial.println();
	  CmdParameters();				// change radio parameters
	  break;
	case 'z':
	  eraseVars();
	  break;
	case 'm':						// menu?
	  SerialprintMenu();
	  break;
#else
	case 'p':
	  Serial.println();
	  Settings.print();				// display parameters
	  break;
#endif
#ifdef EEPROM_CONFIG
	case 'l':						// lan?
	  Serial.println();
	  SerialprintLanConfig();		// display lan configuration
	  break;
#endif

	case 't':						// time?
	  Serial.write('=');
	  SerialprintTimeDate();		// display time
	  break;

//	case 't':						// time?
//	  CmdTone();
//	  break;
#if defined LEDS || defined ROLLO
	case 'a':						// automatic?
	  automatic = true;
	  Serial.println();
	  break;
#endif
#ifdef DEMO
	case 'o':						// demo?
	  demo = getInt();
	  Serial.println();
	  break;
	case 'i':
	  triggered = true;
	  Serial.println();
	  break;
#endif
	}
  }
}


void SerialprintByte(byte data)
{
  Serial.print("=$");
//Serial.print(data < 16 ? "0" : "");
  if (data < 16) Serial.write('0');
  Serial.print(data, HEX);
}

void SerialprintWord(int data)
{
  char buffer[5];
  sprintf(buffer, "%04x", data);
  Serial.print("=$"); Serial.print(buffer);
}


void CmdRead()							// read pins (analog or digital)
{
  int addr, val;

  char cha = commandChar2();			// gets the next byte as a character
  if (cha == 'd') {						// read digital pin "rd"
	Serial.write('=');
	addr = getInt();					// gets the next word as an integer
	if (addr)
	  Serial.print(digitalRead(addr));
	else {
	  for (char i = 2; i < 14; i++) {
		Serial.print(digitalRead(i), HEX);// read pins 2 to 13
		Serial.write(' ');
	  }
	}
  } else if (cha == 'a') {				// read analog pin "ra"
	Serial.write(':');
	addr = getInt();					// gets the next word as an integer
	if (addr)
	  Serial.print(analogRead(addr));
	else {
	  for (char i = 0; i < 6; i++) {
		Serial.write('$');
		Serial.print(analogRead(i), HEX);// read pins 0 to 5
		Serial.write(' ');
	  }
	}
  } else if (cha == 'b') {				// read EEPROM byte "rb"
	addr = getInt();					// gets the next word as an integer
	Serial.write(':');
	val = EEPROM.readByte(addr);
	Serial.print(val); SerialprintByte(val);
  } else if (cha == 'w') {				// read EEPROM word "rw"
	addr = getInt();					// gets the next word as an integer
	Serial.write(':');
	val = EEPROM.readShort(addr);
	Serial.print(val); SerialprintWord(val);
  } else if (cha == 'p') {				// read EEPROM parameter "rp"
	Serial.write(':');
	addr = getInt();
//	val = loadParameter(addr);
	Serial.print(val); SerialprintWord(val);
  }
  Serial.println();
}


void CmdWrite()							// write pin/memory
{
  int addr, val;

  char cha = commandChar2();			// get 2nd command char
  if (cha == 'a') { 					// write an analog pin "wa"
	addr = getInt();					// get pin
	val = getInt();						// get data
	pinMode(addr, OUTPUT);				// set the state of the pin to an output
	analogWrite(addr, val);				// set the PWM of the pin 
  } else if (cha == 't') { 			// write an digital pin "wt"
	addr = getInt();					// get pin
	val = getInt();						// get frequency
	if (val == 0)
	  noTone(addr);						// tone off
	else
	  tone(addr, val);					// set tone of the pin
  } else if (cha == 'd') { 			// write a digital pin "wd"
	addr = getInt();					// get pin
	val = getInt();						// get state
	pinMode(addr, OUTPUT);				// set the state of the pin to an output
	digitalWrite(addr, val);			// set the state of the pin HIGH (1) or LOW (0)
  } else if (cha == 'b') {				// write EEPROM byte "wb"
	addr = getInt();					// get address
	val = getInt();						// get data
	EEPROM.writeByte(addr, val);
  } else if (cha == 'w') {				// write EEPROM word "ww"
	addr = getInt();					// get address
	val = getInt();						// get data
	EEPROM.writeShort(addr, val);
  } else if (cha == 'p') {				// write EEPROM parameter "wp"
	addr = getInt();
	val = getInt();
//	saveParameter(addr, val);
  }
  Serial.println();
}


void CmdTone(void)
{
  uint16_t addr, val;

  addr = getInt();						// get pin
  if (addr) {
	val = getInt();						// get frequency
	if (val == 0)
	  noTone(addr);
	else
	  tone(addr, val);					// set tone on this pin
	Serial.println();
  } else {
	Serial.write('=');
	SerialprintTimeDate();		// display time
  }
}


void CmdDump(void)						// dump memory
{
  int *addr, size;
  state_t type = ERR;

  char cha = commandChar2();			// gets the next byte as a character
  if (cha == 'e') {						// dump EEPROM
	type = ROM;
  } else if (cha == 'f') {				// dump Flash
	type = FLASH;
  } else if (cha == 'r') {				// dump RAM
	type = RAM;
  }
  *addr = getInt();						// get start address
  size = getInt();						// get number of bytes
  if (!size) size = 256;
//if (type != ERR) dump(addr, size, type);
}
/*
// p1 31 50
void CmdParameters(void)				// send parameters
{
  MyMessage tmpMsg;						// build a new message and send parameter to slave
  bool ok;

  char cha = commandChar2();			// get command
  if (cha =='r') {						// "pr"
	SerialprintParameters();
	return;

  } else if (cha =='0') {				// interval "p0"
	tmpMsg.setType(V_CUSTOM);

  } else if (cha > '0' && cha < '6') {	// parameter 0..5 "p1..5"
	tmpMsg.setType(V_VAR1 + cha -'1');
  }
  tmpMsg.sender		 = GATEWAY_ADDRESS;	// simulate gateway ??
  tmpMsg.destination = getInt();		// get node-id
  tmpMsg.last		 = transportConfig.parentNodeId;
  tmpMsg.setSensor(getInt());			// get child
  tmpMsg.set(getInt());				// get value

  ok = sendRoute(tmpMsg);				// send message
  Serial.println();
}


void eraseVars(void)
{
//Serial.println("started clearing. Please wait...");
  for (int i = 0; i < EEPROM_LOCAL_CONFIG_ADDRESS; i++) {
	EEPROM.update(i, 0xFF);
  }
  Serial.println("clearing done");
}
//#endif // MySensors

#ifndef PARAMETERS
// define standard menu text and pointers
const char menuCmds[] PROGMEM = { '1','2','3','4','5' };
const char menuUnits[] PROGMEM = { '?','?','?','?','?' };
const char menuText1[] PROGMEM = "par1";
const char menuText2[] PROGMEM = "par2";
const char menuText3[] PROGMEM = "par3";
const char menuText4[] PROGMEM = "par4";
const char menuText5[] PROGMEM = "par5";

const char* const menuText[] PROGMEM = { menuText1, menuText2, menuText3, menuText4, menuText5 };
#endif
/*
void initParameters(void) // load parameters from eeprom into parameters array
{
  interval = EEPROM.readShort((uint16_t*)EEPROM_INTERVAL_ADDRESS);
  if (interval == 0xFFFF) {
	interval = SLEEP_TIME;
	eeprom_update_word((uint16_t*)EEPROM_INTERVAL_ADDRESS, interval);
  }
  eeprom_read_block((void*)&parameters, (void*)EEPROM_PARAMETERS_ADDRESS, sizeof(parameterConfig_t));	// 5 words
}


int loadParameter(byte idx) // load interval or parameter 0..5 from eeprom
{
  byte i = idx;
  if (i == 0) {
	return EEPROM.readShort((uint16_t*)EEPROM_INTERVAL_ADDRESS);
  } else if (i > 0 && i < 6)
	return EEPROM.readShort((uint16_t*)(EEPROM_PARAMETERS_ADDRESS + (i - 1) * 2));
  return 0xFFFF;
}


void saveParameter(byte idx, int value) // save parameter 0..5 value into eeprom
{
  byte i = idx;
  int val = value;
  if (i == 0) eeprom_update_word((uint16_t*)EEPROM_INTERVAL_ADDRESS, val);
  else if (i > 0 && i < 6)
	eeprom_update_word((uint16_t*)(EEPROM_PARAMETERS_ADDRESS + (i - 1) * 2), val); // update value in EEPROM
}


void SerialprintNodeId()
{
  uint16_t id = EEPROM.readShort((const uint16_t*)EEPROM_NODE_ID_ADDRESS);

  Serial.print("node id="); Serial.print((uint8_t)lowByte(id));
  Serial.print(" parent="); Serial.println((uint8_t)highByte(id));
}


void SerialprintParameter(byte idx) // print interval or value from parameters array
{
  byte i = idx;
  char buffer[12];
  if (i == 0) {
	Serial.print("interval="); Serial.print(interval); Serial.println('s');
  } else if (i > 0 && i < 6) {
    i--;
	strcpy_P(buffer, (char*)pgm_read_word(&(menuText[i]))); // necessary casts and dereferencing, just copy
	Serial.print(buffer);
//	for (int j = 0; j < sizeof(menuText1) - 1; j++) {
//	  Serial.write((char*)pgm_read_byte_near(&(menuText[i * 11 + j])));
//	}
	Serial.write('='); Serial.print(parameters.val[i]);
	Serial.write(pgm_read_byte_near(&(menuUnits[i])));
	Serial.println();
  }
}


void SerialprintMenu(void)
{
  Serial.println();
//for (int i = 0; i < sizeof(menuCmds); i++) {
  for (int i = 0; i < V_VAR5 - V_VAR1 + 1; i++) {
	Serial.write(pgm_read_byte_near(&(menuCmds[i]))); Serial.write(' ');
	SerialprintParameter(i + 1);
  }
}

byte getByte(int addr, state_t type)
{
  if (type == RAM)
	return *((int *)addr);
  else if (type == FLASH)
#if defined(ARDUINO) || defined(MINI) || defined(NANO) || defined(RBOARD)
    return pgm_read_byte_near(addr);
#elif defined(MEGA) || defined (IBOARD_PRO)
	if (addr < 0x10000)
	  return pgm_read_byte_near(addr);
	else
	  return pgm_read_byte_far(addr);
#endif
  else if (type == ROM)
	return EEPROM.readByte(addr);
  else
	return 0;
}

//void dump(void const* start, int size, byte (*memByte)(const void*))
//void dump(void const* start, int size)
void dump(void const* start, uint16_t size, state_t type)
{
  Serial.println();
  while (size > 0) {
//	Serial.print("0x");
	Serial.write('$');
//	Serial.print((ulong)start < 0x10 ? "000" : (ulong)start < 0x100 ? "00" : (unsigned long)start < 0x1000 ? "0" : "");
	Serial.print((ulong)start < 0x10 ? "00" : (ulong)start < 0x100 ? "0" : "");
	Serial.print((ulong)start, HEX);
	Serial.print(": ");
	for (byte i = 0; i < 16; i++) {
	  if (i == 8) Serial.write(' ');
	  if (size - i > 0) {
		// because ISO C forbids `void*` arithmetic, we have to do some funky casting
		void *addr = (void *)((uint16_t)start + i);
		byte data = getByte((int)addr, type); // byte data = pgm_read_byte((uint16_t*)addr);
//		Serial.print(data < 16 ? "0" : "");
		if (data < 16) Serial.write('0');
		Serial.print(data, HEX);
//		Serial.write(data == 0x97 ? '=' : ' ');
		Serial.write(' ');
	  } else Serial.write(' ');
	}
	Serial.write(' ');
	for (byte i = 0; i < 16 && size; i++, size--) {
      // because ISO C forbids `void*` arithmetic, we have to do some funky casting
	  byte data = getByte((uint16_t)start, type); // byte data = pgm_read_byte((uint16_t*)start);
	  start = (void *)((int16_t)start + 1);
	  if (i == 8) Serial.write(' ');
	  Serial.write(data >= ' ' ? data : '.');
//	  Serial.write(data >= ' ' && data <= 'z' ? data : '.');
	}
	Serial.println();
  }
}

byte ramByte(const void* addr) { return *(char *)addr; }

byte eepromByte(const void* addr) { return EEPROM.readByte(addr); }

void dumpRam(void const* start, int size)
{
  return dump(start, size, ramByte);
}

void dumpEeprom(void const* start, int size)
{ 
  return dump(start, size, eepromByte);
}
*/
#endif /* __DEBUG_CPP__ */
