.PHONY: webui-test webui-test-watch webui-dev webui-build webui-lint \
        server-run server-install

WATERING_APP_DIR := device/sf_watering/watering_app

# ============================================================================
# Firmware Build Targets (ESP-IDF via Docker)
# ============================================================================

IDF_VER       ?= release-v5.4
IDF_IMAGE     := espressif/idf:$(IDF_VER)
IDF_TARGET    ?= esp32

IDF_DOCKER_RUN = docker run --rm \
	-v $(CURDIR):/project \
	-w /project \
	$(IDF_IMAGE)

# Build firmware for a specific target
# Usage: make fw-build IDF_TARGET=esp32
#        make fw-build IDF_TARGET=esp32s3
fw-build:
	$(IDF_DOCKER_RUN) bash -c "\
		. \$$IDF_PATH/export.sh && \
		idf.py set-target $(IDF_TARGET) && \
		idf.py build"

# Shorthand targets for each chip
fw-build-esp32:
	$(MAKE) fw-build IDF_TARGET=esp32

fw-build-esp32s3:
	$(MAKE) fw-build IDF_TARGET=esp32s3

# Emulation builds (SF_EMULATION_BUILD env var controls component graph,
# sdkconfig_ci enables Kconfig options like partition table and OpenETH)
fw-build-emulation:
	$(IDF_DOCKER_RUN) bash -c "\
		export SF_EMULATION_BUILD=1 && \
		. \$$IDF_PATH/export.sh && \
		idf.py -DSDKCONFIG_DEFAULTS='sdkconfig_ci' set-target $(IDF_TARGET) && \
		idf.py build"

fw-build-emulation-esp32:
	$(MAKE) fw-build-emulation IDF_TARGET=esp32

fw-build-emulation-esp32s3:
	$(MAKE) fw-build-emulation IDF_TARGET=esp32s3

# Clean the build directory (uses Docker since build/ is owned by root)
fw-clean:
	$(IDF_DOCKER_RUN) rm -rf build

# ============================================================================
# Web UI Targets
# ============================================================================

webui-test:
	cd $(WATERING_APP_DIR)/webui && npm run test:run

webui-test-watch:
	cd $(WATERING_APP_DIR)/webui && npm test

webui-dev:
	cd $(WATERING_APP_DIR)/webui && npm run dev

webui-build:
	cd $(WATERING_APP_DIR)/webui && npm run build

webui-lint:
	cd $(WATERING_APP_DIR)/webui && npm run lint

# ============================================================================
# Server Targets
# ============================================================================

server-run:
	cd $(WATERING_APP_DIR)/server && python backend.py

server-install:
	cd $(WATERING_APP_DIR)/server && pip install -r requirements.txt
