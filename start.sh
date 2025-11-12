#!/bin/bash

# Start script for NUS-SmartStop Flask server
# Note: This project does NOT use Docker
# Services like Mosquitto, Telegraf, and InfluxDB should be installed 
# and managed via systemd on Ubuntu (see docs/DEPLOYMENT.md)

echo "Starting NUS-SmartStop Flask Server..."
echo "======================================="

# Check if .env exists in server/flask directory
if [ ! -f server/flask/.env ]; then
    echo "Error: server/flask/.env file not found!"
    echo "Please copy server/flask/.env.example to server/flask/.env and configure it."
    exit 1
fi

# Check if Python virtual environment exists
if [ -d "venv" ]; then
    echo "Activating virtual environment..."
    source venv/bin/activate
else
    echo "Warning: Virtual environment not found. Using system Python."
fi

# Check if dependencies are installed
echo "Checking Python dependencies..."
if ! python3 -c "import flask" &> /dev/null; then
    echo "Installing Python dependencies..."
    pip3 install -r server/flask/requirements.txt
fi

# Create uploads directory if it doesn't exist
mkdir -p server/flask/uploads

# Start Flask server in background
echo "Starting Flask server..."
python3 server/flask/image_server.py &
FLASK_PID=$!
echo "Flask server started (PID: $FLASK_PID)"

echo ""
echo "======================================="
echo "Flask server started successfully!"
echo "======================================="
echo ""
echo "Flask API: http://localhost:5000"
echo "Process ID: $FLASK_PID"
echo ""
echo "IMPORTANT: Ensure these services are running via systemd:"
echo "  - Mosquitto MQTT Broker (sudo systemctl status mosquitto)"
echo "  - Telegraf (sudo systemctl status telegraf)"
echo "  - InfluxDB (sudo systemctl status influxdb)"
echo ""
echo "See docs/DEPLOYMENT.md for production setup with systemd"
echo ""
echo "To stop Flask server, run: ./stop.sh"
echo "Or use: kill $FLASK_PID"
echo ""

# Keep script running
wait
