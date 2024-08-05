idf_component_register(SRCS "accel.cpp" "ais.cpp" "alarm.cpp" "buzzer.cpp" "display.cpp" "draw.cpp" "history.cpp" "keys.cpp" "main.cpp" "menu.cpp" "nmea.cpp" "pypilot_client.cpp" "serial.cpp" "settings.cpp" "signalk.cpp" "utils.cpp" "web.cpp" "wireless.cpp" "zeroconf.cpp"
	INCLUDE_DIRS "."
	EMBED_TXTFILES "../data/alarms.html" "../data/data.html" "../data/data.js" "../data/favicon.ico" "../data/history.html" "../data/history.js" "../data/index.html" "../data/mfd.js" "../data/styles.css" 
)
