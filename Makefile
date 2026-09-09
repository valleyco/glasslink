# Host-first + IDF device targets.

IDF ?= idf.py
PORT ?= /dev/ttyUSB0

.PHONY: test test-render test-codec test-contract bench-codec mqtt-loopback clean \
	build build-esp32 build-lcd-smoke flash flash-esp32 flash-lcd-smoke monitor

test: test-render test-codec test-contract

test-render:
	$(MAKE) -C host/render test

test-codec:
	$(MAKE) -C host/codec test

test-contract:
	$(MAKE) -C host/contract test

bench-codec:
	$(MAKE) -C host/codec bench

# Host MQTT vs local Mosquitto (no device flash)
mqtt-loopback:
	$(MAKE) -C host/mqtt all
	tools/.venv/bin/python tools/wd_mqtt.py loopback --device loop1

build: build-esp32

build-esp32:
	$(IDF) -B build-esp32 -D SDKCONFIG=sdkconfig.esp32 set-target esp32
	$(IDF) -B build-esp32 -D SDKCONFIG=sdkconfig.esp32 build

build-lcd-smoke:

build-mqtt:
	$(IDF) -B build-esp32-mqtt -D SDKCONFIG=sdkconfig.esp32 -D MQTT_MAIN=1 set-target esp32
	$(IDF) -B build-esp32-mqtt -D SDKCONFIG=sdkconfig.esp32 -D MQTT_MAIN=1 build
	$(IDF) -B build-esp32-lcd-smoke -D SDKCONFIG=sdkconfig.esp32 -D LCD_SMOKE=1 set-target esp32
	$(IDF) -B build-esp32-lcd-smoke -D SDKCONFIG=sdkconfig.esp32 -D LCD_SMOKE=1 build

flash: flash-esp32

flash-esp32:
	$(IDF) -B build-esp32 -D SDKCONFIG=sdkconfig.esp32 -p $(PORT) flash

flash-lcd-smoke:
	$(IDF) -B build-esp32-lcd-smoke -D SDKCONFIG=sdkconfig.esp32 -D LCD_SMOKE=1 -p $(PORT) flash

monitor:
	$(IDF) -B build-esp32 -D SDKCONFIG=sdkconfig.esp32 monitor

clean:
	$(MAKE) -C host/render clean
	$(MAKE) -C host/codec clean
	$(MAKE) -C host/contract clean
	$(MAKE) -C host/mqtt clean
	rm -rf build build-esp32 build-esp32-lcd-smoke \
		sdkconfig sdkconfig.old sdkconfig.esp32 sdkconfig.*.old
