JS_MIN  := $(patsubst %.js,%.min.js,$(wildcard *.js))
CSS_MIN := $(patsubst %.css,%.min.css,$(wildcard *.css))
HTML_MIN := $(patsubst %.html,%.min.html,$(wildcard *.html))


WEB_SRC := $(filter-out %.gz,$(wildcard data/*)) data/chart.js
WEB_GZ  := $(addsuffix .gz,$(WEB_SRC))

all: main/data.h

main/data.h: $(WEB_GZ) filetoarray.py
	python3 filetoarray.py data/*.gz > main/data.h

%.gz: %
	gzip -9 -c $< > $@

data/chart.min.js:
	wget -O data/chart.js -c https://cdnjs.cloudflare.com/ajax/libs/Chart.js/4.4.1/chart.min.js

old_spifs:
	./mkspiffs/mkspiffs-0.2.3-7-gf248296-arduino-esp32-linux64/mkspiffs -b 4096 -p 256 -s 0x100000 -c data spiffs.bin
	esptool.py --chip esp32 --port /dev/ttyUSB0 --baud 460800 write_flash -z 0x320000 spiffs.bin

