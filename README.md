# arduino-gamepad
super simple gamepad project where Pro Mini is in a gamepad and sends keys over USART

LED connected to D9 (on-board one at D13).

Buttons connected from pins to ground with internal pull-up.

Keys are sent as A - pressed, a - released (different letters), over UART at 115200 baud.

Key press is indicated by LED at D9.

