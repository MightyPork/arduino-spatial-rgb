# Spatial RGB

## What

- Arduino
- 3 sonars
- 3 colors in the spectrum
- RGB led strip
- Magic!

## Building

To run this project, you need Arduino Pro Mini, or Nano (those $2 eBay clones will work perfectly).
You can also try to use a genuine Arduino, even larger (UNO), though I haven't tried that.

To flash the firmware, run `make flash`. Adjustment the Makefile as needed. Naturally, you'll need 
`avr-gcc` and `avrdude` installed (and Linux or OSX). 

Make sure the correct Serial device is defined in the Makefile (`/dev/ttyUSB0` or other - it tends
to be something really strange on OSX).

## Hardware

- RGB LED strip with WS2812 or WS2812B. It's set up for a 30-led strip, adjust as needed.




