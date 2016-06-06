#include <avr/io.h>
#include <avr/pgmspace.h>
#include <util/delay.h>

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

// Custom library files
#include "lib/iopins.h"
#include "lib/usart.h"
#include "lib/nsdelay.h"

// --- Pin assignments  ---

// RGB data
#define WS_PIN 7

// Sonars
#define TRIG1_PIN 3
#define ECHO1_PIN 2

#define TRIG2_PIN 9
#define ECHO2_PIN 8

#define TRIG3_PIN 11
#define ECHO3_PIN 10


/** Number of LEDs in your strip */
#define LED_COUNT 30


/** averaging buffer length (number of samples) */
#define MBUF_LEN 16

/** Phase of the measurement (state-machine state) */
typedef enum {
	MEAS_WAIT_1,
	MEAS_WAIT_0,
	MEAS_DONE
} MeasPhase;

/** Averaging buffer instance */
typedef struct {
	float data[MBUF_LEN];
} MBuf;

static MBuf mb_offs1;
static MBuf mb_offs2;
static MBuf mb_offs3;

/** RGB color structure */
typedef struct __attribute__((packed)) {
	uint8_t r;
	uint8_t g;
	uint8_t b;
} RGB;

/** LED strip colors */
static RGB history[LED_COUNT];

/** Add a value to the averaging buffer. Returns currrent mean value */
static float mbuf_add(MBuf *buf, float value)
{
	float aggr = value;
	for (int i = MBUF_LEN - 1; i > 0; i--) {
		float m = buf->data[i - 1];
		aggr += m;
		buf->data[i] = m;
	}

	buf->data[0] = value;

	return aggr / (float)MBUF_LEN;
}


/** Wait long enough for the colors to show */
static inline  __attribute__((always_inline))
void ws_show(void)
{
	_delay_us(10);
}

/** Send one byte to the RGB strip */
static inline  __attribute__((always_inline))
void ws_send_byte(uint8_t bb)
{
	// If your LEDs don't work right, you may need to adjust the number of NOPs here
	// It's a good idea to debug this with oscilloscope or a logic analyzer

	for (int8_t i = 8; i > 0; i--) {
		pin_up(WS_PIN);
		if (bb & 0x80) {
			__asm__ volatile("nop");
			__asm__ volatile("nop");
			__asm__ volatile("nop");
			__asm__ volatile("nop");
			__asm__ volatile("nop");
			pin_down(WS_PIN);
			__asm__ volatile("nop");
		} else {
			__asm__ volatile("nop");
			pin_down(WS_PIN);
			__asm__ volatile("nop");
			__asm__ volatile("nop");
			__asm__ volatile("nop");
			__asm__ volatile("nop");
			__asm__ volatile("nop");
		}

		bb = (uint8_t)(bb << 1);
	}
}

/** Send a RGB color to the strip */
static void ws_send_rgb(uint8_t r, uint8_t g, uint8_t b)
{
	ws_send_byte(g);
	ws_send_byte(r);
	ws_send_byte(b);
}

/** Init hardware resources */
static void hw_init(void)
{
	usart_init(BAUD_115200);

	as_output(WS_PIN);

	as_input_pu(ECHO1_PIN);
	as_input_pu(ECHO2_PIN);
	as_input_pu(ECHO3_PIN);

	as_output(TRIG1_PIN);
	as_output(TRIG2_PIN);
	as_output(TRIG3_PIN);

	as_output(13);
}

/**
 * Measure one ultrasonic sensor distance
 *
 * We could run all 3 at once, but then the sound waves tend to reflect
 * into different receives and you get false readings.
 */
static uint8_t meas(MBuf *mbuf, uint8_t trig_pin, uint8_t echo_pin)
{
	_delay_ms(6);
	pin_up_n(trig_pin);
	_delay_ms(1);
	pin_down_n(trig_pin);

	MeasPhase meas_phase = MEAS_WAIT_1;

	uint32_t echo = 0;

	TCNT1 = 0;
	TCCR1B = (0b010 << CS10);

	while (true) {
		if (meas_phase == MEAS_WAIT_1) {
			if (pin_is_high_n(echo_pin)) {
				echo = TCNT1;
				meas_phase = MEAS_WAIT_0;
			}
		} else if (meas_phase == MEAS_WAIT_0) {
			if (pin_is_low_n(echo_pin)) {
				echo = TCNT1 - echo;
				break;
			}
		}

		// timeout
		if (TCNT1 >= 15000) {
			echo = 15000;
			break;
		}
	}

	TCCR1B = 0; // stop

	// Pulse measured with 0.5us accuracy
	// To convert to mm -> multiply by 0.8

	// --- convert to R/G/B value 0-255 ---

	// The number '25.0f' here determines the sensitivity
	float offset = 255 - echo / (1.25f * 25.0f);

	if (offset > 255) {
		offset = 255;
	} else if (offset < 0) {
		offset = 0;
	}

	// averaging
	offset = mbuf_add(mbuf, offset);

	// to int
	return (uint8_t) roundf(offset);
}

/** Measure all 3 sensors and update the colors */
static void sonar_measure(void)
{
	uint8_t c1 = meas(&mb_offs1, TRIG1_PIN, ECHO1_PIN);
	uint8_t c2 = meas(&mb_offs2, TRIG2_PIN, ECHO2_PIN);
	uint8_t c3 = meas(&mb_offs3, TRIG3_PIN, ECHO3_PIN);

	for (int i = LED_COUNT - 1; i > 0; i--) {
		history[i].r = history[i - 1].r;
		history[i].g = history[i - 1].g;
		history[i].b = history[i - 1].b;
	}

	history[0].r = c1;
	history[0].g = c2;
	history[0].b = c3;

	for (int i = 0; i < LED_COUNT; i++) {
		ws_send_rgb(history[i].r, history[i].g, history[i].b);
	}

	ws_show();
}


int main(void)
{
	hw_init();

	usart_puts_P(PSTR("===========================\r\n"));
	usart_puts_P(PSTR("\r\n"));
	usart_puts_P(PSTR("SPATIAL RGB - SONAR DEMO\r\n"));
	usart_puts_P(PSTR("FEE CTU Prague, K338\r\n"));
	usart_puts_P(PSTR("\r\n"));
	usart_puts_P(PSTR("(c) Ondrej Hruska 2016\r\n"));
	usart_puts_P(PSTR("\r\n"));
	usart_puts_P(PSTR("===========================\r\n"));

	int cnt = 0;

	while (1) {
		// This takes something close to 50 ms, varies with measured distances.
		sonar_measure(); 

		// Notice how the indicator blinking changes speed with distances
		// You might want to do some adjustments here if you want 100% constant animation speed.

		if (++cnt == 20) {
			cnt = 0;
			pin_toggle(13); // blink the indicator to show that we're OK
		}
	}
}
