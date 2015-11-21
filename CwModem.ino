/*
 *
 *
 *    CwModem.ino
 *
 *    A simple terminal for CW.
 *
 *    License: GNU General Public License Version 3.0.
 *    
 *    Copyright (C) 2014 by Matthew K. Roberts, KK5JY. All rights reserved.
 *
 *    This program is free software: you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation, either version 3 of the License, or
 *    (at your option) any later version.
 *    
 *    This program is distributed in the hope that it will be useful, but
 *    WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *    General Public License for more details.
 *    
 *    You should have received a copy of the GNU General Public License
 *    along with this program.  If not, see: http://www.gnu.org/licenses/
 *
 *
 */
//#define VERBOSE_DEBUG
//
//  Pin Definitions - These are all used for digital I/O,
//                    so any of the analog or digital pins
//                    can be used for any function.  When
//                    analog pins are used, the A# macros
//                    should be used (e.g., A0, A1, etc.)
//

// blinks momentarily when data is received from the host
//    (comment out to disable)
//#define CMD_LED (13)

// goes HIGH whenever received pulse data is encountered
#define CW_DET_PIN (0)

// input pin for detected RX tone data
#define CW_RX_PIN (A0)

// the PTT output pin
#define CW_PTT_PIN (7) // Stepper enable
#define CW_STEPPER_STEP_PIN (9)
#define CW_STEPPER_DIR_PIN (8)

// the KEY output pin
#define CW_KEY_PIN (6)
#define CW_KEY_PIN2 3

//
//  Serial Port - assign a specific hardware port to be the
//                interface to the host; by default, the USB
//                connection on most boards is 'Serial', which
//                is also the hardware TTL serial port on the
//                UNO board.  The hardware serial device on
//                the Leonardo is Serial1.
//
#define HostSerial Serial

// the 'baud rate' of the serial port
#define SerialRate (115200)

//#define TIMING_DEBUG

// ================== END OF CONFIGURATION SETTINGS ==================

// local headers
#include "CircularBuffer.h"
#include "CwTimingLogic.h"
#include "CwDecoderLogic.h"
#include "Elapsed.h"
#include "Bounce2.h"

// the version string
#define VERSION_STRING ("CwModem 1.0 (beta-20140413a)")

// CR+LF
#define LOCAL_CRLF ("\r\n")

// import some namespaces
using namespace KK5JY::Collections;
using namespace KK5JY::CW;

// buffer for pulse timing data
CircularBuffer<CwElement> CwBuffer(32);

// buffer for decoded elements
CircularBuffer<MorseElements> ElementBuffer(32);

// a TX buffer
CircularBuffer<char> TxBuffer(512);

// the clock restoration logic
CwTimingLogic Timing;

// the decoder
CwDecoderLogic Decoder;

// debounce for the key input
Bounce KeyInput;

// a string to hold incoming data
String inputString; // start with Version, WPM queries

// and another for command data
String cmdText = "";

// command parsing
String command = "";
String argument = "";

// whether the string is complete
boolean inputReady = false;

// I/O buffer
char ioBuffer[32];

// the last RX data from the host
unsigned long lastRxData = 0;
uint8_t lastCmdLED = LOW;

// last CW transition
unsigned long lastTime = 0;

// RX CW state - start with 'true' (unkeyed or no-tone)
bool lastState = true;

// used to detect end of transmission
bool hanging = false;

// echo USB RX data back to host
bool echo = false;

// currently parsing a command
bool cmd = false;

// TX mode (otherwise RX)
bool tx = false;

// tracks PTT state without reading PTT pin
bool pttState = false;

// data LED hang-time (makes it more obvious to the user that data was rec'd)
const int DataLedDelay = 250;

volatile bool tx_tone = false;

// TX element timing
unsigned long curTxStart = 0;
unsigned long lastKeyDown = 0;
unsigned curTxLength = 0;
CwElement cwOut;

// PTT timing
unsigned pttLeadIn = 100;			// msec
unsigned pttLeadOut = 2500;			// msec

// Limits
const int MaxPttLeadIn = 500;		// msec
const int MaxPttLeadOut = 1000;		// msec
const int MaxWPM = 60;				// WPM
const int MinWPM = 5;				// WPM
const unsigned long
	MaxBounceInterval = 1200 / MaxWPM;	// msec
const unsigned MinBoxCarLength = 8;		// elements
const unsigned MaxBoxCarLength = 65535;	// elements

const int KeyDebounceInterval = 3;	// msec

const int InputStringLength = 16;
const int CommandStringLength = 16;

// control keys
const char ESC = 0x1B;
const char CtlC = 0x03;
const char CtlU = 0x15;		// up
const char CtlD = 0x04;		// down


//
//  Pulse(...) - run the decoder on a state change
//
static void Pulse(unsigned long now, unsigned pulseWidth, bool state) {
	CwElement cw;
	cw.Mark = (bool)(!state); // the keyer pulls the pin LOW on contact
	cw.Length = (unsigned)pulseWidth;
	CwBuffer.Add(cw);		// TODO: check this for failure, to detect overrun
#ifdef TIMING_DEBUG
	if (state)
		Serial.print("(");
	Serial.print(pulseWidth);
	if (state)
		Serial.print(")");

	Serial.print(" ");
#endif

	if (Timing.Decode(CwBuffer, ElementBuffer)) {
		byte ct = Decoder.Decode(ElementBuffer, ioBuffer, 8);
		if (ct > 0) {
			ioBuffer[ct] = 0;
			HostSerial.print(ioBuffer);
#ifdef TIMING_DEBUG
			Serial.print(" --> ");
			Serial.println(Timing.DotLength());
#endif
		}
	}
}


//
//  DumpElementBuffer()
//
void DumpElementBuffer() {
	HostSerial.println(strcpy_P(ioBuffer, PSTR("Decoder.Encode(): OK")));
	for (int k = 0; k != ElementBuffer.Count(); ++k) {
		switch(ElementBuffer.ItemAt(k)) {
			case Dot: HostSerial.print('.'); break;
			case Dash: HostSerial.print('-'); break;
			case WordSpace:
			case DashSpace: HostSerial.print(' '); break;
		}
	}
}


//
//  SplitCommand(...) - split a command from its argument
//
static void SplitCommand(const String &input, String &cmd, String &arg) {
	bool isCmd = true;
	cmd = "";
	arg = "";
	for (int i = 0; i != input.length(); ++i) {
		char ch = input.charAt(i);
		if (ch == '=') {
			isCmd = false;
			continue;
		}
		if (isCmd) {
			cmd += ch;
		} else {
			arg += ch;
		}
	}
}


//
//  setup()
//
void setup() {
	// start the serial port
	HostSerial.begin(SerialRate);
	
	// allocate strings
	cmdText.reserve(CommandStringLength);
	//inputString.reserve(InputStringLength);
	//inputString = strcpy_P(ioBuffer, PSTR("#VERSION;"));
	//inputReady = true;

	// the 'command' LED
	#ifdef CMD_LED
	pinMode(CMD_LED, OUTPUT);
	digitalWrite(CMD_LED, LOW);
	lastCmdLED = LOW;

	// send 'K' to the command LED (just a toy)
	digitalWrite(CMD_LED, HIGH);
	delay(150);
	digitalWrite(CMD_LED, LOW);
	delay(50);
	digitalWrite(CMD_LED, HIGH);
	delay(50);
	digitalWrite(CMD_LED, LOW);
	delay(50);
	digitalWrite(CMD_LED, HIGH);
	delay(150);
	digitalWrite(CMD_LED, LOW);
	delay(250);
	#endif
	
	// CW RX
	pinMode(CW_RX_PIN, INPUT_PULLUP);
	KeyInput.attach(CW_RX_PIN);
	KeyInput.interval(KeyDebounceInterval);
#if CW_DET_PIN != 0
	pinMode(CW_DET_PIN, OUTPUT);
	digitalWrite(CW_DET_PIN, LOW);
#endif
	
	// CW TX
	pinMode(CW_KEY_PIN, INPUT);
	tx_tone = false;
	//digitalWrite(CW_KEY_PIN, LOW);
	// Set up 128us period PWM on pin PD3 / D3 to limit current
	// through telegraph coils. Higer frequency reduces coil noise.
	// This timer is also used to generate the tone output.
	pinMode(CW_KEY_PIN2, INPUT);
	TCCR2A = _BV(COM2B1) | _BV(WGM20) | _BV(WGM21);
	TCCR2B = _BV(CS21);
	// PWM duty cycle (higher is more current / force)
	OCR2B = 80;

	TIMSK2 = _BV(TOIE2);

	// CW PTT
#if CW_PTT_PIN != 0
	// PTT pin controls stepper enable pin
	pinMode(CW_PTT_PIN, OUTPUT);
	digitalWrite(CW_PTT_PIN, HIGH);
	pttState = false;

#endif

	// Set up variable frequency 50% PWM for stepper driver "step"
	// pin (pin PB1 / D9). A single timer step is 16us.
	pinMode(CW_STEPPER_STEP_PIN, OUTPUT);
	TCCR1A = _BV(COM1A0) |  _BV(WGM11) | _BV(WGM10);
	TCCR1B = _BV(WGM13) | _BV(WGM12) |_BV(CS22);
	OCR1A = 6; // Step time (lower is faster)

	// Stepper direction is fixed
	pinMode(CW_STEPPER_DIR_PIN, OUTPUT);
	digitalWrite(CW_STEPPER_DIR_PIN, HIGH);

	// save initial timestamps
	lastTime = millis();
	lastRxData = lastTime;
	
	// initialize WPM
	Timing.RxWPM(10);
	Timing.TxWPM(12);
	Timing.RxMode(SpeedAuto);
	Timing.TxMode(SpeedManual);
	// Be a bit more lenient about the length of spaces, to
	// facilitate inexperienced operators
	Timing.MaximumDotSpaceLength = 4;
	Timing.MinimumWordSpace = 15;
}

uint8_t overflow_count = 0;
const uint8_t TONE_PERIOD = 6; // 11 periods of 128us gives 710Hz

static_assert(CW_KEY_PIN == 6, "CW_KEY_PIN assumed to be D6 / PD6");

ISR(TIMER2_OVF_vect) {
	if ((DDRD & (1 << PD6)) && overflow_count == 0) {
		overflow_count = TONE_PERIOD;
		PIND = (1 << PD6);
	}
	overflow_count--;
}

//
//  loop()
//
void loop() {
	unsigned long now = millis();

	// extinguish the data LED
	#ifdef CMD_LED
	if ((lastCmdLED == HIGH) && (Elapsed(now, lastRxData) > DataLedDelay)) {
		lastCmdLED = LOW;
		digitalWrite(CMD_LED, LOW);
	}
	#endif
	
	// if we are in transmit mode, service the TX queue
	if (tx) {
		// if the current element has been sent...
		if (Elapsed(now, curTxStart) >= curTxLength) {
			// if there are no more elements to send, generate some more
			if (CwBuffer.Count() == 0 && TxBuffer.Count()) {
				char next;
				if (TxBuffer.Remove(next) && Decoder.Encode(ElementBuffer, &next, 1)) {
					#ifdef VERBOSE_DEBUG
					DumpElementBuffer();
					#endif

					// encode the char
					Timing.Encode(ElementBuffer, CwBuffer);
				}
			}
			
			// if there are elements to send, start sending the first one
			if (CwBuffer.Remove(cwOut)) {
				curTxStart = now;
				curTxLength = cwOut.Length;
				tx_tone = cwOut.Mark;
				pinMode(CW_KEY_PIN, cwOut.Mark);
				pinMode(CW_KEY_PIN2, cwOut.Mark);
				if (!cwOut.Mark) {
					lastKeyDown = now;
				}
							
				#ifdef VERBOSE_DEBUG
				HostSerial.print(strcpy_P(ioBuffer, PSTR("New Element: M=")));
				HostSerial.print(cwOut.Mark);
				HostSerial.print(strcpy_P(ioBuffer, PSTR("; L=")));
				HostSerial.println(cwOut.Length);
				#endif
			} else {
				tx = false;
			}
		}
	}

	// key-up
	now = millis();
#if CW_PTT_PIN != 0
	if (!tx && pttState && Elapsed(now, lastKeyDown) > pttLeadOut) {
		digitalWrite(CW_PTT_PIN, HIGH);
		pttState = false;
	}
#endif
	
	// if user input ready
	if (inputReady) {
		// light the CMD LED
		#ifdef CMD_LED
		lastRxData = millis();
		if (lastCmdLED == LOW) {
			lastCmdLED = HIGH;
			digitalWrite(CMD_LED, HIGH);
		}
		#endif
		
		// for each char received from the USB...
		for (int i = 0; i != inputString.length(); ++i) {
			char ch = inputString.charAt(i);
			
		/*	
			// if we are in command mode...
			if (cmd) {
				if (ch == '#') {
					// if a '#' sent mid-command, start over
					cmdText = "";
				} else if (ch == ';') {
					// terminate and process the command string
					cmd = false;
					bool ok = false;
					if (echo) HostSerial.print(';');

					// parse the command and argument
					SplitCommand(cmdText, command, argument);
					if (command.equalsIgnoreCase(strcpy_P(ioBuffer, PSTR("RXWPM")))) {
						//
						//  QUERY/SET WPM (RX)
						//
						if (argument.length() == 0) {
							ok = true;
							cmdText += '=';
							cmdText += (int)(Timing.RxWPM());
						} else {
							int newWPM = argument.toInt();
							if (newWPM >= MinWPM && newWPM <= MaxWPM) {
								Timing.RxWPM(newWPM);
								ok = true;
							}
						}
					} else if (command.equalsIgnoreCase(strcpy_P(ioBuffer, PSTR("TXWPM")))) {
						//
						//  QUERY/SET WPM (TX)
						//
						if (argument.length() == 0) {
							ok = true;
							cmdText += '=';
							cmdText += (int)(Timing.TxWPM());
						} else {
							int newWPM = argument.toInt();
							if (newWPM >= MinWPM && newWPM <= MaxWPM) {
								Timing.TxWPM(newWPM);
								ok = true;
							}
						}
					} else if (command.equalsIgnoreCase(strcpy_P(ioBuffer, PSTR("ECHO")))) {
						//
						//  QUERY/SET ECHO
						//
						if (argument.length() == 0) {
							ok = true;
							cmdText += '=';
							cmdText += echo ? "1" : "0";
						} else {
							int newEcho = argument.toInt();
							echo = newEcho ? true : false;
							ok = true;
						}
					} else if (command.equalsIgnoreCase(strcpy_P(ioBuffer, PSTR("PTTIN")))) {
						//
						//  QUERY/SET PTT LEAD-IN
						//
						if (argument.length() == 0) {
							ok = true;
							cmdText += '=';
							cmdText += (int)(pttLeadIn);
						} else {
							int newPTT = argument.toInt();
							if (newPTT >= 0 && newPTT <= MaxPttLeadIn) {
								pttLeadIn = newPTT;
								ok = true;
							}
						}
					} else if (command.equalsIgnoreCase(strcpy_P(ioBuffer, PSTR("PTTOUT")))) {
						//
						//  QUERY/SET PTT LEAD-OUT
						//
						if (argument.length() == 0) {
							ok = true;
							cmdText += '=';
							cmdText += (int)(pttLeadOut);
						} else {
							int newPTT = argument.toInt();
							if (newPTT >= 0 && newPTT <= MaxPttLeadOut) {
								pttLeadOut = newPTT;
								ok = true;
							}
						}
					} else if (command.equalsIgnoreCase(strcpy_P(ioBuffer, PSTR("DOT")))) {
						//
						//  QUERY DOT LENGTH
						//
						ok = true;
						cmdText += '=';
						cmdText += (int)(Timing.DotLength());
					} else if (command.equalsIgnoreCase(strcpy_P(ioBuffer, PSTR("RXMODE")))) {
						//
						//  ENABLE/DISABLE RX WPM AUTO-TRACK
						//
						if (argument.length() == 0) {
							ok = true;
							cmdText += '=';
							cmdText += Timing.RxMode() == SpeedAuto ? "AUTO" : "MANUAL";
						} else {
							if (argument.equalsIgnoreCase("AUTO")) {
								ok = true;
								Timing.RxMode(SpeedAuto);
							} else if (argument.equalsIgnoreCase("MANUAL")) {
								ok = true;
								Timing.RxMode(SpeedManual);
							}
						}
					} else if (command.equalsIgnoreCase(strcpy_P(ioBuffer, PSTR("TXMODE")))) {
						//
						//  ENABLE/DISABLE TX WPM AUTO-TRACK
						//
						ok = true;
						if (argument.length() == 0) {
							ok = true;
							cmdText += '=';
							cmdText += Timing.TxMode() == SpeedAuto ? "AUTO" : "MANUAL";
						} else {
							if (argument.equalsIgnoreCase("AUTO")) {
								ok = true;
								Timing.TxMode(SpeedAuto);
							} else if (argument.equalsIgnoreCase("MANUAL")) {
								ok = true;
								Timing.TxMode(SpeedManual);
							}
						}
					} else if (command.equalsIgnoreCase(strcpy_P(ioBuffer, PSTR("BOUNCE")))) {
						//
						//  GET/SET CW INPUT DEBOUNCE INTERVAL
						//
						if (argument.length() == 0) {
							ok = true;
							cmdText += '=';
							cmdText += (int)(KeyInput.interval());
						} else {
							int newBounce = argument.toInt();
							if (newBounce >= 0 && newBounce <= MaxBounceInterval) {
								KeyInput.interval(newBounce);
								ok = true;
							}
						}
					} else if (command.equalsIgnoreCase(strcpy_P(ioBuffer, PSTR("BCL")))) {
						//
						//  GET/SET CW SPEED TRACKING BOXCAR SIZE
						//
						if (argument.length() == 0) {
							ok = true;
							cmdText += '=';
							cmdText += (int)(Timing.BoxCarLength());
						} else {
							unsigned newBCL = argument.toInt();
							if (newBCL >= MinBoxCarLength && newBCL <= MaxBoxCarLength) {
								Timing.BoxCarLength(newBCL);
								ok = true;
							}
						}
					} else if (command.equalsIgnoreCase(strcpy_P(ioBuffer, PSTR("DZ")))) {
						//
						//  GET/SET CW SPEED TRACKING DEAD ZONE (percent)
						//
						if (argument.length() == 0) {
							ok = true;
							cmdText += '=';
							cmdText += (int)(Timing.MinimumAverageDistance() * 100);
						} else {
							unsigned newDZ = argument.toInt();
							if (newDZ >= 0 && newDZ <= 100) {
								Timing.MinimumAverageDistance((float)newDZ / 100.0);
								ok = true;
							}
						}
					} else if (command.equalsIgnoreCase(strcpy_P(ioBuffer, PSTR("VERSION")))) {
						//
						//  QUERY VERSION NUMBER
						//
						ok = true;
						cmdText += '=';
						cmdText += strcpy_P(ioBuffer, PSTR(VERSION_STRING));
					}
					
					//
					//  TODO: process other command contents
					//

					// respond with #OK or #ERR
					if (echo)
						HostSerial.print(strcpy_P(ioBuffer, PSTR(LOCAL_CRLF)));
					HostSerial.print(ok ? strcpy_P(ioBuffer, PSTR("#OK:")) : strcpy_P(ioBuffer, PSTR("#ERR:")));
					HostSerial.print(cmdText);
					HostSerial.print(';');
					if (echo)
						HostSerial.print(strcpy_P(ioBuffer, PSTR(LOCAL_CRLF)));
					cmdText = "";
				} else {
					if (isAlpha(ch) || isDigit(ch) || isPunct(ch) || (ch == ' ')) {
						// add the char to the command string
						if (cmdText.length() < CommandStringLength)
							cmdText += ch;
						
						// echo the char, if enabled
						if (echo) {
							HostSerial.print(ch);
						}
					}
				}
				continue;
			}
			// start command mode??
			if ((!cmd) && (ch == '#')) {
				cmd = true;
				if (echo) HostSerial.print('#');
				continue;
			}

			// hard-stop?
			if (ch == ESC || ch == CtlC) {
				tx_tone = false;
				pinMode(CW_KEY_PIN, INPUT);
				pinMode(CW_KEY_PIN2, INPUT);
#if CW_PTT_PIN != 0
				digitalWrite(CW_PTT_PIN, HIGH);
#endif
				TxBuffer.Clear();
				CwBuffer.Clear();
				ElementBuffer.Clear();
				pttState = false;
				tx = false;
				continue;
			}
*/
			// echo the char, if enabled
			if (echo) {
				HostSerial.print(ch);
				if (ch == '\r') {
					HostSerial.print('\n');
				}
			}

			// upper-case the char to search for a TX pattern
			ch = toupper(ch);

			// switch to TX mode, if needed
			if (!tx) {
				CwBuffer.Clear();
				ElementBuffer.Clear();
			}
			
			#ifdef VERBOSE_DEBUG
			HostSerial.print(strcpy_P(ioBuffer, PSTR("Send: ")));
			HostSerial.println(ch);
			#endif

			// if we are already in TX mode, just queue up the character
			if (tx) {
				TxBuffer.Add(ch);
			} else if (Decoder.Encode(ElementBuffer, &ch, 1)) {
				#ifdef VERBOSE_DEBUG
				DumpElementBuffer();
				#endif
				
				// otherwise, generate characters, and go into TX mode
				if (Timing.Encode(ElementBuffer, CwBuffer)) {
					if (CwBuffer.Remove(cwOut)) {
#if CW_PTT_PIN != 0
						// PTT lead-in
						pttState = true;
						digitalWrite(CW_PTT_PIN, LOW);
						delay(pttLeadIn);
#endif

						// start sending
						tx = true;
						curTxStart = millis();
						curTxLength = cwOut.Length;
						tx_tone = cwOut.Mark;
						pinMode(CW_KEY_PIN, cwOut.Mark);
						pinMode(CW_KEY_PIN2, cwOut.Mark);
						if (!cwOut.Mark) {
							lastKeyDown = curTxStart;
						}
						
						#ifdef VERBOSE_DEBUG
						HostSerial.print(strcpy_P(ioBuffer, PSTR("New Element: M=")));
						HostSerial.print(cwOut.Mark);
						HostSerial.print(strcpy_P(ioBuffer, PSTR("; L=")));
						HostSerial.println(cwOut.Length);
						#endif
					}
				}
			}
		}

done:
		// clear the string:
		inputString = "";
		inputReady = false;
	}

	//
	//   RX: read data from input pin
	//
	if (!tx) {
		// if the state has changed
		if (KeyInput.update()) {
			bool newState = KeyInput.read();
			unsigned long now = millis();
			unsigned long pulseWidth = 0;

			pinMode(CW_KEY_PIN, newState ? INPUT : OUTPUT);
			tx_tone = !newState;
			
			// calculate the pulse width
			pulseWidth = Elapsed(now, lastTime);
			if (pulseWidth > 65535) {
				pulseWidth = 65535;
			}

#if CW_DET_PIN != 0
			digitalWrite(CW_DET_PIN, !newState);
#endif
			// update the decoder
			Pulse(now, pulseWidth, lastState);
			
			// if the new state is key-up, look for trailing space
			if (newState) {
				hanging = true;
			}

			lastTime = now;		
			lastState = newState;
		} else {
			// handling the trailing edge of the last key-down element
			if (lastState && hanging) {
				unsigned long now = millis();
				unsigned ws = Timing.DotLength() * Timing.MinimumWordSpace;
				if (Elapsed(now, lastTime) > ws) {
					Pulse(now, ws, true);
					hanging = false;
				}
			}
		}
	}

	// service the serial port
	while (HostSerial.available() && (inputString.length() < InputStringLength)) {
		// get the new byte:
		char inChar = (char)HostSerial.read();
		// add it to the inputString:
		inputString += inChar;
Serial.write(inChar);
		inputReady = true;
	}
}

// EOF
