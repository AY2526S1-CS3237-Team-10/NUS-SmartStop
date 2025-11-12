#!/bin/bash

# Stop script for NUS-SmartStop Flask server
# Note: This project does NOT use Docker
# This only stops the Flask server process

echo "Stopping NUS-SmartStop Flask Server..."
echo "======================================="

# Stop Flask server process
echo "Stopping Flask server..."
pkill -f "server/flask/image_server.py"
pkill -f "server/flask/esp32cam_image_server.py"

echo ""
echo "Flask server stopped."
echo ""
echo "Note: Systemd services (Mosquitto, Telegraf, InfluxDB) are not affected."
echo "To manage systemd services, use: sudo systemctl stop/start <service-name>"
echo ""
