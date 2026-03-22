idf_component_register(SRCS "display.cpp" "main.cpp" "../../src/main/web.cpp" "../../src/main/serial.cpp" "../../src/main/wireless.cpp" "../../src/main/settings.cpp" "../../src/main/nmea.cpp" "../../src/main/utils.cpp"  "../../src/main/pypilot_client.cpp" "../../src/main/sensors.cpp" "../../src/main/signalk.cpp" "../../src/main/zeroconf.cpp" "../../sensors/common/dhcp.c" "../../linenoise/linenoise.c"
	INCLUDE_DIRS "." "../../src/main" "../../linenoise"  REQUIRES arduino TFT_eSPI nvs_flash esp_http_server spiffs app_update dns_server
)
