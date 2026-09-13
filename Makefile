# Host-first + IDF device targets.

IDF ?= idf.py
PORT ?= /dev/ttyUSB0

.PHONY: test test-render test-codec test-contract test-bind test-net mqtt-loopback \
	sim sim-mqtt demo demo-fetch demo-sim showcase showcase-sim panel panel-sim scene clean audit-mem \
	build build-esp32 build-lcd-smoke build-mqtt \
	flash flash-esp32 flash-lcd-smoke flash-mqtt flash-nvs monitor monitor-mqtt

test: test-render test-codec test-contract test-bind test-net

test-render:
	$(MAKE) -C host/render test

test-codec:
	$(MAKE) -C host/codec test

test-contract:
	$(MAKE) -C host/contract test

test-bind:
	$(MAKE) -C host/bind test

test-net:
	$(MAKE) -C host/net test

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

# Demo: cache originals (JPEG); convert RGB565 only at play time
demo-fetch:
	tools/.venv/bin/python tools/wd_demo.py fetch

demo:
	$(MAKE) -C host/mqtt all
	tools/.venv/bin/python tools/wd_demo.py --device $${WD_DEVICE:-cyd1}

demo-sim:
	$(MAKE) -C host/sim all
	$(MAKE) -C host/mqtt all
	tools/.venv/bin/python tools/wd_demo.py --device sim1 --visual

# Step 14 flagship (W19). Glass: make showcase. SDL: make showcase-sim.
# Pack-only: tools/.venv/bin/python tools/wd_showcase.py --dry-run
showcase:
	$(MAKE) -C host/mqtt all
	tools/.venv/bin/python tools/wd_showcase.py --device $${WD_DEVICE:-cyd1}

showcase-sim:
	$(MAKE) -C host/sim all
	$(MAKE) -C host/mqtt all
	tools/.venv/bin/python tools/wd_showcase.py --device sim1 --visual

# Clock + weather panel daemon (docs/plans/weather-clock-panel.md)
# Default place: Rehovot, IL (override WD_LAT/WD_LON/WD_PLACE). Offline: --fake.
panel:
	$(MAKE) -C host/mqtt all
	tools/.venv/bin/python tools/wd_panel.py --device $${WD_DEVICE:-cyd1}

panel-sim:
	$(MAKE) -C host/sim all
	$(MAKE) -C host/mqtt all
	tools/.venv/bin/python tools/wd_panel.py --device sim1 --visual --fake --weather-min 2

# YAML scene composer (Step 9). Override: make scene SCENE=tools/scenes/hello.yaml WD_DEVICE=cyd1
SCENE ?= tools/scenes/hello.yaml
scene:
	$(MAKE) -C host/mqtt all
	tools/.venv/bin/python tools/wd_scene.py $(SCENE) --device $${WD_DEVICE:-cyd1}

build: build-esp32

build-mqtt:
	@if [ ! -f build-esp32-mqtt/build.ninja ]; then \
	  $(IDF) -B build-esp32-mqtt -D SDKCONFIG=sdkconfig.esp32 -D MQTT_MAIN=1 set-target esp32; \
	fi
	$(IDF) -B build-esp32-mqtt -D SDKCONFIG=sdkconfig.esp32 -D MQTT_MAIN=1 reconfigure
	$(IDF) -B build-esp32-mqtt -D SDKCONFIG=sdkconfig.esp32 -D MQTT_MAIN=1 build

# Const→flash / pools→DRAM check (needs build-mqtt map)
audit-mem:
	python3 tools/audit_elf_mem.py

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
