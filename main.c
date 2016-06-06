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

static void sonar_start(void);

#define WS_PIN 6

#define TRIG_PIN 2

#define ECHO1_PIN 3
#define ECHO2_PIN 4
#define ECHO3_PIN 5


/** Wait long enough for the colors to show */
static void ws_show(void)
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
			__asm__ volatile ("nop");
			__asm__ volatile ("nop");
			__asm__ volatile ("nop");
			__asm__ volatile ("nop");
			__asm__ volatile ("nop");
			pin_down(WS_PIN);
			__asm__ volatile ("nop");
		} else {
			__asm__ volatile ("nop");
			pin_down(WS_PIN);
			__asm__ volatile ("nop");
			__asm__ volatile ("nop");
			__asm__ volatile ("nop");
			__asm__ volatile ("nop");
			__asm__ volatile ("nop");
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

	as_output(2);

	as_input_pu(3);
	as_input_pu(4);
	as_input_pu(5);

	as_output(6); // WS
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
		float m = buf->data[i-1];
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

static void sonar_measure(void)
{
	pin_up(TRIG_PIN);
	_delay_ms(10);
	pin_down(TRIG_PIN);

	MeasPhase e1_ph = MEAS_WAIT_1;
	MeasPhase e2_ph = MEAS_WAIT_1;
	MeasPhase e3_ph = MEAS_WAIT_1;

	uint32_t echo1 = 0;
	uint32_t echo2 = 0;
	uint32_t echo3 = 0;

	TCNT1 = 0;
	TCCR1B = (0b010 << CS10);
	while (true) {
		switch (e1_ph) {
			case MEAS_WAIT_1:
				if (pin_is_high(ECHO1_PIN)) {
					echo1 = TCNT1;
					e1_ph = MEAS_WAIT_0;
				}
				break;

			case MEAS_WAIT_0:
				if (pin_is_low(ECHO1_PIN)) {
					echo1 = TCNT1 - echo1;
					e1_ph = MEAS_DONE;
				}
				break;
		}

		switch (e2_ph) {
			case MEAS_WAIT_1:
				if (pin_is_high(ECHO2_PIN)) {
					echo2 = TCNT1;
					e2_ph = MEAS_WAIT_0;
				}
				break;

			case MEAS_WAIT_0:
				if (pin_is_low(ECHO2_PIN)) {
					echo2 = TCNT1 - echo2;
					e2_ph = MEAS_DONE;
				}
				break;
		}

		switch (e3_ph) {
			case MEAS_WAIT_1:
				if (pin_is_high(ECHO3_PIN)) {
					echo3 = TCNT1;
					e3_ph = MEAS_WAIT_0;
				}
				break;

			case MEAS_WAIT_0:
				if (pin_is_low(ECHO3_PIN)) {
					echo3 = TCNT1 - echo3;
					e3_ph = MEAS_DONE;
				}
				break;
		}

		if (e1_ph == MEAS_DONE && e2_ph == MEAS_DONE && e3_ph == MEAS_DONE) {
			break; // done
		}
	}

	TCCR1B = 0; // stop

	// Both pulses measured with 0.5us accuracy

	// Convert to mm

	echo1 *= 8000;
	echo1 /= 10000;

	echo2 *= 8000;
	echo2 /= 10000;

	echo3 *= 8000;
	echo3 /= 10000;

	float offset1 = 255 - echo1 / 25.0f;
	float offset2 = 255 - echo2 / 25.0f;
	float offset3 = 255 - echo3 / 25.0f;

	if (offset1 > 255) offset1 = 255;
	if (offset2 > 255) offset2 = 255;
	if (offset3 > 255) offset3 = 255;
	if (offset1 < 0) offset1 = 0;
	if (offset2 < 0) offset2 = 0;
	if (offset3 < 0) offset3 = 0;

	offset1 = mbuf_add(&mb_offs1, offset1);
	offset2 = mbuf_add(&mb_offs2, offset2);
	offset3 = mbuf_add(&mb_offs3, offset3);

	int c1 = (int) roundf(offset1);
	int c2 = (int) roundf(offset2);
	int c3 = (int) roundf(offset3);

//	char buf[30];

	for (int i = 30 - 1; i > 0; i--) {
		history[i].r = history[i-1].r;
		history[i].g = history[i-1].g;
		history[i].b = history[i-1].b;

//		sprintf(buf, "%d %d %d\n", history[i].r, history[i].g, history[i].b);
//		usart_puts(buf);
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

	while (1) {
		sonar_measure();
		_delay_ms(10);
	}
}
