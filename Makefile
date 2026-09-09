# Host-first + IDF device targets.

IDF ?= idf.py
PORT ?= /dev/ttyUSB0

.PHONY: test test-render test-codec test-contract bench-codec mqtt-loopback \
	sim sim-mqtt clean \
	build build-esp32 build-lcd-smoke build-mqtt \
	flash flash-esp32 flash-lcd-smoke flash-mqtt flash-nvs monitor monitor-mqtt

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

# SDL CYD panel (needs libsdl2-dev / DISPLAY)
sim:
	$(MAKE) -C host/sim run-demo

# Live MQTT → SDL window (inject from another terminal)
sim-mqtt:
	$(MAKE) -C host/sim all
	$(MAKE) -C host/mqtt all
	tools/.venv/bin/python tools/wd_mqtt.py visual --device sim1

build: build-esp32

build-mqtt:
	@if [ ! -f build-esp32-mqtt/build.ninja ]; then \
	  $(IDF) -B build-esp32-mqtt -D SDKCONFIG=sdkconfig.esp32 -D MQTT_MAIN=1 set-target esp32; \
	fi
	$(IDF) -B build-esp32-mqtt -D SDKCONFIG=sdkconfig.esp32 -D MQTT_MAIN=1 reconfigure
	$(IDF) -B build-esp32-mqtt -D SDKCONFIG=sdkconfig.esp32 -D MQTT_MAIN=1 build

build-lcd-smoke:
	@if [ ! -f build-esp32-lcd-smoke/build.ninja ]; then \
	  $(IDF) -B build-esp32-lcd-smoke -D SDKCONFIG=sdkconfig.esp32 -D LCD_SMOKE=1 set-target esp32; \
	fi
	$(IDF) -B build-esp32-lcd-smoke -D SDKCONFIG=sdkconfig.esp32 -D LCD_SMOKE=1 reconfigure
	$(IDF) -B build-esp32-lcd-smoke -D SDKCONFIG=sdkconfig.esp32 -D LCD_SMOKE=1 build

build-esp32:
	@if [ ! -f build-esp32/build.ninja ]; then \
	  $(IDF) -B build-esp32 -D SDKCONFIG=sdkconfig.esp32 set-target esp32; \
	fi
	$(IDF) -B build-esp32 -D SDKCONFIG=sdkconfig.esp32 build

flash: flash-esp32

flash-esp32:
	$(IDF) -B build-esp32 -D SDKCONFIG=sdkconfig.esp32 -p $(PORT) flash

flash-lcd-smoke:
	$(IDF) -B build-esp32-lcd-smoke -D SDKCONFIG=sdkconfig.esp32 -D LCD_SMOKE=1 -p $(PORT) flash

flash-mqtt:
	$(IDF) -B build-esp32-mqtt -D SDKCONFIG=sdkconfig.esp32 -D MQTT_MAIN=1 -p $(PORT) flash

# Flash credentials into NVS (needs tools/nvs.csv + IDF env). See tools/nvs.example.csv
flash-nvs:
	bash tools/wd_flash_nvs.sh

monitor:
	$(IDF) -B build-esp32 -D SDKCONFIG=sdkconfig.esp32 monitor

monitor-mqtt:
	$(IDF) -B build-esp32-mqtt -D SDKCONFIG=sdkconfig.esp32 -D MQTT_MAIN=1 monitor

clean:
	$(MAKE) -C host/render clean
	$(MAKE) -C host/codec clean
	$(MAKE) -C host/contract clean
	$(MAKE) -C host/mqtt clean
	$(MAKE) -C host/sim clean
	rm -rf build build-esp32 build-esp32-lcd-smoke build-esp32-mqtt \
		sdkconfig sdkconfig.old sdkconfig.esp32 sdkconfig.*.old
