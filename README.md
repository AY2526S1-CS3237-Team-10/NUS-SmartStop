# NUS-SmartStop

A comprehensive IoT solution for smart bus stops using ESP32, featuring real-time sensor monitoring, image capture and analysis, and ML-powered insights.

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
- **Documentation**: Comprehensive guides in `docs/` folder
- **Deployment**: Native Ubuntu 24.04 with systemd (no Docker)

## 📁 Project Structure

```
NUS-SmartStop/
├── esp32/                      # ESP32 firmware code
│   └── smartstop_main.ino     # Main ESP32 sketch with API auth
├── esp32_cam/                      # ESP32 firmware code
|   ├── camera_pins.h          # Header Pins for ESP32CAM
│   └── CameraPhotoCapture.ino     # Main ESP32CAM sketch with Flask server image publishing
├── server/
│   ├── flask/                 # Flask image server
│   │   ├── image_server.py   # Flask application with dual upload modes
│   │   ├── esp32cam_image_server.py   # Flask application with auto image processing with run_analysis.py
│   │   ├── run_analysis.py   # Base Code for .jpg file image comparision and analysis, to upload to DB
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
├── docs/                     # Documentation
│   ├── DEPLOYMENT.md         # Production deployment guide
│   ├── TELEGRAF.md          # Telegraf setup guide
│   ├── ARCHITECTURE.md      # System architecture
│   └── API.md               # API reference
├── telegraf.conf             # Telegraf MQTT→InfluxDB bridge config
├── requirements.txt          # Python dependencies
├── .env.example             # Environment variables template
└── README.md                # This file
```

## 🏗️ Architecture

```
ESP32-CAM → HTTP POST (with API key) → Flask Server → Image Storage
                                          ↓
                                     SQLite Metadata

ESP32 Sensors → MQTT → Mosquitto → Telegraf → InfluxDB (Local)
                                         ↓
                                   Time-series Data
```

**Key Components:**
- **ESP32-CAM**: Captures images, uploads via HTTP with X-API-Key authentication
- **ESP32 Sensors**: Collects sensor data, publishes to MQTT
- **MQTT Broker (Mosquitto)**: Message routing via systemd
- **Telegraf**: Bridges MQTT topics to InfluxDB (via systemd)
- **InfluxDB**: Time-series storage (local, same server at 127.0.0.1:8086)
- **Flask**: Dual-mode image server (multipart + raw body) with API authentication and gallery UI

**Deployment**: All services run natively on Ubuntu 24.04 using systemd (no Docker).  
**Production Server**: 157.230.250.226 (DigitalOcean, 512MB RAM)

## 🛠️ Hardware Requirements

### ESP32 DOIT DevKit V1
- **Microcontroller**: ESP32-WROOM-32
- **Flash**: 4MB
- **RAM**: 520KB
- **WiFi**: 802.11 b/g/n
- **Bluetooth**: v4.2 BR/EDR and BLE

### Sensors and Peripherals
- **Camera Module**: ESP32-CAM (AI-Thinker) with OV2640
- **Ultrasonic Sensor**: HC-SR04 or similar
- **Speaker/Buzzer**: For audio notifications
- **Power Supply**: 5V USB or external power adapter

### Pin Configuration (Default)
```
Ultrasonic Sensor:
- TRIG_PIN: GPIO 12
- ECHO_PIN: GPIO 14

Speaker:
- SPEAKER_PIN: GPIO 13

Camera (ESP32-CAM):
- See esp32/smartstop_main.ino for detailed pin mapping
```

## 📦 Software Requirements

### For Server (Ubuntu 24.04)
- Python 3.8+
- Mosquitto MQTT broker
- Telegraf
- InfluxDB 2.x
- systemd (for service management)

### For ESP32
- Arduino IDE 1.8+ or PlatformIO
- ESP32 Board Support Package
- Required Libraries:
  - WiFi.h
  - PubSubClient
  - HTTPClient
  - esp_camera.h (for ESP32-CAM)

## 🚀 Quick Start

**Note**: This project does NOT use Docker. All services run natively using systemd or directly via Python.

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

### For Production Deployment (Ubuntu 24.04)

This project uses **native systemd services** (no Docker). Follow these steps for manual deployment:

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

# Initial setup (one-time)
influx setup \
  --username admin \
  --password <secure-password> \
  --org "NUS SmartStop" \
  --bucket sensor_data \
  --retention 30d \
  --force

# Generate API token for Telegraf
influx auth create --org "NUS SmartStop" --all-access
# Save the token - you'll need it for telegraf.conf
```

#### 3. Deploy Mosquitto MQTT Broker

```bash
# Copy configuration
sudo cp server/systemd/mosquitto-cs3237.conf /etc/mosquitto/conf.d/cs3237.conf

# Start and enable service
sudo systemctl restart mosquitto
sudo systemctl enable mosquitto

# Verify it's running
sudo systemctl status mosquitto
```

#### 4. Deploy Telegraf

```bash
# Copy configuration
sudo cp telegraf.conf /etc/telegraf/telegraf.conf

# IMPORTANT: Edit config to add your InfluxDB token
sudo nano /etc/telegraf/telegraf.conf
# Update line: token = "YOUR_INFLUXDB_TOKEN_HERE"

# Copy systemd service file
sudo cp server/systemd/telegraf.service /etc/systemd/system/telegraf.service

# Start and enable service
sudo systemctl daemon-reload
sudo systemctl start telegraf
sudo systemctl enable telegraf

# Verify it's running
sudo systemctl status telegraf
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
pip install -r /path/to/NUS-SmartStop/server/flask/requirements.txt

# Create environment file
cat > /root/cs3237_server/.env << 'EOF'
API_KEY=CS3237-Group10-SecretKey
UPLOAD_FOLDER=/root/cs3237_server/uploads
FLASK_HOST=0.0.0.0
FLASK_PORT=5000
MAX_CONTENT_LENGTH=16777216
EOF

# Copy systemd service file
sudo cp server/systemd/flask-image-server.service /etc/systemd/system/flask-image-server.service

# Edit service to use venv Python (if needed)
sudo nano /etc/systemd/system/flask-image-server.service
# Set: ExecStart=/root/cs3237_server/venv/bin/python3 /root/cs3237_server/image_server.py

# Start and enable service
sudo systemctl daemon-reload
sudo systemctl start flask-image-server
sudo systemctl enable flask-image-server

# Verify it's running
sudo systemctl status flask-image-server
curl http://localhost:5000/health
```

#### 6. Verify All Services

```bash
# Check status of all services
sudo systemctl status mosquitto telegraf influxdb flask-image-server

# Test MQTT
mosquitto_sub -h localhost -t "#" -v

# Test Flask
curl http://localhost:5000/health

# View logs if needed
sudo journalctl -u mosquitto -f
sudo journalctl -u telegraf -f
sudo journalctl -u flask-image-server -f
```

**For complete deployment guide with troubleshooting, see [docs/DEPLOYMENT.md](docs/DEPLOYMENT.md)**

### ESP32 Setup

1. **Install Arduino IDE** and ESP32 board support:
   - Open Arduino IDE
   - Go to File > Preferences
   - Add to Additional Board Manager URLs:
     ```
     https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
     ```
   - Go to Tools > Board > Board Manager
   - Search for "esp32" and install

2. **Install Required Libraries**:
   - Tools > Manage Libraries
   - Install: PubSubClient, ArduinoJson, HTTPClient

3. **Configure WiFi and Server Details**:
   - Open `esp32/smartstop_main.ino`
   - Update:
     ```cpp
     const char* ssid = "YOUR_WIFI_SSID";
     const char* password = "YOUR_WIFI_PASSWORD";
     const char* mqtt_server = "157.230.250.226";  // Production server
     const char* flask_server = "http://157.230.250.226:5000";
     const char* api_key = "CS3237-Group10-SecretKey";  // Must match server!
     ```

4. **Upload to ESP32**:
   - Connect ESP32 via USB
   - Select Board: Tools > Board > ESP32 Dev Module
   - Select Port: Tools > Port > (your port)
   - Click Upload

## 🔄 Service Management

Once deployed, manage your systemd services with these commands:

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

## 🔧 Configuration

### Environment Variables (.env)

```bash
# Flask Server Configuration
FLASK_HOST=0.0.0.0
FLASK_PORT=5000
FLASK_DEBUG=False
UPLOAD_FOLDER=/root/cs3237_server/images
MAX_CONTENT_LENGTH=16777216  # 16MB max upload

# Flask API Authentication (REQUIRED!)
API_KEY=CS3237-Group10-SecretKey

# InfluxDB Configuration (Local)
INFLUXDB_URL=http://127.0.0.1:8086
INFLUXDB_TOKEN=your-influxdb-token  # Set in telegraf.conf
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

**Important**: The API key in `.env` must match the key in your ESP32-CAM code!

## 📡 API Endpoints

### Flask Image Server

**All upload endpoints require `X-API-Key` header for authentication.**

#### Health Check
```bash
GET /health

Response:
{
  "status": "running",
  "timestamp": "2025-10-26T17:21:29Z",
  "images_stored": 42,
  "disk_free_gb": 8.5
}

Example:
curl http://157.230.250.226:5000/health
```

#### Upload Image (Raw Body - ESP32-CAM Compatible)
```bash
POST /upload
Headers:
  X-API-Key: CS3237-Group10-SecretKey
  Device-ID: ESP32_001
  Content-Type: image/jpeg
Body: (raw image bytes)

Response:
{
  "success": true,
  "filename": "ESP32_001_20251026_172129.jpg",
  "size": 82540,
  "url": "/images/ESP32_001_20251026_172129.jpg"
}

Example:
curl -X POST \
  -H "X-API-Key: CS3237-Group10-SecretKey" \
  -H "Device-ID: ESP32_001" \
  -H "Content-Type: image/jpeg" \
  --data-binary "@photo.jpg" \
  http://157.230.250.226:5000/upload
```

#### Upload Image (Multipart - Web/Mobile Compatible)
```bash
POST /upload
Headers:
  X-API-Key: CS3237-Group10-SecretKey
Content-Type: multipart/form-data
Body:
  - image: (file)
  - device_id: (optional)

Response: (same as raw body)

Example:
curl -X POST \
  -H "X-API-Key: CS3237-Group10-SecretKey" \
  -F "image=@photo.jpg" \
  -F "device_id=esp32_001" \
  http://157.230.250.226:5000/upload
```

#### List Images
```bash
GET /images?limit=50&offset=0

Response:
{
  "count": 100,
  "limit": 50,
  "offset": 0,
  "images": [
    {
      "filename": "ESP32_001_20251026_172129.jpg",
      "size": 82540,
      "url": "/images/ESP32_001_20251026_172129.jpg",
      "timestamp": "2025-10-26T17:21:29Z"
    },
    ...
  ]
}

Example:
curl http://157.230.250.226:5000/images
```

#### Get Image
```bash
GET /images/<filename>

Response: Image file (JPEG)

Example:
curl http://157.230.250.226:5000/images/ESP32_001_20251026_172129.jpg
```

#### Web Gallery
```bash
GET /

Response: HTML gallery interface

Access in browser:
http://157.230.250.226:5000/
```

## 🔌 MQTT Topics

All topics use the `nus-smartstop/` prefix.

### Sensor Data (Published by ESP32)
```
Topic: nus-smartstop/sensors/{sensor_type}
Payload: {
  "deviceId": "esp32_001",
  "location": "bus_stop_01",
  "distance": 123.45,
  "timestamp": 1698765432
}
```

**Important**: Use `deviceId` (not `device_id`) as this is the tag key configured in Telegraf.

### Camera Events (Published by ESP32)
```
Topic: nus-smartstop/camera
Payload: {
  "deviceId": "esp32_001",
  "location": "bus_stop_01",
  "event_type": "capture",
  "image_count": 1
}
```

### Commands (Subscribe on ESP32)
```
Topic: nus-smartstop/command/{device_id}
Payload: "BEEP" | "CAPTURE" | ...
```

## 🤖 ML Models

The project supports integration with various ML models for image analysis:

### Supported Use Cases
1. **Object Detection**: Detect buses, people, vehicles
2. **Crowd Analysis**: Estimate number of people waiting
3. **Scene Classification**: Classify bus stop conditions
4. **Anomaly Detection**: Detect unusual activities

### Adding Custom Models

1. Place your trained model in `ml_models/` directory
2. Update `ml_models/inference.py` to load your model:
   ```python
   handler = MLInferenceHandler()
   handler.load_model('your_model.pth')
   ```
3. Implement custom inference logic as needed

## 📊 Data Visualization

InfluxDB data can be visualized using:
- **InfluxDB UI**: Built-in at http://127.0.0.1:8086 (SSH tunnel to access)
- **Grafana**: Connect to InfluxDB datasource
- **Custom dashboards**: Query via InfluxDB API and Flux

## 🧪 Testing

```bash
# Test Flask server health
curl http://157.230.250.226:5000/health

# Test image upload with API key (should work)
curl -X POST \
  -H "X-API-Key: CS3237-Group10-SecretKey" \
  -H "Device-ID: test_device" \
  --data-binary "@test.jpg" \
  http://157.230.250.226:5000/upload

# Test without API key (should fail with 401)
curl -X POST \
  --data-binary "@test.jpg" \
  http://157.230.250.226:5000/upload

# Test MQTT (using mosquitto_pub)
mosquitto_pub -h 157.230.250.226 \
  -t "nus-smartstop/sensors/ultrasonic" \
  -m '{"deviceId":"test1","location":"test","distance":100,"timestamp":1698765432}'

# Subscribe to all topics
mosquitto_sub -h 157.230.250.226 -t "nus-smartstop/#" -v

# Check Telegraf logs
sudo journalctl -u telegraf -f

# Check Flask logs
sudo journalctl -u flask-image-server -f

# Query InfluxDB
influx query 'from(bucket: "sensor_data") |> range(start: -1h) |> limit(n: 10)'
```

## 🔒 Security Features

### Flask Image Server
- **API Key Authentication**: All uploads require `X-API-Key` header
- **Image Validation**: Validates uploaded files are actual images using Pillow
- **Secure Filenames**: Sanitizes filenames using `secure_filename()`
- **Device ID Sanitization**: Prevents injection attacks
- **Allowed Extensions**: Only png, jpg, jpeg, gif
- **Content-Length Limits**: 16MB max (configurable)
- **CORS Enabled**: For web dashboard access

### Infrastructure
- InfluxDB runs locally (127.0.0.1:8086), not exposed externally
- MQTT can be configured with authentication (see mosquitto.conf)
- Systemd services run with appropriate permissions
- Environment variables for secrets management

### Production Recommendations
- Change default API keys
- Enable MQTT authentication in production
- Use HTTPS with nginx reverse proxy
- Implement rate limiting for upload endpoints
- Regular security updates

## 📊 Database

### SQLite (Flask Metadata)
Uses SQLite (`server/flask/metadata.db`) to store:
- Upload timestamp (UTC)
- Filename
- File size in bytes
- Device ID
- Auto-incrementing ID

Schema:
```sql
CREATE TABLE uploads (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    timestamp TEXT NOT NULL,
    filename TEXT NOT NULL,
    size_bytes INTEGER NOT NULL,
    device_id TEXT NOT NULL
);
```

### InfluxDB (Sensor Time-Series Data)
Stores sensor readings with:
- Measurements: ultrasonic, temperature, humidity, etc.
- Tags: deviceId, location, sensor_type
- Fields: distance, value, etc.
- Timestamps: Nanosecond precision

## 🖼️ Gallery UI

The root endpoint (`/`) displays a responsive web gallery:
- 📸 Latest 30 images
- Grid layout with thumbnails
- Click to view full image
- Shows filename and metadata
- Responsive design for mobile/desktop

## 🐛 Troubleshooting

### ESP32-CAM Upload Fails with 401 Unauthorized
- **Cause**: Missing or incorrect API key
- **Solution**: Verify `X-API-Key` header matches server's `.env` API_KEY

### Camera Issues
- Verify pin configuration matches your camera module
- Check power supply (camera requires stable 5V)
- Reduce image quality if memory issues occur

### MQTT Connection Failed
- Verify broker is running: `sudo systemctl status mosquitto`
- Check broker logs: `sudo journalctl -u mosquitto -f`
- Test with mosquitto_sub: `mosquitto_sub -h localhost -t "#"`

### InfluxDB Connection Failed
- Verify InfluxDB is running: `sudo systemctl status influxdb`
- Check token in telegraf.conf
- Access InfluxDB UI: http://127.0.0.1:8086 (via SSH tunnel)

### Telegraf Not Writing Data
- Check Telegraf logs: `sudo journalctl -u telegraf -f`
- Verify MQTT messages are published: `mosquitto_sub -h localhost -t "#" -v`
- Test telegraf config: `telegraf --config /etc/telegraf/telegraf.conf --test`

## 📚 Documentation

- **[DEPLOYMENT.md](docs/DEPLOYMENT.md)** - Complete production deployment guide with systemd
- **[TELEGRAF.md](docs/TELEGRAF.md)** - Telegraf setup and troubleshooting
- **[ARCHITECTURE.md](docs/ARCHITECTURE.md)** - System architecture and data flows
- **[API.md](docs/API.md)** - Complete API reference with authentication
- **[HARDWARE.md](docs/HARDWARE.md)** - Hardware setup and wiring diagrams

## 📚 Additional Resources

- [ESP32 Documentation](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/)
- [InfluxDB Documentation](https://docs.influxdata.com/influxdb/v2.7/)
- [Telegraf Documentation](https://docs.influxdata.com/telegraf/)
- [MQTT Protocol](https://mqtt.org/)
- [Flask Documentation](https://flask.palletsprojects.com/)
- [Mosquitto MQTT Broker](https://mosquitto.org/)

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
- Check documentation in `docs/` folder
- Review troubleshooting section above

---

**Note**: This is an educational project for CS3237 (Introduction to Internet of Things). Always follow safety guidelines when working with electronics and IoT devices.

**Production Server**: 157.230.250.226 (DigitalOcean, Ubuntu 24.04, 512MB RAM)