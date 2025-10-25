#!/bin/bash

# Start script for NUS-SmartStop servers
# This script starts all necessary services

echo "Starting NUS-SmartStop Services..."
echo "=================================="

# Check if .env exists
if [ ! -f .env ]; then
    echo "Error: .env file not found!"
    echo "Please copy .env.example to .env and configure it."
    exit 1
fi

# Check if Docker is available
if command -v docker-compose &> /dev/null; then
    echo "Starting Docker services (InfluxDB, Mosquitto)..."
    docker-compose up -d
    
    # Wait for services to be ready
    echo "Waiting for services to be ready..."
    sleep 5
else
    echo "Warning: docker-compose not found. Assuming manual service setup."
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
if ! python -c "import flask" &> /dev/null; then
    echo "Installing Python dependencies..."
    pip install -r requirements.txt
fi

# Create uploads directory if it doesn't exist
mkdir -p uploads

# Start Flask server in background
echo "Starting Flask server..."
python server/flask/app.py &
FLASK_PID=$!
echo "Flask server started (PID: $FLASK_PID)"

# Wait a moment
sleep 2

# Start MQTT client in background
echo "Starting MQTT client..."
python server/mqtt/mqtt_client.py &
MQTT_PID=$!
echo "MQTT client started (PID: $MQTT_PID)"

echo ""
echo "=================================="
echo "All services started successfully!"
echo "=================================="
echo ""
echo "Services running:"
echo "- InfluxDB: http://localhost:8086"
echo "- Flask API: http://localhost:5000"
echo "- MQTT Broker: localhost:1883"
echo ""
echo "Process IDs:"
echo "- Flask: $FLASK_PID"
echo "- MQTT Client: $MQTT_PID"
echo ""
echo "To stop services, run: ./stop.sh"
echo "Or press Ctrl+C and use: kill $FLASK_PID $MQTT_PID"
echo ""

# Keep script running
wait
