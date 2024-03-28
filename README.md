This repository contains code for a multifunction display

The data formats currently are nmea0183, signalK, and direct communication to pypilot.

It supports zeroconf (mDNS) to detect pypilot and signalk servers.   There is support for wifi, usb (serial port) and nmea0183 (rs422) communications.

Many displays are supported by u8g2 library or tft_spi library, but I specifically target jlx256160 and the ttgo_t_display.

Wireless wind sensors that transmit data via esp-now can be received and displayed or relayed over the various data interfaces.
