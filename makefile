PROJECT_NAME = tcp-udp-server
BINARY_NAME = Server
SERVICE_NAME = $(PROJECT_NAME).service
BUILD_DIR = build
INSTALL_DIR = /usr/local/bin
SERVICE_DIR = /etc/systemd/system
PORT ?= 8087

.PHONY: all
all: build

.PHONY: build
build:
	@mkdir -p $(BUILD_DIR)
	@cd $(BUILD_DIR) && cmake ..
	@cd $(BUILD_DIR) && make
	@echo "Build completed successfully"

.PHONY: clean
clean:
	@rm -rf $(BUILD_DIR)

.PHONY: install
install:
	@echo "Installing $(BINARY_NAME) to $(INSTALL_DIR)..."
	@sudo install -m 755 $(BUILD_DIR)/$(BINARY_NAME) $(INSTALL_DIR)/$(BINARY_NAME)
	@echo "Installing systemd service to $(SERVICE_DIR)..."
	@sudo install -m 644 $(SERVICE_NAME) $(SERVICE_DIR)/$(SERVICE_NAME)
	@sudo systemctl daemon-reload
	@echo "Installation completed successfully"

.PHONY: uninstall
uninstall:
	@echo "Uninstalling $(PROJECT_NAME)..."
	@sudo systemctl stop $(SERVICE_NAME) 2>/dev/null
	@sudo systemctl disable $(SERVICE_NAME) 2>/dev/null
	@sudo rm -f $(INSTALL_DIR)/$(BINARY_NAME)
	@sudo rm -f $(SERVICE_DIR)/$(SERVICE_NAME)
	@sudo systemctl daemon-reload
	@echo "Uninstallation completed"

.PHONY: run
run:
	@echo "Starting $(SERVICE_NAME) on port $(PORT)..."
	@sudo sed -i "s/Environment=SERVER_PORT=.*/Environment=SERVER_PORT=$(PORT)/" $(SERVICE_DIR)/$(SERVICE_NAME)
	@sudo systemctl daemon-reload
	@sudo systemctl enable $(SERVICE_NAME)
	@sudo systemctl restart $(SERVICE_NAME)
	@echo "Service started on port $(PORT)"

.PHONY: stop
stop:
	@sudo systemctl stop $(SERVICE_NAME) 2>/dev/null
	@echo "Service stopped"

.PHONY: status
status:
	@sudo systemctl status $(SERVICE_NAME)

.PHONY: logs
logs:
	@sudo journalctl -u $(SERVICE_NAME) -f

.PHONY: restart
restart: stop run

.PHONY: help
help:
	@echo "Available targets:"
	@echo "  build     - Build the project using CMake"
	@echo "  clean     - Clean build files"
	@echo "  install   - Install binary and systemd service"
	@echo "  uninstall - Remove binary and systemd service"
	@echo "  run       - Start server via systemd (use PORT=8087)"
	@echo "  stop      - Stop the server"
	@echo "  status    - Show server status"
	@echo "  logs      - Show server logs in real-time"
	@echo "  restart   - Restart the server"
	@echo "  help      - Show this help message"
	@echo ""