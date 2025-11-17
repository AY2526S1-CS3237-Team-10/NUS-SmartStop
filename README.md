# NUS-SmartStop

A comprehensive IoT solution for smart bus stops using ESP32, featuring real-time sensor monitoring, image capture and analysis, and ML-powered insights.

Note: this README was generated with assistance from Copilot. There may be inaccuracies. 

## 📋 Table of Contents

- [Project Overview](#-project-overview)
- [Project Structure](#-project-structure)
- [Architecture](#️-architecture)
- [Quick Start](#-quick-start)
- [Installation Guide](#-installation-guide)
- [Deployment Guide](#-deployment-guide)
- [Configuration](#-configuration)
- [API Reference](#-api-reference)
- [MQTT Topics](#-mqtt-topics)
- [Service Management](#-service-management)
- [Security](#-security)
- [ML Models](#-ml-models)

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

## 📁 Project Structure

```
NUS-SmartStop/
├── esp32/                      # ESP32 firmware code
│   ├── esp32_cam/             # ESP32-CAM firmware
│   │   └── CameraPhotoCapture.ino # Image capture and Flask upload
│   ├── ultrasonic_sensors/    # Ultrasonic sensor code
│   └── ESP32_Actuator/        # Actuator control code
├── server/
│   ├── flask/                # Flask image server
│   │   ├── Deployment/       # Flask Predictor Deployment
│   │   │   ├── busstop_xgboost.json       # Json File for XGBoost Model Parameters
│   │   │   ├── busstop_xgboost_scaler.pkl       # Pickle File for XGBoost Model refence
│   │   │   ├── background_predictor.py       # Background Script for prediction running
│   │   │   ├── predict.py       # Simple inference script for capcity prediction
│   │   │   ├── query_influxdb.py  # InfluxDB data extraction
│   │   │   └── run_inference.py # Activation code for Data extraction, cleaning and prediction
│   │   ├── image_server.py   # Flask application with dual upload modes
│   │   └── esp32cam_image_server.py # Flask with auto image processing
│   ├── influxdb/             # InfluxDB client
│   │   └── client.py         # InfluxDB client utilities
│   ├── mqtt/                 # MQTT configuration
│   │   └── mqtt_client.py    # MQTT client utilities
│   └── systemd/              # Systemd service files
│       ├── flask-image-server.service
│       ├── cs3237-predictor.service
│       ├── telegraf.service
│       └── mosquitto-cs3237.conf
├── ml_models/                # ML models
│   ├── XGBoost_Training/                # XGBoost Training Model
│   │   ├── prepare_data_xgboost.py     # Cleaning InfluxDB for training
│   │   ├── query_influxdb.py     # InfluxDB data extraction
│   │   └── train_xgboost.py       # XGBoost model training
│   ├── Crowd Classification CAM.ipynb  # CNN training notebook
│   └── Test classification.ipynb       # XGBoost model notebook
├── telegraf.conf             # Telegraf MQTT→InfluxDB bridge config
├── requirements.txt          # Python dependencies
├── .env.example             # Environment variables template
├── start.sh                 # Start Flask server script
└── stop.sh                  # Stop Flask server script
```

### Data Flow

**Image Processing Flow:**
```
ESP32-CAM → HTTP POST → Flask Server → ML Inference (CNN/XGBoost)
                                              ↓
                                         MQTT Publish
                                              ↓
                                      Mosquitto Broker
```

**Sensor Data Flow:**
```
ESP32 Sensors → MQTT → Mosquitto → Telegraf → InfluxDB (Local)
```

**Predictor Data Flow:**
```
InfluxDB → Flask Server → ESP32 Sensors
                ↓ ↑
       ML Inference (XGBoost)
```

### Key Components

#### ESP32 Devices
- WiFi connectivity (2.4GHz)
- Sensor data collection (ultrasonic, etc.)
- MQTT publishing
- Actuator control (speaker, servo)

#### ESP32-CAM
- Image capture with OV2640 camera
- HTTP uploads to Flask server
- Device ID tagging

#### Flask Server
- Receives images from ESP32-CAM
- Runs ML inference (CNN and XGBoost models)
- Publishes results to MQTT
- Gallery UI at root endpoint

#### MQTT Broker (Mosquitto)
- Message routing via systemd
- Topic structure: `nus-smartstop/*`
- Receives sensor data and inference results

#### Telegraf
- Bridges MQTT topics to InfluxDB
- Native MQTT consumer plugin
- InfluxDB v2 output plugin

#### InfluxDB
- Time-series storage (local at 157.230.250.226:8086)
- Bucket: sensor_data
- 30-day retention policy

**Deployment**: All services run natively on Ubuntu 24.04 using systemd.

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
   - Open your ESP32 sketch file
   - Update WiFi credentials and server settings:
     ```cpp
     const char* ssid = "YOUR_WIFI_SSID";
     const char* password = "YOUR_WIFI_PASSWORD";
     const char* mqtt_server = "YOUR_SERVER_IP";
     const char* flask_server = "http://YOUR_SERVER_IP:5000";
     ```

4. **Upload to ESP32**:
   - Connect ESP32 via USB
   - Select Board: Tools > Board > ESP32 Dev Module
   - Select Port: Tools > Port > (your port)
   - Click Upload

## 📦 Installation Guide

### Prerequisites

- Ubuntu 24.04 LTS or compatible Linux distribution
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
```

#### 2. Configure Mosquitto MQTT

```bash
# Start Mosquitto
sudo systemctl start mosquitto
sudo systemctl enable mosquitto

# Verify it's running
sudo systemctl status mosquitto
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
```

#### 4. Setup Environment Variables

```bash
# Copy example environment file
cp server/flask/.env.example server/flask/.env

# Edit .env file with your configuration
nano server/flask/.env
```

Key configurations:
- `API_KEY`: Set a secure API key
- `UPLOAD_FOLDER`: Path for storing images
- `FLASK_HOST`: Set to `0.0.0.0` to accept connections from ESP32

#### 5. Start Flask Server

For development/testing:

```bash
# Start Flask server directly
python server/flask/image_server.py
```

For production, see the Deployment Guide below.

## 🚀 Deployment Guide

### Production Deployment on Ubuntu 24.04

#### Deploy Flask Image Server

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
EOF

# Copy and start systemd service
sudo cp server/systemd/flask-image-server.service /etc/systemd/system/
sudo systemctl daemon-reload
sudo systemctl start flask-image-server
sudo systemctl enable flask-image-server
```

#### Configure Firewall

```bash
# Allow required ports
sudo ufw allow 1883/tcp  # MQTT
sudo ufw allow 5000/tcp  # Flask
sudo ufw enable
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
```

## 🔄 Service Management

### Check Service Status

```bash
# Check individual services
sudo systemctl status mosquitto
sudo systemctl status telegraf
sudo systemctl status influxdb
sudo systemctl status flask-image-server
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

# View last 100 lines
sudo journalctl -u flask-image-server -n 100
```

### Stop/Start Services

```bash
# Stop service
sudo systemctl stop flask-image-server

# Start service
sudo systemctl start flask-image-server

# Enable service to start on boot
sudo systemctl enable flask-image-server
```

## 🔧 Configuration

### Environment Variables (.env)

Located at `server/flask/.env`:

```bash
# Flask Server Configuration
FLASK_HOST=0.0.0.0
FLASK_PORT=5000
UPLOAD_FOLDER=/root/cs3237_server/uploads
MAX_CONTENT_LENGTH=16777216  # 16MB max upload

# Flask API Authentication
API_KEY=CS3237-Group10-SecretKey

# InfluxDB Configuration (Local)
INFLUXDB_URL=http://127.0.0.1:8086
INFLUXDB_TOKEN=your-influxdb-token
INFLUXDB_ORG=NUS SmartStop
INFLUXDB_BUCKET=sensor_data

# MQTT Configuration
MQTT_BROKER=127.0.0.1
MQTT_PORT=1883
```

**Important**: Never commit `.env` files with real credentials to git.

### Telegraf Configuration

Key sections in `telegraf.conf`:

```toml
# MQTT Consumer Input
[[inputs.mqtt_consumer]]
  servers = ["tcp://157.230.250.226:1883"]
  topics = ["nus-smartstop/sensors/#"]
  data_format = "json"

# InfluxDB Output
[[outputs.influxdb_v2]]
  urls = ["http://157.230.250.226:8086"]
  token = "YOUR_INFLUXDB_TOKEN_HERE"  # UPDATE THIS!
  organization = "NUS SmartStop"
  bucket = "sensor_data"
```

## 📡 API Reference

### Flask Server API

Base URL: `http://localhost:5000` (development) or `http://157.230.250.226:5000` (production)

#### Authentication

Upload endpoints require `X-API-Key` header.

### Key Endpoints

#### Health Check
```bash
GET /health

# Example:
curl http://localhost:5000/health
```

#### Upload Image (Raw Body - ESP32-CAM)
```bash
POST /upload
Headers: 
  X-API-Key: CS3237-Group10-SecretKey
  Device-ID: ESP32_001
  Content-Type: image/jpeg
Body: Raw image bytes

# Example:
curl -X POST \
  -H "X-API-Key: CS3237-Group10-SecretKey" \
  -H "Device-ID: ESP32_001" \
  -H "Content-Type: image/jpeg" \
  --data-binary "@photo.jpg" \
  http://157.230.250.226:5000/upload
```

#### Upload Image (Multipart)
```bash
POST /upload
Headers: X-API-Key: CS3237-Group10-SecretKey
Content-Type: multipart/form-data
Parameters:
  - image: Image file
  - device_id: Device identifier

# Example:
curl -X POST \
  -H "X-API-Key: CS3237-Group10-SecretKey" \
  -F "image=@photo.jpg" \
  -F "device_id=esp32_001" \
  http://157.230.250.226:5000/upload
```

#### List Images
```bash
GET /images?limit=50&offset=0

# Example:
curl http://157.230.250.226:5000/images?limit=10
```

#### Web Gallery
Open `http://157.230.250.226:5000/` in your browser to view uploaded images.

## 📊 MQTT Topics

All topics use the `nus-smartstop/` prefix.

### Topic Structure

```
nus-smartstop/
├── sensors/
│   ├── ultrasonic
│   ├── temperature
│   └── humidity
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

**Example:**
```bash
# Publish sensor data
mosquitto_pub -h localhost \
  -t "nus-smartstop/sensors/ultrasonic" \
  -m '{"deviceId":"esp32_001","location":"bus_stop_01","distance":125.5,"timestamp":1698765432}'

# Subscribe to all topics
mosquitto_sub -h localhost -t "nus-smartstop/#" -v
```

### Command Topics

**Topic Pattern:** `nus-smartstop/command/{device_id}`

**Example:**
```bash
# Send command to device
mosquitto_pub -h localhost \
  -t "nus-smartstop/command/esp32_001" \
  -m "BEEP"
```

## 🔒 Security

### Flask Image Server

- **API Key Authentication**: All uploads require `X-API-Key` header
- **Image Validation**: Validates uploaded files are actual images
- **File Size Limits**: Maximum 16MB (configurable)
- **Secure Filenames**: Sanitizes filenames to prevent injection

### Infrastructure Security

- **InfluxDB**: Runs locally (157.230.250.226:8086), token-based authentication
- **MQTT Broker**: Currently allows anonymous connections (enable authentication for production)
- **Systemd Services**: Run with appropriate permissions

### Production Recommendations

1. Change default credentials (InfluxDB password, Flask API key)
2. Enable MQTT authentication
3. Use HTTPS with SSL certificates
4. Configure firewall (only expose necessary ports)
5. Keep all services updated

## 🤖 ML Models

The project uses machine learning models for crowd analysis at bus stops.

### Models

1. **CNN (Convolutional Neural Network)**: Image classification model for crowd detection
   - Training notebook: `ml_models/Crowd Classification CAM.ipynb`
   
2. **XGBoost**: Classification model for crowd analysis
   - Training notebook: `ml_models/Test classification.ipynb`

### Integration

The Flask server runs ML inference on uploaded images and publishes results to MQTT for real-time analysis.

### Supported Use Cases

- **Crowd Detection**: Identify crowd density levels
- **Occupancy Analysis**: Estimate number of people waiting
- **Scene Classification**: Classify bus stop conditions

---

**Note**: This is an educational project for CS3237 (Introduction to Internet of Things).

**Production Server**: 157.230.250.226 (DigitalOcean, Ubuntu 24.04, 512MB RAM)
