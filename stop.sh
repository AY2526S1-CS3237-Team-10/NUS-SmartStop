#!/bin/bash

# Stop script for NUS-SmartStop servers
# This script stops all running services

echo "Stopping NUS-SmartStop Services..."
echo "==================================="

# Stop Python processes
echo "Stopping Python services..."
pkill -f "server/flask/app.py"
pkill -f "server/mqtt/mqtt_client.py"

# Stop Docker services if available
if command -v docker-compose &> /dev/null; then
    echo "Stopping Docker services..."
    docker-compose down
fi

echo ""
echo "All services stopped."
