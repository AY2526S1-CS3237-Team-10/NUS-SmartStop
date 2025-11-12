# NUS-SmartStop

A comprehensive IoT solution for smart bus stops using ESP32, featuring real-time sensor monitoring, image capture and analysis, and ML-powered insights.

## 📋 Table of Contents

- [Project Overview](#-project-overview)
- [Project Structure](#-project-structure)
- [Architecture](#️-architecture)
- [Hardware Setup](#-hardware-setup)
- [Quick Start](#-quick-start)
- [Installation Guide](#-installation-guide)
- [Deployment Guide](#-deployment-guide)
- [Configuration](#-configuration)
- [API Reference](#-api-reference)
- [MQTT Topics](#-mqtt-topics)
- [Service Management](#-service-management)
- [Testing](#-testing)
- [Troubleshooting](#-troubleshooting)
- [Security](#-security)
- [ML Models](#-ml-models)
- [Additional Resources](#-additional-resources)

## 🚀 Project Overview

NUS-SmartStop is an IoT project designed for smart bus stops that integrates:
- **ESP32 DOIT DevKit V1** microcontroller with cameras and sensors
- **ESP32-CAM** for image capture with API key authentication
- **Ultrasonic sensors** for distance/occupancy detection
- **Speakers** for audio notifications
- **MQTT** messaging protocol (Mosquitto) for real-time communication
- **Telegraf** for MQTT to InfluxDB data bridging
- **InfluxDB** time-series database for sensor data (local)
- **Flask** web server with dual upload modes and API key authentication
- **ML models** for image analysis and predictions

### Project Statistics
- **Python code**: ~950 lines
- **ESP32 firmware**: ~290 lines
- **Deployment**: Native Ubuntu 24.04 with systemd (no Docker)
- **Production Server**: 157.230.250.226 (DigitalOcean, 512MB RAM)

## 📁 Project Structure

```
NUS-SmartStop/
├── esp32/                      # ESP32 firmware code
│   └── smartstop_main.ino     # Main ESP32 sketch with API auth
├── esp32_cam/                  # ESP32-CAM firmware
│   ├── camera_pins.h          # Header Pins for ESP32CAM
│   └── CameraPhotoCapture.ino # ESP32CAM sketch with Flask image publishing
├── server/
│   ├── flask/                 # Flask image server
│   │   ├── image_server.py   # Flask application with dual upload modes
│   │   ├── esp32cam_image_server.py # Flask with auto image processing
│   │   ├── run_analysis.py   # Image comparison and analysis
│   │   ├── requirements.txt  # Python dependencies
│   │   └── .env.example      # Configuration template
│   ├── influxdb/             # InfluxDB setup scripts
│   │   └── setup.sh          # InfluxDB initialization
│   ├── mqtt/                 # MQTT configuration
│   │   └── mqtt_client.py    # MQTT debug client (optional)
│   └── systemd/              # Systemd service files
│       ├── flask-image-server.service
│       ├── telegraf.service
│       └── mosquitto-cs3237.conf
├── ml_models/                # ML model inference
│   └── inference.py          # Inference handler
├── telegraf.conf             # Telegraf MQTT→InfluxDB bridge config
├── requirements.txt          # Python dependencies
├── .env.example             # Environment variables template
├── start.sh                 # Start Flask server script
├── stop.sh                  # Stop Flask server script
└── README.md                # This file
```

## 🏗️ Architecture

### System Overview

```
┌─────────────────────────────────────────────────────────────────┐
│                         Smart Bus Stop                           │
│                                                                  │
│  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌──────────┐       │
│  │ Camera   │  │Ultrasonic│  │ Speaker  │  │  Other   │       │
│  │          │  │ Sensor   │  │          │  │ Sensors  │       │
│  └────┬─────┘  └────┬─────┘  └────┬─────┘  └────┬─────┘       │
│       │             │              │             │              │
│       └─────────────┴──────────────┴─────────────┘              │
│                         │                                        │
│                    ┌────▼─────┐                                 │
│                    │  ESP32   │                                 │
│                    │ DevKit   │                                 │
│                    └────┬─────┘                                 │
└─────────────────────────┼────────────────────────────────────────┘
                          │
                          │ WiFi
                          │
┌─────────────────────────▼────────────────────────────────────────┐
│                     Server Infrastructure                         │
│                    (Ubuntu 24.04 + systemd)                      │
│                                                                  │
│  ┌───────────────────────────────────────────────────────────┐ │
│  │                    MQTT Broker                             │ │
│  │                   (Mosquitto)                              │ │
│  │  Topics: nus-smartstop/sensors/*, camera/*, command/*     │ │
│  └─────┬────────────────────────────────────────────┬─────────┘ │
│        │                                             │           │
│  ┌─────▼─────────┐                           ┌──────▼────────┐  │
│  │   Telegraf    │                           │ Flask Server  │  │
│  │  MQTT → DB    │                           │  Image API    │  │
│  └─────┬─────────┘                           └───────────────┘  │
│        │                                                         │
│  ┌─────▼──────────────────────────────────────────────────┐    │
│  │              InfluxDB (Time Series DB)                 │    │
│  │              Bucket: sensor_data                       │    │
│  └────────────────────────────────────────────────────────┘    │
└───────────────────────────────────────────────────────────────────┘
```

### Data Flow

**Image Upload Flow:**
```
ESP32-CAM → HTTP POST (with API key) → Flask Server → Image Storage
                                          ↓
                                     SQLite Metadata
```

**Sensor Data Flow:**
```
ESP32 Sensors → MQTT → Mosquitto → Telegraf → InfluxDB (Local)
                                         ↓
                                   Time-series Data
```

### Key Components

#### ESP32 DOIT DevKit V1
- WiFi connectivity (2.4GHz)
- Sensor data collection (ultrasonic, temperature, etc.)
- MQTT publishing
- Speaker control

#### ESP32-CAM
- Image capture with OV2640 camera
- HTTP uploads with X-API-Key authentication
- Device ID tagging

#### MQTT Broker (Mosquitto)
- Message routing via systemd
- Topic structure: `nus-smartstop/*`
- Low latency messaging

#### Telegraf
- Bridges MQTT topics to InfluxDB
- Native MQTT consumer plugin
- InfluxDB v2 output plugin
- Managed via systemd

#### InfluxDB
- Time-series storage (local at 127.0.0.1:8086)
- Bucket: sensor_data
- 30-day retention policy
- Flux query language

#### Flask Server
- Dual-mode image server (multipart + raw body)
- API key authentication (X-API-Key header)
- Gallery UI at root endpoint
- SQLite metadata storage

**Deployment**: All services run natively on Ubuntu 24.04 using systemd (no Docker).

## 🛠️ Hardware Setup

### ESP32 DOIT DevKit V1 Specifications

- **Microcontroller**: ESP32-WROOM-32
- **Flash**: 4MB
- **RAM**: 520KB
- **WiFi**: 802.11 b/g/n (2.4GHz only)
- **Bluetooth**: v4.2 BR/EDR and BLE
- **GPIO Pins**: 30 pins
- **ADC**: 18 channels, 12-bit
- **DAC**: 2 channels, 8-bit
- **PWM**: 16 channels
- **UART**: 3 interfaces
- **I2C/SPI**: Supported

### ESP32 Pinout Reference

```
                     ESP32 DOIT DevKit V1
                    ┌──────────────────┐
                    │                  │
         EN  ┌──────┤ EN            D23├──────┐
      GPIO36 ├──────┤ VP            D22├──────┤ GPIO22
      GPIO39 ├──────┤ VN            TX0├──────┤ GPIO1
      GPIO34 ├──────┤ D34           RX0├──────┤ GPIO3
      GPIO35 ├──────┤ D35           D21├──────┤ GPIO21
      GPIO32 ├──────┤ D32           D19├──────┤ GPIO19
      GPIO33 ├──────┤ D33           D18├──────┤ GPIO18
      GPIO25 ├──────┤ D25            D5├──────┤ GPIO5
      GPIO26 ├──────┤ D26           D17├──────┤ GPIO17
      GPIO27 ├──────┤ D27           D16├──────┤ GPIO16
      GPIO14 ├──────┤ D14            D4├──────┤ GPIO4
      GPIO12 ├──────┤ D12            D0├──────┤ GPIO0
            GND ────┤ GND            D2├──────┤ GPIO2
      GPIO13 ├──────┤ D13           D15├──────┤ GPIO15
         9V  ├──────┤ 9V           CMD├──────┤
         EN  ├──────┤ CMD           3V3├──────┤ 3.3V
         3V3 ├──────┤ 3V3           GND├──────┤ GND
                    └──────────────────┘
```

### Pin Assignments for Smart Bus Stop

#### Ultrasonic Sensor (HC-SR04)
```
HC-SR04         ESP32 Pin
VCC      ──────> 5V
GND      ──────> GND
TRIG     ──────> GPIO 12 (D12)
ECHO     ──────> GPIO 14 (D14)
```

#### Speaker/Buzzer
```
Component       ESP32 Pin
Positive ──────> GPIO 13 (D13) [with 220Ω resistor]
Negative ──────> GND
```

#### ESP32-CAM Camera Pins (Standard Configuration)
The ESP32-CAM module has pre-wired camera connections. Refer to `esp32_cam/camera_pins.h` for specific pin mappings.

### Wiring Diagram

```
Ultrasonic Sensor:
┌──────────┐
│  HC-SR04 │
│          │
│ VCC  GND │
│ TRIG ECHO│
└──┬───┬─┬─┘
   │   │ │
   │   │ └──────> GPIO 14 (Echo)
   │   └────────> GPIO 12 (Trigger)
   └────────────> 5V & GND

Speaker:
┌──────────┐
│  Buzzer  │
│   (+)    │
└────┬─────┘
     │
    [R] 220Ω
     │
     └──────────> GPIO 13
     
     (-) ───────> GND
```

### Hardware Requirements

- **ESP32 DOIT DevKit V1** (primary controller)
- **ESP32-CAM Module** with OV2640 camera
- **HC-SR04 Ultrasonic Sensor** (or compatible)
- **Buzzer/Speaker** (3-5V)
- **220Ω Resistor** (for speaker)
- **Jumper Wires** (male-to-male, male-to-female)
- **Breadboard** (optional, for prototyping)
- **USB Cable** (Micro-USB for ESP32)
- **5V Power Supply** (2A recommended)

### Power Considerations

- **ESP32**: 3.3V logic, 5V power via USB
- **Ultrasonic Sensor**: 5V or 3.3V compatible
- **Speaker**: Check voltage rating, use resistor if needed
- **ESP32-CAM**: Requires stable 5V, 2A power supply recommended

**⚠️ Important**: Do not connect 5V directly to ESP32 GPIO pins (they are 3.3V tolerant).

## 🚀 Quick Start

### For Development (Local Testing)

```bash
# 1. Clone repository
git clone https://github.com/AY2526S1-CS3237-Team-10/NUS-SmartStop.git
cd NUS-SmartStop

# 2. Install Python dependencies
pip install -r server/flask/requirements.txt

# 3. Configure environment
cp server/flask/.env.example server/flask/.env
# Edit .env with your settings (especially API_KEY)

# 4. Run Flask server locally
python server/flask/image_server.py

# 5. Access gallery
# Open http://localhost:5000 in your browser
```

### For ESP32 Setup

1. **Install Arduino IDE** and ESP32 board support:
   - Open Arduino IDE
   - Go to File > Preferences
   - Add Board Manager URL:
     ```
     https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
     ```
   - Go to Tools > Board > Board Manager
   - Search for "esp32" and install

2. **Install Required Libraries**:
   - Tools > Manage Libraries
   - Install: PubSubClient, ArduinoJson, HTTPClient

3. **Configure WiFi and Server**:
   - Open `esp32/smartstop_main.ino`
   - Update:
     ```cpp
     const char* ssid = "YOUR_WIFI_SSID";
     const char* password = "YOUR_WIFI_PASSWORD";
     const char* mqtt_server = "YOUR_SERVER_IP";
     const char* flask_server = "http://YOUR_SERVER_IP:5000";
     const char* api_key = "CS3237-Group10-SecretKey";  // Must match server!
     ```

4. **Upload to ESP32**:
   - Connect ESP32 via USB
   - Select Board: Tools > Board > ESP32 Dev Module
   - Select Port: Tools > Port > (your port)
   - Click Upload

## 📦 Installation Guide

**Note**: This project does NOT use Docker. All services are installed and managed natively.

### Prerequisites

- Ubuntu 24.04 LTS (recommended) or compatible Linux distribution
- Python 3.8 or higher
- Git
- Arduino IDE 1.8+ (for ESP32 development)

### System Packages Installation

```bash
# Update system
sudo apt update && sudo apt upgrade -y

# Install required services
sudo apt install -y mosquitto mosquitto-clients telegraf influxdb2 python3-pip python3-venv
```

### Python Dependencies

```bash
# Clone repository
git clone https://github.com/AY2526S1-CS3237-Team-10/NUS-SmartStop.git
cd NUS-SmartStop

# Create virtual environment (recommended)
python3 -m venv venv
source venv/bin/activate

# Install dependencies
pip install -r server/flask/requirements.txt
```

### Configure Services

#### 1. Setup InfluxDB

```bash
# Start InfluxDB
sudo systemctl start influxdb
sudo systemctl enable influxdb

# Initial setup via CLI
influx setup \
  --username admin \
  --password <secure-password> \
  --org "NUS SmartStop" \
  --bucket sensor_data \
  --retention 30d \
  --force

# Generate API token for Telegraf
influx auth create --org "NUS SmartStop" --all-access
# Save this token - you'll need it for telegraf.conf
```

**Alternative**: Use the InfluxDB web UI at http://localhost:8086 for initial setup.

#### 2. Configure Mosquitto MQTT

```bash
# Copy configuration (if using custom config)
sudo cp server/systemd/mosquitto-cs3237.conf /etc/mosquitto/conf.d/cs3237.conf

# Start Mosquitto
sudo systemctl start mosquitto
sudo systemctl enable mosquitto

# Verify it's running
sudo systemctl status mosquitto

# Test MQTT
mosquitto_sub -h localhost -t "#" -v
```

#### 3. Configure Telegraf

```bash
# Copy configuration
sudo cp telegraf.conf /etc/telegraf/telegraf.conf

# IMPORTANT: Edit to add your InfluxDB token
sudo nano /etc/telegraf/telegraf.conf
# Update line: token = "YOUR_INFLUXDB_TOKEN_HERE"

# Start Telegraf
sudo systemctl start telegraf
sudo systemctl enable telegraf

# Verify it's running
sudo systemctl status telegraf
sudo journalctl -u telegraf -f
```

#### 4. Setup Environment Variables

```bash
# Copy example environment file
cp server/flask/.env.example server/flask/.env

# Edit .env file with your configuration
nano server/flask/.env
```

Key configurations:
- `API_KEY`: Set a secure API key (must match ESP32 code!)
- `UPLOAD_FOLDER`: Path for storing images
- `FLASK_HOST`: Set to `0.0.0.0` to accept connections from ESP32
- `INFLUXDB_TOKEN`: Token from InfluxDB setup

#### 5. Start Flask Server (Development Mode)

For development/testing:

```bash
# Start Flask server directly
python server/flask/image_server.py
```

For production, see the Deployment Guide below.

### Verification

```bash
# Test Flask Server
curl http://localhost:5000/health

# Test MQTT
mosquitto_sub -h localhost -t "#" -v

# Test InfluxDB
influx query 'from(bucket: "sensor_data") |> range(start: -1h) |> limit(n: 10)'
```

## 🚀 Deployment Guide

This guide explains production deployment on Ubuntu 24.04 using systemd services.

### Deployment Server Information

- **Server**: 157.230.250.226 (DigitalOcean Droplet)
- **OS**: Ubuntu 24.04 LTS
- **Services**: Mosquitto, Telegraf, InfluxDB, Flask (all via systemd)

### File Deployment Mapping

| Repository File | Deployment Location | Purpose |
|----------------|---------------------|---------|
| `server/flask/image_server.py` | `/root/cs3237_server/image_server.py` | Flask image upload server |
| `telegraf.conf` | `/etc/telegraf/telegraf.conf` | Telegraf MQTT→InfluxDB bridge config |
| `server/systemd/mosquitto-cs3237.conf` | `/etc/mosquitto/conf.d/cs3237.conf` | Mosquitto MQTT broker config |
| `server/systemd/flask-image-server.service` | `/etc/systemd/system/flask-image-server.service` | Flask systemd service |
| `server/systemd/telegraf.service` | `/etc/systemd/system/telegraf.service` | Telegraf systemd service |

### Step-by-Step Production Deployment

#### 1. Install Required Packages

```bash
# Update system
sudo apt update && sudo apt upgrade -y

# Install services
sudo apt install -y mosquitto mosquitto-clients telegraf influxdb2 python3-pip python3-venv
```

#### 2. Setup InfluxDB

```bash
# Start InfluxDB service
sudo systemctl start influxdb
sudo systemctl enable influxdb

# Initial setup
influx setup \
  --username admin \
  --password <secure-password> \
  --org "NUS SmartStop" \
  --bucket sensor_data \
  --retention 30d \
  --force

# Generate API token
influx auth create --org "NUS SmartStop" --all-access
# Save the token for telegraf.conf
```

#### 3. Deploy Mosquitto MQTT Broker

```bash
# Copy configuration
sudo cp server/systemd/mosquitto-cs3237.conf /etc/mosquitto/conf.d/cs3237.conf

# Test configuration
mosquitto -c /etc/mosquitto/mosquitto.conf -v

# Start and enable service
sudo systemctl restart mosquitto
sudo systemctl enable mosquitto

# Verify
sudo systemctl status mosquitto
mosquitto_sub -h localhost -t "#" -v
```

#### 4. Deploy Telegraf

```bash
# Copy configuration
sudo cp telegraf.conf /etc/telegraf/telegraf.conf

# IMPORTANT: Edit to add InfluxDB token
sudo nano /etc/telegraf/telegraf.conf
# Update: token = "YOUR_INFLUXDB_TOKEN_HERE"

# Copy systemd service
sudo cp server/systemd/telegraf.service /etc/systemd/system/telegraf.service

# Start and enable
sudo systemctl daemon-reload
sudo systemctl start telegraf
sudo systemctl enable telegraf

# Verify
sudo systemctl status telegraf
sudo journalctl -u telegraf -f
```

#### 5. Deploy Flask Image Server

```bash
# Create deployment directory
sudo mkdir -p /root/cs3237_server/uploads

# Copy Flask application
sudo cp server/flask/image_server.py /root/cs3237_server/image_server.py

# Install Python dependencies
cd /root/cs3237_server
sudo python3 -m venv venv
source venv/bin/activate
pip install flask flask-cors python-dotenv werkzeug pillow

# Create environment file
cat > /root/cs3237_server/.env << 'EOF'
API_KEY=CS3237-Group10-SecretKey
UPLOAD_FOLDER=/root/cs3237_server/uploads
FLASK_HOST=0.0.0.0
FLASK_PORT=5000
MAX_CONTENT_LENGTH=16777216
EOF

# Copy systemd service
sudo cp server/systemd/flask-image-server.service /etc/systemd/system/flask-image-server.service

# Edit service to use venv Python
sudo nano /etc/systemd/system/flask-image-server.service
# Set: ExecStart=/root/cs3237_server/venv/bin/python3 /root/cs3237_server/image_server.py

# Start and enable
sudo systemctl daemon-reload
sudo systemctl start flask-image-server
sudo systemctl enable flask-image-server

# Verify
sudo systemctl status flask-image-server
curl http://localhost:5000/health
```

#### 6. Configure Firewall

```bash
# Allow required ports
sudo ufw allow 1883/tcp  # MQTT
sudo ufw allow 5000/tcp  # Flask
sudo ufw allow 8086/tcp  # InfluxDB (if accessing remotely)
sudo ufw enable
```

### Testing the Deployment

#### Test MQTT

```bash
# Subscribe to all topics
mosquitto_sub -h 127.0.0.1 -t "#" -v

# In another terminal, publish test message
mosquitto_pub -h 127.0.0.1 \
  -t "nus-smartstop/sensors/ultrasonic" \
  -m '{"deviceId":"test_device","location":"test","distance":100,"timestamp":1234567890}'
```

#### Test Flask Server

```bash
# Health check
curl http://127.0.0.1:5000/health

# Test image upload (with API key)
curl -X POST \
  -H "X-API-Key: CS3237-Group10-SecretKey" \
  -F "image=@test_image.jpg" \
  -F "device_id=esp32_001" \
  http://127.0.0.1:5000/upload

# Test without API key (should fail with 401)
curl -X POST -F "image=@test.jpg" http://127.0.0.1:5000/upload
```

#### Test Telegraf → InfluxDB Pipeline

```bash
# Publish test sensor data
mosquitto_pub -h 127.0.0.1 \
  -t "nus-smartstop/sensors/ultrasonic" \
  -m '{"deviceId":"esp32_001","location":"bus_stop_01","distance":125.5,"timestamp":1698765432}'

# Check Telegraf logs
sudo journalctl -u telegraf -n 50

# Query InfluxDB
influx query 'from(bucket: "sensor_data") |> range(start: -1h) |> limit(n: 10)'
```

### Updating the Deployment

```bash
# Update Flask application
sudo cp server/flask/image_server.py /root/cs3237_server/image_server.py
sudo systemctl restart flask-image-server

# Update Telegraf config
sudo cp telegraf.conf /etc/telegraf/telegraf.conf
# Re-add InfluxDB token!
sudo systemctl restart telegraf

# Update Mosquitto config
sudo cp server/systemd/mosquitto-cs3237.conf /etc/mosquitto/conf.d/cs3237.conf
sudo systemctl restart mosquitto
```

### Backup and Recovery

#### Backup InfluxDB

```bash
# Backup all data
influx backup /root/backups/influxdb-$(date +%Y%m%d)

# Backup specific bucket
influx backup --bucket sensor_data /root/backups/sensor_data-$(date +%Y%m%d)
```

#### Restore InfluxDB

```bash
influx restore /root/backups/influxdb-20231025
```

#### Backup Configuration Files

```bash
# Create backup directory
mkdir -p /root/backups/configs

# Backup configs
sudo cp /etc/telegraf/telegraf.conf /root/backups/configs/
sudo cp /etc/mosquitto/conf.d/cs3237.conf /root/backups/configs/
sudo cp /root/cs3237_server/.env /root/backups/configs/
```

## 🔄 Service Management

### Check Service Status

```bash
# Check individual services
sudo systemctl status mosquitto
sudo systemctl status telegraf
sudo systemctl status influxdb
sudo systemctl status flask-image-server

# Check all at once
sudo systemctl status mosquitto telegraf influxdb flask-image-server
```

### Restart Services

```bash
# Restart individual service
sudo systemctl restart flask-image-server

# Restart all services
sudo systemctl restart mosquitto telegraf influxdb flask-image-server
```

### View Service Logs

```bash
# Follow logs in real-time
sudo journalctl -u flask-image-server -f
sudo journalctl -u telegraf -f
sudo journalctl -u mosquitto -f

# View last 100 lines
sudo journalctl -u flask-image-server -n 100

# View logs since 1 hour ago
sudo journalctl -u telegraf --since "1 hour ago"

# View all errors
sudo journalctl -u flask-image-server --since "1 hour ago" | grep -i error
```

### Stop/Start Services

```bash
# Stop service
sudo systemctl stop flask-image-server

# Start service
sudo systemctl start flask-image-server

# Enable service to start on boot
sudo systemctl enable flask-image-server

# Disable service from starting on boot
sudo systemctl disable flask-image-server
```

### Update Configuration

```bash
# After updating configuration files
sudo systemctl daemon-reload
sudo systemctl restart <service-name>
```

### Monitor System Resources

```bash
# Check CPU/Memory usage
htop

# Check disk space
df -h

# Check service resource usage
systemctl status mosquitto telegraf influxdb flask-image-server
```

## 🔧 Configuration

### Environment Variables (.env)

Located at `server/flask/.env` (development) or `/root/cs3237_server/.env` (production):

```bash
# Flask Server Configuration
FLASK_HOST=0.0.0.0
FLASK_PORT=5000
FLASK_DEBUG=False
UPLOAD_FOLDER=/root/cs3237_server/uploads
MAX_CONTENT_LENGTH=16777216  # 16MB max upload

# Flask API Authentication (REQUIRED!)
API_KEY=CS3237-Group10-SecretKey

# InfluxDB Configuration (Local)
INFLUXDB_URL=http://127.0.0.1:8086
INFLUXDB_TOKEN=your-influxdb-token
INFLUXDB_ORG=NUS SmartStop
INFLUXDB_BUCKET=sensor_data

# MQTT Configuration
MQTT_BROKER=127.0.0.1
MQTT_PORT=1883
MQTT_TOPIC_PREFIX=nus-smartstop

# ML Model Configuration
MODEL_PATH=./ml_models/
CONFIDENCE_THRESHOLD=0.5
```

**Important**: 
- The API key in `.env` must match the key in your ESP32 code!
- Change default API keys in production
- Never commit `.env` files with real credentials to git

### Telegraf Configuration

Key sections in `telegraf.conf`:

```toml
# MQTT Consumer Input
[[inputs.mqtt_consumer]]
  servers = ["tcp://127.0.0.1:1883"]
  topics = ["nus-smartstop/sensors/#"]
  data_format = "json"
  json_string_fields = ["deviceId", "location"]

# InfluxDB Output
[[outputs.influxdb_v2]]
  urls = ["http://127.0.0.1:8086"]
  token = "YOUR_INFLUXDB_TOKEN_HERE"  # UPDATE THIS!
  organization = "NUS SmartStop"
  bucket = "sensor_data"
```

### Mosquitto Configuration

Located at `/etc/mosquitto/conf.d/cs3237.conf`:

```
listener 1883
allow_anonymous true
```

For production, consider enabling authentication:

```
listener 1883
allow_anonymous false
password_file /etc/mosquitto/passwd
```

## 📡 API Reference

### Flask Server API

Base URL: `http://localhost:5000` (development) or `http://157.230.250.226:5000` (production)

#### Authentication

All upload endpoints require `X-API-Key` header:

```bash
X-API-Key: CS3237-Group10-SecretKey
```

### Endpoints

#### 1. Health Check

Check if the server is running.

**Endpoint:** `GET /health`

**Response:**
```json
{
  "status": "running",
  "timestamp": "2025-10-26T17:21:29Z",
  "images_stored": 42,
  "disk_free_gb": 8.5
}
```

**Example:**
```bash
curl http://localhost:5000/health
```

---

#### 2. Upload Image (Raw Body - ESP32-CAM)

Upload image as raw bytes (optimized for ESP32-CAM).

**Endpoint:** `POST /upload`

**Headers:**
- `X-API-Key`: CS3237-Group10-SecretKey (required)
- `Device-ID`: ESP32_001 (optional)
- `Content-Type`: image/jpeg

**Body:** Raw image bytes

**Response:**
```json
{
  "success": true,
  "filename": "ESP32_001_20251026_172129.jpg",
  "size": 82540,
  "url": "/images/ESP32_001_20251026_172129.jpg"
}
```

**Example:**
```bash
curl -X POST \
  -H "X-API-Key: CS3237-Group10-SecretKey" \
  -H "Device-ID: ESP32_001" \
  -H "Content-Type: image/jpeg" \
  --data-binary "@photo.jpg" \
  http://localhost:5000/upload
```

---

#### 3. Upload Image (Multipart - Web/Mobile)

Upload image via multipart form data.

**Endpoint:** `POST /upload`

**Headers:**
- `X-API-Key`: CS3237-Group10-SecretKey (required)

**Content-Type:** `multipart/form-data`

**Parameters:**
- `image` (required): Image file
- `device_id` (optional): Device identifier

**Response:** Same as raw body upload

**Example:**
```bash
curl -X POST \
  -H "X-API-Key: CS3237-Group10-SecretKey" \
  -F "image=@photo.jpg" \
  -F "device_id=esp32_001" \
  http://localhost:5000/upload
```

---

#### 4. List Images

Get list of uploaded images with metadata.

**Endpoint:** `GET /images?limit=50&offset=0`

**Query Parameters:**
- `limit`: Number of images to return (default: 50)
- `offset`: Number of images to skip (default: 0)

**Response:**
```json
{
  "count": 100,
  "limit": 50,
  "offset": 0,
  "images": [
    {
      "filename": "ESP32_001_20251026_172129.jpg",
      "size": 82540,
      "url": "/images/ESP32_001_20251026_172129.jpg",
      "timestamp": "2025-10-26T17:21:29Z",
      "device_id": "ESP32_001"
    }
  ]
}
```

**Example:**
```bash
curl http://localhost:5000/images?limit=10
```

---

#### 5. Get Image

Retrieve a specific image file.

**Endpoint:** `GET /images/<filename>`

**Response:** Image file (JPEG)

**Example:**
```bash
curl http://localhost:5000/images/ESP32_001_20251026_172129.jpg -o image.jpg
```

---

#### 6. Web Gallery

View images in a web browser interface.

**Endpoint:** `GET /`

**Response:** HTML gallery interface

**Features:**
- Grid layout with thumbnails
- Latest 30 images
- Click to view full image
- Shows filename and metadata
- Responsive design

**Access:** Open `http://localhost:5000/` in your browser

---

### Error Responses

#### 401 Unauthorized
```json
{
  "error": "Unauthorized",
  "message": "Missing or invalid API key"
}
```

#### 400 Bad Request
```json
{
  "error": "Bad Request",
  "message": "No image file provided"
}
```

#### 413 Payload Too Large
```json
{
  "error": "File too large",
  "message": "Maximum file size is 16MB"
}
```

## 📊 MQTT Topics

All topics use the `nus-smartstop/` prefix.

### Topic Structure

```
nus-smartstop/
├── sensors/
│   ├── ultrasonic
│   ├── temperature
│   └── humidity
├── camera/
└── command/{device_id}
```

### Sensor Data Topics

**Topic Pattern:** `nus-smartstop/sensors/{sensor_type}`

**Payload Format:**
```json
{
  "deviceId": "esp32_001",
  "location": "bus_stop_01",
  "distance": 123.45,
  "timestamp": 1698765432
}
```

**Examples:**
```bash
# Ultrasonic sensor data
mosquitto_pub -h localhost \
  -t "nus-smartstop/sensors/ultrasonic" \
  -m '{"deviceId":"esp32_001","location":"bus_stop_01","distance":125.5,"timestamp":1698765432}'

# Temperature sensor data
mosquitto_pub -h localhost \
  -t "nus-smartstop/sensors/temperature" \
  -m '{"deviceId":"esp32_001","location":"bus_stop_01","temperature":28.5,"timestamp":1698765432}'
```

**Important:** Use `deviceId` (not `device_id`) as this is the tag key configured in Telegraf.

### Camera Events

**Topic:** `nus-smartstop/camera`

**Payload Format:**
```json
{
  "deviceId": "esp32_001",
  "location": "bus_stop_01",
  "event_type": "capture",
  "image_count": 1,
  "timestamp": 1698765432
}
```

### Command Topics (Subscribe on ESP32)

**Topic Pattern:** `nus-smartstop/command/{device_id}`

**Payload:** String commands

**Examples:**
```bash
# Send beep command to device
mosquitto_pub -h localhost \
  -t "nus-smartstop/command/esp32_001" \
  -m "BEEP"

# Send capture command
mosquitto_pub -h localhost \
  -t "nus-smartstop/command/esp32_001" \
  -m "CAPTURE"
```

### Subscribe to All Topics

```bash
# Monitor all messages
mosquitto_sub -h localhost -t "nus-smartstop/#" -v

# Monitor only sensor data
mosquitto_sub -h localhost -t "nus-smartstop/sensors/#" -v
```

## 🧪 Testing

### Test Flask Server

```bash
# Health check
curl http://localhost:5000/health

# Test image upload with API key (should succeed)
curl -X POST \
  -H "X-API-Key: CS3237-Group10-SecretKey" \
  -H "Device-ID: test_device" \
  --data-binary "@test.jpg" \
  http://localhost:5000/upload

# Test without API key (should fail with 401)
curl -X POST --data-binary "@test.jpg" http://localhost:5000/upload

# List images
curl http://localhost:5000/images
```

### Test MQTT

```bash
# Subscribe to all topics
mosquitto_sub -h localhost -t "nus-smartstop/#" -v

# In another terminal, publish test message
mosquitto_pub -h localhost \
  -t "nus-smartstop/sensors/ultrasonic" \
  -m '{"deviceId":"test1","location":"test","distance":100,"timestamp":1698765432}'
```

### Test Telegraf → InfluxDB Pipeline

```bash
# 1. Publish MQTT message
mosquitto_pub -h localhost \
  -t "nus-smartstop/sensors/ultrasonic" \
  -m '{"deviceId":"esp32_001","location":"bus_stop_01","distance":125.5,"timestamp":1698765432}'

# 2. Check Telegraf logs
sudo journalctl -u telegraf -n 50

# 3. Query InfluxDB
influx query 'from(bucket: "sensor_data") |> range(start: -1h) |> limit(n: 10)'
```

### Test ESP32 Connection

1. Open Serial Monitor in Arduino IDE
2. Verify connection messages:
   - WiFi connected
   - MQTT connected
   - Camera initialized (if using ESP32-CAM)
3. Check sensor readings appear in serial output
4. Verify MQTT messages published to broker
5. Confirm images uploaded to Flask server

### End-to-End Test

```bash
# 1. Start all services
sudo systemctl start mosquitto telegraf influxdb flask-image-server

# 2. ESP32 should publish sensor data
# Monitor with: mosquitto_sub -h localhost -t "#" -v

# 3. Check data in InfluxDB
influx query 'from(bucket: "sensor_data") |> range(start: -1h)'

# 4. Upload test image
curl -X POST \
  -H "X-API-Key: CS3237-Group10-SecretKey" \
  -F "image=@test.jpg" \
  http://localhost:5000/upload

# 5. Verify image in uploads folder
ls -lh /root/cs3237_server/uploads/
```

## 🐛 Troubleshooting

### ESP32 Issues

#### ESP32 Won't Connect to WiFi
- Verify SSID and password are correct
- Ensure WiFi is 2.4GHz (ESP32 doesn't support 5GHz)
- Check for special characters in credentials
- Verify WiFi signal strength

#### Can't Upload to ESP32
- Check correct board selected (ESP32 Dev Module)
- Check correct port selected
- Try pressing BOOT button while uploading
- Verify USB cable supports data (not just charging)

#### Camera Initialization Failed
- Check power supply (camera requires stable 5V)
- Verify pin configuration matches your hardware
- Try reducing image quality/size
- Check camera module is properly seated

### Server Issues

#### Flask Upload Fails with 401 Unauthorized
- **Cause**: Missing or incorrect API key
- **Solution**: Verify `X-API-Key` header matches `.env` API_KEY

#### MQTT Connection Failed
- Verify Mosquitto is running: `sudo systemctl status mosquitto`
- Check broker logs: `sudo journalctl -u mosquitto -f`
- Test with mosquitto_sub: `mosquitto_sub -h localhost -t "#"`
- Check firewall isn't blocking port 1883

#### InfluxDB Connection Failed
- Verify InfluxDB is running: `sudo systemctl status influxdb`
- Check token in telegraf.conf
- Access InfluxDB UI: http://127.0.0.1:8086
- Verify bucket and organization exist

#### Telegraf Not Writing Data
- Check Telegraf logs: `sudo journalctl -u telegraf -f`
- Verify MQTT messages published: `mosquitto_sub -h localhost -t "#" -v`
- Test config: `telegraf --config /etc/telegraf/telegraf.conf --test`
- Verify InfluxDB token and bucket name
- Check JSON format matches expected schema

#### No Data in InfluxDB
1. Check MQTT messages are being published
2. Verify Telegraf is receiving messages (check logs)
3. Test Telegraf config syntax
4. Verify InfluxDB token and connection
5. Check for JSON parsing errors in Telegraf logs

#### Flask Server Won't Start
- Check Flask logs: `sudo journalctl -u flask-image-server -f`
- Verify port 5000 is not in use: `sudo lsof -i :5000`
- Check file permissions on uploads folder
- Verify Python dependencies installed

### Service Management Issues

#### Service Won't Start
```bash
# Check service status and logs
sudo systemctl status <service-name>
sudo journalctl -u <service-name> -n 100

# Check configuration syntax
# For Telegraf:
telegraf --config /etc/telegraf/telegraf.conf --test

# For Mosquitto:
mosquitto -c /etc/mosquitto/mosquitto.conf -v
```

#### Service Crashes on Boot
- Check logs: `sudo journalctl -u <service-name> --since boot`
- Verify dependencies are started first
- Check systemd service file configuration

### Network Issues

#### ESP32 Can't Reach Server
- Verify server IP address is correct
- Check firewall rules: `sudo ufw status`
- Test network connectivity: `ping <server-ip>`
- Verify services are listening: `sudo netstat -tlnp`

#### High Latency
- Check network bandwidth
- Reduce image quality/size
- Optimize MQTT message frequency
- Consider local caching

## 🔒 Security

### Flask Image Server

#### API Key Authentication
- All uploads require `X-API-Key` header
- Change default API key in production
- Store API key in environment variables
- Never commit API keys to git

#### Image Validation
- Validates uploaded files are actual images using Pillow
- Checks file extensions (png, jpg, jpeg, gif only)
- Maximum file size: 16MB (configurable)

#### Secure Filenames
- Sanitizes filenames using `secure_filename()`
- Device ID sanitization prevents injection attacks
- Timestamped filenames prevent collisions

#### CORS Configuration
- Enabled for web dashboard access
- Configure allowed origins in production

### Infrastructure Security

#### MQTT Broker
- Currently allows anonymous connections
- For production, enable authentication:
  ```bash
  # Create password file
  sudo mosquitto_passwd -c /etc/mosquitto/passwd username
  
  # Edit /etc/mosquitto/conf.d/cs3237.conf
  allow_anonymous false
  password_file /etc/mosquitto/passwd
  ```

#### InfluxDB
- Runs locally (127.0.0.1:8086), not exposed externally
- Token-based authentication
- Access from external tools via SSH tunnel if needed

#### Systemd Services
- Services run with appropriate permissions
- Environment variables for secrets management
- Logs accessible only to authorized users

### Production Recommendations

1. **Change Default Credentials**
   - Update InfluxDB admin password
   - Change Flask API key
   - Enable MQTT authentication

2. **Use HTTPS**
   - Configure nginx as reverse proxy
   - Install SSL certificates with Let's Encrypt
   - Redirect HTTP to HTTPS

3. **Implement Rate Limiting**
   - Limit upload requests per IP
   - Prevent abuse and DoS attacks

4. **Regular Updates**
   - Keep all services up to date
   - Apply security patches promptly
   - Monitor security advisories

5. **Firewall Configuration**
   - Only expose necessary ports
   - Use ufw or iptables
   - Consider fail2ban for SSH

6. **Monitoring and Alerting**
   - Monitor service health
   - Set up alerts for failures
   - Log analysis for suspicious activity

## 🤖 ML Models

The project supports integration with various ML models for image analysis.

### Supported Use Cases

1. **Object Detection**: Detect buses, people, vehicles
2. **Crowd Analysis**: Estimate number of people waiting
3. **Scene Classification**: Classify bus stop conditions
4. **Anomaly Detection**: Detect unusual activities

### Model Integration

Place your trained model in `ml_models/` directory and update `ml_models/inference.py`:

```python
from ml_models.inference import MLInferenceHandler

# Initialize handler
handler = MLInferenceHandler()

# Load your model
handler.load_model('your_model.pth')

# Run inference
results = handler.predict(image_path)
```

### Supported Frameworks

- **PyTorch**: via torch and torchvision
- **TensorFlow**: via tensorflow
- **ONNX**: Cross-framework model format

### Adding Custom Models

1. Place model file in `ml_models/`
2. Update `inference.py` with model loading logic
3. Implement preprocessing pipeline
4. Define inference function
5. Configure Flask endpoint if needed

## 📊 Data Visualization

### InfluxDB UI

Built-in visualization at http://127.0.0.1:8086 (access via SSH tunnel):

```bash
# From local machine, create SSH tunnel
ssh -L 8086:localhost:8086 user@157.230.250.226

# Then open http://localhost:8086 in browser
```

### Grafana Integration

1. Install Grafana
2. Add InfluxDB as datasource
3. Configure connection:
   - URL: http://127.0.0.1:8086
   - Organization: NUS SmartStop
   - Token: (your InfluxDB token)
   - Bucket: sensor_data

### Custom Dashboards

Query InfluxDB via Flux language:

```flux
from(bucket: "sensor_data")
  |> range(start: -24h)
  |> filter(fn: (r) => r._measurement == "ultrasonic")
  |> filter(fn: (r) => r.deviceId == "esp32_001")
```

## 📚 Additional Resources

### Documentation

- [ESP32 Documentation](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/)
- [Arduino ESP32 Core](https://github.com/espressif/arduino-esp32)
- [InfluxDB Documentation](https://docs.influxdata.com/influxdb/v2.7/)
- [Telegraf Documentation](https://docs.influxdata.com/telegraf/)
- [MQTT Protocol](https://mqtt.org/)
- [Flask Documentation](https://flask.palletsprojects.com/)
- [Mosquitto MQTT Broker](https://mosquitto.org/)

### Libraries

- [PubSubClient](https://github.com/knolleary/pubsubclient) - MQTT client for Arduino
- [ArduinoJson](https://arduinojson.org/) - JSON library for Arduino
- [ESP32-CAM Library](https://github.com/espressif/esp32-camera) - Camera support

### Tools

- [Arduino IDE](https://www.arduino.cc/en/software)
- [PlatformIO](https://platformio.org/) - Alternative to Arduino IDE
- [MQTT Explorer](http://mqtt-explorer.com/) - MQTT debugging tool
- [Postman](https://www.postman.com/) - API testing tool

## 👥 Team

**CS3237 Team 10 - AY2526S1**  
National University of Singapore  
School of Computing

## 📄 License

This project is for educational purposes as part of CS3237 coursework.

## 🤝 Contributing

1. Fork the repository
2. Create a feature branch (`git checkout -b feature/amazing-feature`)
3. Commit your changes (`git commit -m 'Add amazing feature'`)
4. Push to the branch (`git push origin feature/amazing-feature`)
5. Create a Pull Request

## 📞 Support

For issues and questions:
- Open an issue on GitHub
- Review troubleshooting section above
- Check service logs for errors

---

**Note**: This is an educational project for CS3237 (Introduction to Internet of Things). Always follow safety guidelines when working with electronics and IoT devices.

**Production Server**: 157.230.250.226 (DigitalOcean, Ubuntu 24.04, 512MB RAM)
