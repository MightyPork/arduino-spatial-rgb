#pragma once

//
//  An implementation of button debouncer.
//
//  ----
//
//  You must provide a config file debo_config.h (next to your main.c)
//
//  A pin is registered like this:
//
//    #define BTN1 12 // pin D12
//    #define BTN2 13
//
//    debo_add(BTN0);  // The function returns number assigned to the pin (0, 1, ...)
//    debo_add_rev(BTN1);  // active low
//    debo_register(&PINB, PB2, 0);  // direct access - register, pin & invert
//
//  Then periodically call the tick function (perhaps in a timer interrupt):
//
//    debo_tick();
//
//  To check if input is active, use
//
//    debo_get_pin(0); // state of input #0 (registered first)
//    debo_get_pin(1); // state of input #1 (registered second)
//


#include <avr/io.h>
#include <stdbool.h>
#include <stdint.h>

#include "calc.h"
#include "iopins.h"

#define DEBO_CHANNELS 11
#define DEBO_TICKS 20


/* Internal deboucer entry */
typedef struct
{
	PORT_P reg;    // pin ptr
	uint8_t mask;
	uint8_t count; // number of ticks this was in the new state
	bool invert;
	bool state;
	void (*handler)(uint8_t pin_n, bool state);
} DeboSlot;

extern DeboSlot debo_slots[DEBO_CHANNELS];

/** Add a pin for debouncing (must be used with constant args) */
#define debo_add(pin, reverse, hdlr) debo_add_do(&_pin(pin), _pn(pin), reverse, hdlr)

/** Add a pin for debouncing (low level function) */
uint8_t debo_add_do(PORT_P pin_reg_pointer, uint8_t bit, bool invert, void (*handler)(uint8_t, bool));

/** Check debounced pins, should be called periodically. */
void debo_tick(void);

/** Get a value of debounced pin */
#define debo_get_pin(i) (debo_slots[i].state ^ debo_slots[i].invert)
