#include <avr/io.h>
#include <avr/pgmspace.h>
#include <stdbool.h>
#include <stdlib.h>


#include "debounce.h"
#include "calc.h"
#include "iopins.h"

#include "lib/usart.h"

DeboSlot debo_slots[DEBO_CHANNELS];

/** Debounce data array */
static uint8_t debo_next_slot = 0;

uint8_t debo_add_do(PORT_P pin_ptr, uint8_t bit, bool invert, void (*handler)(uint8_t, bool))
{
	DeboSlot *slot = &debo_slots[debo_next_slot];

	slot->reg = pin_ptr;
	slot->mask = (uint8_t)(1 << bit);
	slot->invert = invert;
	slot->count = 0;
	slot->state = (*slot->reg & slot->mask);
	slot->handler = handler;

	return debo_next_slot++;
}


/** Check debounced pins, should be called periodically. */
void debo_tick(void)
{
	for (uint8_t i = 0; i < debo_next_slot; i++) {
		DeboSlot *slot = &debo_slots[i];

		// current pin value (right 3 bits, xored with inverse bit)
		bool state = (*slot->reg & slot->mask);

		if (state != slot->state) {
			// different pin state than last recorded state
			if (slot->count < DEBO_TICKS) {
				slot->count++;
			} else {
				// overflown -> latch value
				slot->state = state; // set state bit
				slot->count = 0;

				if (slot->handler != NULL) {
					slot->handler(i, slot->invert ^ state);
				}
			}
		} else {
			slot->count = 0;; // reset the counter
		}
	}
}
