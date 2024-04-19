all:
	./mkspiffs/mkspiffs-0.2.3-7-gf248296-arduino-esp32-linux64/mkspiffs -b 4096 -p 256 -s 0x170000 -c pypilot_mfd/data spiffs.bin
	esptool.py --chip esp32 --port /dev/ttyUSB0 --baud 460800 write_flash -z 0x290000 spiffs.bin

