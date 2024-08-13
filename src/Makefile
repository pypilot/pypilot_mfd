all: data/chart.js
	python3 filetoarray.py data/* > main/data.h

data/chart.js:
	wget -O $@ -c https://cdnjs.cloudflare.com/ajax/libs/Chart.js/4.4.1/chart.min.js

old_spifs:
	./mkspiffs/mkspiffs-0.2.3-7-gf248296-arduino-esp32-linux64/mkspiffs -b 4096 -p 256 -s 0x100000 -c data spiffs.bin
	esptool.py --chip esp32 --port /dev/ttyUSB0 --baud 460800 write_flash -z 0x320000 spiffs.bin

