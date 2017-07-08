/*
 *    A telegraph-to-redis gateway, based on CwMode.ino.
 *
 *    License: GNU General Public License Version 3.0.
 *    
 *    Copyright (C) 2014 by Matthew K. Roberts, KK5JY. All rights reserved.
 *    Copyright (C) 2017 by Matthijs Kooijman <matthijs@stdin.nl>
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

#include <stdint.h>
#include <unistd.h>
#include <stdio.h>
#include <termios.h>
#include <chrono>
#include <thread>
using namespace std::chrono_literals;

#include <pigpiod_if2.h>
#include <hiredis/hiredis.h>
#include <hiredis/async.h>
#include <hiredis/adapters/libev.h>

//#define VERBOSE_DEBUG
//#define TIMING_DEBUG

#include "CircularBuffer.h"
#include "CwTimingLogic.h"
#include "CwDecoderLogic.h"

// import some namespaces
using namespace KK5JY::Collections;
using namespace KK5JY::CW;

const uint32_t HW_PWM_MAX_DUTYCYCLE = 1E6;
const uint32_t DMA_PWM_MAX_DUTYCYCLE = 256;

const uint8_t STEPPER_DIR_PIN = 6;
const uint8_t STEPPER_ENABLE_PIN = 26;
const uint8_t STEPPER_STEP_PIN = 13; // PWM
const uint32_t STEPPER_FREQ = 7000;
const auto STEPPER_LEAD_IN = 100ms;
const auto STEPPER_LEAD_OUT = 3500ms;

const uint8_t COIL_PIN = 16;
// Only specific frequencies are available for DMA-driven PWM
const uint32_t COIL_FREQ = 8000;
const uint8_t COIL_DUTYCYCLE = 0.3 * DMA_PWM_MAX_DUTYCYCLE;

const uint8_t SPEAKER_PIN = 18; // PWM
const uint32_t TONE_FREQ = 700;

const uint8_t KEY_PIN = 17;

const char *PUBLISH_TOPIC = "toSL";
const char *SUBSCRIBE_TOPIC = "toPlayers";

int pigpiod = -1;

// the clock restoration logic
CwTimingLogic Timing;

// the decoder
CwDecoderLogic Decoder;

void tone_on() {
	hardware_PWM(pigpiod, SPEAKER_PIN, TONE_FREQ, HW_PWM_MAX_DUTYCYCLE / 2);
}

void tone_off() {
	set_mode(pigpiod, SPEAKER_PIN, PI_OUTPUT);
	gpio_write(pigpiod, SPEAKER_PIN, 0);
}

void coil_on() {
	set_PWM_dutycycle(pigpiod, COIL_PIN, COIL_DUTYCYCLE);
}

void coil_off() {
	set_mode(pigpiod, COIL_PIN, PI_OUTPUT);
	gpio_write(pigpiod, COIL_PIN, 0);
}

void stepper_on() {
	gpio_write(pigpiod, STEPPER_ENABLE_PIN, 0);
}

void stepper_off() {
	gpio_write(pigpiod, STEPPER_ENABLE_PIN, 1);
}

// TODO: Cleanup on error/signal using atexit & signal handlers?


using time_point = std::chrono::steady_clock::time_point;

void process_tx_char(char ch, time_point tx_start) {
	time_point tx_next = tx_start;

	ch = toupper(ch);
	std::queue<MorseElements> elems;
	Decoder.Encode(ch, elems);

	while (!elems.empty()) {
		MorseElements elem = elems.front();
		elems.pop();

		CwElement cwe = Timing.Encode(elem);
		std::this_thread::sleep_until(tx_next);

		if (cwe.Mark) {
			tone_on();
			coil_on();
		}

		tx_next += std::chrono::milliseconds(cwe.Length);
		std::this_thread::sleep_until(tx_next);

		tone_off();
		coil_off();
	}
}

void process_tx_message(const char *msg) {
	printf("Sending message: %s\n", msg);

	stepper_on();
	time_point tx_start = std::chrono::steady_clock::now() + STEPPER_LEAD_IN;
	tx_start = std::chrono::steady_clock::now();
	while (*msg) {
		process_tx_char(*msg, tx_start);
		tx_start = std::chrono::steady_clock::now();
		msg++;
	}
	std::this_thread::sleep_for(STEPPER_LEAD_OUT);
	stepper_off();
}

#if 0
void setup_stdin() {
	struct termios ctrl;
	tcgetattr(STDIN_FILENO, &ctrl);
	ctrl.c_lflag &= ~ICANON; // turning off canonical mode makes input unbuffered
	tcsetattr(STDIN_FILENO, TCSANOW, &ctrl);
}

void process_tx() {
	setup_stdin();
	while (true) {
		char c;

		int len = read(STDIN_FILENO, &c, 1);
		if (len == 0)
			break;
		if (len < 1) {
			perror("Failed to read stdin");
			break;
		}

		process_tx_char(c);
	}
}
#endif

void process_rx_msg(const char*msg) {
	static redisContext *publishContext = NULL;
	if (!publishContext)
		publishContext = redisConnect("127.0.0.1", 6379);
	redisCommand(publishContext, "PUBLISH %s %s", PUBLISH_TOPIC, msg);
}

// buffer for pulse timing data
CircularBuffer<CwElement> CwBuffer(32);

// buffer for decoded elements
CircularBuffer<MorseElements> ElementBuffer(32);
//
//  Pulse(...) - run the decoder on a state change
//
static void Pulse(unsigned pulseWidth, bool state) {
	CwElement cw;
	cw.Mark = state; // the keyer pulls LOW, so state becomes true *after* a mark
	cw.Length = (unsigned)pulseWidth;
	CwBuffer.Add(cw);
#ifdef TIMING_DEBUG
	if (state)
		printf("(%u) ", pulseWidth);
	else
		printf("%u ", pulseWidth);
#endif

	if (Timing.Decode(CwBuffer, ElementBuffer)) {
		// I/O buffer
		char ioBuffer[32];

		uint8_t ct = Decoder.Decode(ElementBuffer, ioBuffer, 8);
		if (ct > 0) {
			ioBuffer[ct] = 0;
			printf("%s", ioBuffer);
			process_rx_msg(ioBuffer);
#ifdef TIMING_DEBUG
			printf(" --> %f\n", Timing.DotLength());
#endif
		}
	}
}


// Callback, called when the key pin changes, or a timeout occurs
void process_rx_edge(int pi, unsigned user_gpio, unsigned level, uint32_t tick) {
	static uint32_t prev_edge = 0;
	static bool active = false;

	uint32_t duration = tick - prev_edge;
	prev_edge = tick;

	// Debounce
	// TODO: Improve?
	if (duration < 5000)
		return;

	// Eat up the first edge after some time of inactivity, and set a
	// watchdog to detect inactivity after the GPIO stops changing.
	if (!active) {
		set_watchdog(pi, user_gpio, Timing.MinimumWordSpace * Timing.DotLength());
		active = true;
		return;
	}

	if (level == PI_TIMEOUT) {
		// Watchdog timeout, some time passed without events. Disable
		// the watchdog and generate a trailing space pulse.
		set_watchdog(pi, user_gpio, 0);
		active = false;
		Pulse(duration / 1000, false);
		return;
	}
	
	Pulse(duration / 1000, level == PI_HIGH);
}

	/*
		MorseElements elem;
		{
			std::lock_guard<std::mutex> lock (tx_queue_mutex);
			while (tx_queue.empty())
				tx_queue_ready.wait();
			elem = tx_queue.front();
			tx_queue.pop();
		}
		*/

void process_redis_tx() {
	redisContext *subscribeContext = redisConnect("127.0.0.1", 6379);
	redisReply *reply = (redisReply*)redisCommand(subscribeContext,"SUBSCRIBE toPlayers");
	freeReplyObject(reply);
	while(redisGetReply(subscribeContext,(void**)&reply) == REDIS_OK) {
		if (reply->type == REDIS_REPLY_ARRAY && reply->elements == 3 &&
		    reply->element[0]->type == REDIS_REPLY_STRING &&
		    reply->element[2]->type == REDIS_REPLY_STRING) {
			const char *kind = reply->element[0]->str;
			if (strcmp(kind, "message") == 0) {
				process_tx_message(reply->element[2]->str);
				continue;
			} else if (strcmp(kind, "subscribe")) {
				// Ignore subscribe confirmation
				continue;
			} else {
				fprintf(stderr, "Unexpected redis reply: %s\n", kind);
			}
		} else {
			fprintf(stderr, "Unexpected redis reply");
		}

    		// consume message
		freeReplyObject(reply);
	}
	redisFree(subscribeContext);
}

int main(int argc, char **argv) {
	// Connect to localhost
	pigpiod = pigpio_start(NULL, NULL);

	// Enable is active-low, so disable by writing 1
	set_mode(pigpiod, STEPPER_ENABLE_PIN, PI_OUTPUT);
	stepper_off();

	// Direction 1 is forward
	set_mode(pigpiod, STEPPER_DIR_PIN, PI_OUTPUT);
	gpio_write(pigpiod, STEPPER_DIR_PIN, 1);

	// Set up the step pin to continuously generate step pulses, the
	// stepper is controlled using the enable pin.
	hardware_PWM(pigpiod, STEPPER_STEP_PIN, STEPPER_FREQ, HW_PWM_MAX_DUTYCYCLE / 2);

	set_mode(pigpiod, KEY_PIN, PI_INPUT);
	set_pull_up_down(pigpiod, KEY_PIN, PI_PUD_UP);

	tone_off();
	coil_off();

	// initialize WPM
	Timing.RxWPM(10);
	Timing.TxWPM(10);
	Timing.RxMode(SpeedAuto);
	Timing.TxMode(SpeedManual);

	// Be a bit more lenient about the length of spaces, to
	// facilitate inexperienced operators
	Timing.MaximumDotSpaceLength = 4;
	Timing.MinimumWordSpace = 15;

	// Setup callback to run on RX changes. This uses a background thread.
	callback(pigpiod, KEY_PIN, EITHER_EDGE, process_rx_edge);

	printf("Started\n");

	// Does not normally return
	process_redis_tx();

	pigpio_stop(pigpiod);
	return 0;
}
