.PHONY: webui-test webui-test-watch webui-dev webui-build webui-lint \
        server-run server-install

WATERING_APP_DIR := device/sf_watering/watering_app

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
