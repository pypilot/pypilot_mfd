.PHONY: all clean js css html min

all: min js css html favicon.ico.gz ../main/data.h

min:
	mkdir -p min

clean:
	rm -rf *.js.gz *.css.gz *.html.gz favicon.ico.gz min

CSS := $(wildcard *.css)
JS := $(wildcard *.js)
HTML := $(wildcard *.html)

CSS_MIN := $(patsubst %.css,min/%.css,$(CSS))
JS_MIN  := $(patsubst %.js,min/%.js,$(JS)) min/chart.js
HTML_MIN := $(patsubst %.html,min/%.html,$(HTML))

define mkcompress
$(1) := $$(patsubst %,%.gz,$$($(2))) #$$(patsubst %,%.br,$$($(2)))
endef

$(eval $(call mkcompress,CSS_MIN_GZ,CSS_MIN))
$(eval $(call mkcompress,JS_MIN_GZ,JS_MIN))
$(eval $(call mkcompress,HTML_MIN_GZ,HTML_MIN))

$(eval $(call mkcompress,CSS_GZ,CSS))
$(eval $(call mkcompress,JS_GZ,JS))
$(eval $(call mkcompress,HTML_GZ,HTML))

css: $(CSS_GZ) $(CSS_MIN_GZ)
js:  $(JS_GZ) $(JS_MIN_GZ)
html: $(HTML_GZ) $(HTML_MIN_GZ)

../main/data.h: $(CSS) $(JS) $(HTML) ../filetoarray.py
	#python ../filetoarray.py min/*.gz > ../main/data.h
	python ../filetoarray.py *.gz > ../main/data.h

min/%.css: %.css
	cleancss $< -o $@

min/%.js : %.js
	terser $< -o $@

min/%.html: %.html
	html-minifier-terser $< \
		--collapse-whitespace \
		--remove-comments \
		--remove-optional-tags \
		--remove-redundant-attributes \
		--remove-script-type-attributes \
		--remove-style-link-type-attributes \
		-o $@

%.gz : %
	gzip -k -9 $<

%.br: %
	brotli -k -f -q 11 $<

chart.js:
	wget -O chart.js -c https://cdnjs.cloudflare.com/ajax/libs/Chart.js/4.4.1/chart.min.js

old_spifs:
	./mkspiffs/mkspiffs-0.2.3-7-gf248296-arduino-esp32-linux64/mkspiffs -b 4096 -p 256 -s 0x100000 -c data spiffs.bin
	esptool.py --chip esp32 --port /dev/ttyUSB0 --baud 460800 write_flash -z 0x320000 spiffs.bin

