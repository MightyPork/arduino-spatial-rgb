#include <avr/io.h>          // register definitions
#include <avr/pgmspace.h>    // storing data in program memory
#include <avr/interrupt.h>   // interrupt vectors
#include <util/delay.h>      // delay functions

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

// Include stuff from the library
#include "lib/iopins.h"
#include "lib/usart.h"
#include "lib/nsdelay.h"

#define WS_PIN 7

#define TRIG1_PIN 3
#define TRIG2_PIN 9
#define TRIG3_PIN 11

#define ECHO1_PIN 2
#define ECHO2_PIN 8
#define ECHO3_PIN 10


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


static void ws_send_rgb(uint8_t r, uint8_t g, uint8_t b)
{
	ws_send_byte(g);
	ws_send_byte(r);
	ws_send_byte(b);
}


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

typedef enum {
	MEAS_WAIT_1,
	MEAS_WAIT_0,
	MEAS_DONE
} MeasPhase;


#define MBUF_LEN 16
typedef struct {
	float data[MBUF_LEN];
} MBuf;

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

static MBuf mb_offs1;
static MBuf mb_offs2;
static MBuf mb_offs3;

typedef struct {
	uint8_t r;
	uint8_t g;
	uint8_t b;
} RGB;

static RGB history[30];


static uint16_t meas(MBuf *mbuf, uint8_t TRIG, uint8_t ECHO)
{
	_delay_ms(6);
	pin_up_n(TRIG);
	_delay_ms(1);
	pin_down_n(TRIG);

	MeasPhase meas_phase = MEAS_WAIT_1;

	uint32_t echo = 0;

	TCNT1 = 0;
	TCCR1B = (0b010 << CS10);

	while (true) {
		if (meas_phase == MEAS_WAIT_1) {
			if (pin_is_high_n(ECHO)) {
				echo = TCNT1;
				meas_phase = MEAS_WAIT_0;
			}
		} else if (meas_phase == MEAS_WAIT_0) {
			if (pin_is_low_n(ECHO)) {
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

	// Both pulses measured with 0.5us accuracy

	// Convert to mm

	float offset = 255 - echo / (1.25f * 25.0f); //25

	if (offset > 255) {
		offset = 255;
	} else if (offset < 0) {
		offset = 0;
	}

	offset = mbuf_add(mbuf, offset);

	return (uint16_t) roundf(offset); // converted to RGB-level
}




static void sonar_measure(void)
{
	uint16_t c1 = meas(&mb_offs1, TRIG1_PIN, ECHO1_PIN);
	uint16_t c2 = meas(&mb_offs2, TRIG2_PIN, ECHO2_PIN);
	uint16_t c3 = meas(&mb_offs3, TRIG3_PIN, ECHO3_PIN);

	for (int i = 30 - 1; i > 0; i--) {
		history[i].r = history[i - 1].r;
		history[i].g = history[i - 1].g;
		history[i].b = history[i - 1].b;
	}

	history[0].r = (uint8_t)c1;
	history[0].g = (uint8_t)c2;
	history[0].b = (uint8_t)c3;

	for (int i = 0; i < 30; i++) {
		ws_send_rgb(history[i].r, history[i].g, history[i].b);
	}

	ws_show();
}


int main(void)
{
	hw_init();

	usart_puts_P(PSTR("===========================\r\n"));
	usart_puts_P(PSTR("\r\n"));
	usart_puts_P(PSTR("RGB SONAR DEMO\r\n"));
	usart_puts_P(PSTR("FEL CVUT K338\r\n"));
	usart_puts_P(PSTR("\r\n"));
	usart_puts_P(PSTR("(c) Ondrej Hruska 2016\r\n"));
	usart_puts_P(PSTR("\r\n"));
	usart_puts_P(PSTR("WS2812B LED strip - DIN: D7\r\n"));
	usart_puts_P(PSTR("Sonars (HC-SR04) - trig/echo:\r\n"));
	usart_puts_P(PSTR("  \"red\"   D3/D2\r\n"));
	usart_puts_P(PSTR("  \"green\" D9/D8\r\n"));
	usart_puts_P(PSTR("  \"blue\"  D11/D10\r\n"));
	usart_puts_P(PSTR("\r\n"));
	usart_puts_P(PSTR("===========================\r\n"));

	int cnt = 0;

	while (1) {
		sonar_measure();

		if (++cnt == 20) {
			cnt = 0;
			pin_toggle(13);
		}
	}
}
