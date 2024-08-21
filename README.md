This repository contains code for an open source multifunction marine display.

The data formats currently are nmea0183, signalK, and direct communication to pypilot.

It supports zeroconf (mDNS) to detect pypilot and signalk servers.   There is support for wifi, usb (serial port) and nmea0183 (rs422) communications.

The esp32s3 can drive high resolution (800x480) RGB parallel dayligth visible lcd.

A standard esp32 can be used for monochrome displays eg: jlx256160

3d printed enclosures may be waterproofed by treating (100% infill and soaking with dichtol) (FDM) or by printing with SLA/SLS.  The plexiglass cover is sealed with a rubber oring tested submerged under water for prolonged periods.

Wireless sensors are implemented specifically:
-  3d printed wind sensors using ceramic bearings and hall effect sensors (non contact)  Multiple wind sensors can be used mounted in different locations on the boat, and the sensors can compensate motion using inertial sensors.
-  pressure, humidity and air quality sensors (bme680)
-  lightning, uv and visible light sensors
-  (planned but not fully implemented)  sonar for depth, water speed, water temperature.
