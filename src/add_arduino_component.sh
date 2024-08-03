mkdir -p components
cd components/
git clone --depth 1 https://github.com/espressif/arduino-esp32.git
cd arduino
git submodule update --init --recursive
cd ..
git clone --depth 1 --recursive https://github.com/me-no-dev/ESPAsyncWebServer
git clone --depth 1 --recursive https://github.com/me-no-dev/AsyncTCP
