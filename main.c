#include <avr/io.h>          // register definitions
#include <avr/pgmspace.h>    // storing data in program memory
#include <avr/interrupt.h>   // interrupt vectors
#include <util/delay.h>      // delay functions

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>

// Include stuff from the library
#include "lib/iopins.h"
#include "lib/usart.h"
#include "lib/nsdelay.h"

static void sonar_start(void);

#define WS_PIN 5

#define TRIG_PIN 2
#define ECHO1_PIN 3
#define ECHO2_PIN 4


/** Wait long enough for the colors to show */
static void ws_show(void)
{
	_delay_us(10);
}

static inline __attribute__((always_inline))
void delay_cyc(uint8_t __count)
{
	__asm__ volatile (
		"1: dec %0" "\n\t"
		"brne 1b"
		: "=r" (__count)
		: "0" (__count)
	);
}


/** Send one byte to the RGB strip */
static inline  __attribute__((always_inline))
void ws_send_byte(uint8_t bb)
{
	for (int8_t i = 8; i > 0; i--) {
		pin_up(WS_PIN);
		if (bb & 0x80) {
			delay_cyc(4);
			pin_down(WS_PIN);
			delay_cyc(1);
		} else {
			delay_cyc(1);
			pin_down(WS_PIN);
			delay_cyc(4);
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
	as_output(5); // WS
}

typedef enum {
	MEAS_WAIT_1,
	MEAS_WAIT_0,
	MEAS_DONE
} MeasPhase;


#define PULSE_LEN 9
const uint8_t pulse[PULSE_LEN] = {79, 150, 206, 243, 255, 243, 206, 150, 79};


#define MBUF_LEN 64
typedef struct {
	float data[MBUF_LEN];
} MBuf;

float mbuf_add(MBuf *buf, float value)
{
	float aggr = value;
	for (int i = 1; i < MBUF_LEN; i++) {
		aggr += buf->data[i];
		buf->data[i] = buf->data[i-1];
	}
	buf->data[0] = value;

	return aggr / (float)MBUF_LEN;
}

MBuf mb_offs1;
MBuf mb_offs2;


static void sonar_measure(void)
{
	usart_puts("MEASURE!\n");

	pin_up(TRIG_PIN);
	_delay_ms(10);
	pin_down(TRIG_PIN);

	MeasPhase e1_ph = MEAS_WAIT_1;
	MeasPhase e2_ph = MEAS_WAIT_1;

	uint32_t echo1 = 0;
	uint32_t echo2 = 0;

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

		if (e1_ph == MEAS_DONE && e2_ph == MEAS_DONE) {
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

	char buf[10];

	ltoa(echo1, buf, 10);
	usart_puts("echo1: ");
	usart_puts(buf);
	usart_puts(" -> ");

	float offset1 = echo1 / 100.0f;
	float offset2 = echo2 / 100.0f;

	offset1 = mbuf_add(&mb_offs1, offset1);
	offset2 = mbuf_add(&mb_offs2, offset2);

	int of1i = (int) roundf(offset1);
	int of2i = (int) roundf(offset2);

	of1i = 15 - of1i;
	of2i = 15 - of2i;

	int gi = -15 + PULSE_LEN/2 - of1i - 15;
	int bi = -15 + PULSE_LEN/2 + of2i + 15;
	int ri = -15 + PULSE_LEN/2 + 0;

	uint8_t r, g, b;
	for (int i = 0; i < 32; i++) {
		if (ri >= 0 && ri < PULSE_LEN) {
			r = pulse[ri];
		} else {
			r = 0;
		}

		if (gi >= 0 && gi < PULSE_LEN) {
			g = pulse[gi];
		} else {
			g = 0;
		}

		if (bi >= 0 && bi < PULSE_LEN) {
			b = pulse[bi];
		} else {
			b = 0;
		}

		ws_send_rgb(r,g,b);

		ri++;
		gi++;
		bi++;
	}

	ws_show();
}


int main(void)
{
	hw_init();

	while (1) {
		sonar_measure();
		_delay_ms(20);
	}
}
