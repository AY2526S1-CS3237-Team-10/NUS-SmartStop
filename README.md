# NUS-SmartStop

A comprehensive IoT solution for smart bus stops using ESP32, featuring real-time sensor monitoring, image capture and analysis, and ML-powered insights.

## 🚀 Project Overview

NUS-SmartStop is an IoT project designed for smart bus stops that integrates:
- **ESP32 DOIT DevKit V1** microcontroller
- **Cameras** for image capture and analysis
- **Ultrasonic sensors** for distance/occupancy detection
- **Speakers** for audio notifications
- **InfluxDB** time-series database for sensor data (external)
- **Telegraf** for MQTT to InfluxDB bridging
- **MQTT** messaging protocol for real-time communication
- **Flask** web server for image handling
- **ML models** for image analysis and predictions

## 📁 Project Structure

```
NUS-SmartStop/
├── esp32/                      # ESP32 firmware code
│   └── smartstop_main.ino     # Main ESP32 sketch
├── server/
│   ├── flask/                 # Flask image server
│   │   └── app.py            # Flask application
│   ├── influxdb/             # InfluxDB client (optional)
│   │   └── client.py         # InfluxDB handler
│   └── mqtt/                 # MQTT broker config
│       ├── mqtt_client.py    # MQTT debug client (deprecated)
│       └── mosquitto.conf    # Mosquitto configuration
├── ml_models/                # ML model inference
│   └── inference.py          # Inference handler
├── docs/                     # Documentation
│   ├── TELEGRAF.md          # Telegraf setup guide
│   └── ...
├── telegraf.conf             # Telegraf configuration
├── docker-compose.yml        # Docker services (Mosquitto, Telegraf)
├── requirements.txt          # Python dependencies
├── .env.example             # Environment variables template
└── README.md                # This file
```

## 🏗️ Architecture

```
ESP32 Devices → MQTT Broker (Mosquitto) → Telegraf → InfluxDB (External)
                                             ↓
                                      Flask Server (Images)
```

**Key Components:**
- **ESP32**: Collects sensor data, captures images
- **MQTT Broker**: Message routing (Mosquitto)
- **Telegraf**: Bridges MQTT topics to InfluxDB
- **InfluxDB**: Time-series storage (managed externally)
- **Flask**: Image upload/retrieval and ML inference API

## 🛠️ Hardware Requirements

### ESP32 DOIT DevKit V1
- **Microcontroller**: ESP32-WROOM-32
- **Flash**: 4MB
- **RAM**: 520KB
- **WiFi**: 802.11 b/g/n
- **Bluetooth**: v4.2 BR/EDR and BLE

### Sensors and Peripherals
- **Camera Module**: ESP32-CAM or compatible OV2640/OV5640
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

### For Server (Python)
- Python 3.8+
- pip (Python package manager)
- Docker & Docker Compose (optional, for containerized deployment)

### For ESP32
- Arduino IDE 1.8+ or PlatformIO
- ESP32 Board Support Package
- Required Libraries:
  - WiFi.h
  - PubSubClient
  - HTTPClient
  - esp_camera.h

## 🚀 Quick Start

### 1. Clone the Repository
```bash
git clone https://github.com/AY2526S1-CS3237-Team-10/NUS-SmartStop.git
cd NUS-SmartStop
```

### 2. Setup InfluxDB (External)

Set up your InfluxDB instance separately (not in Docker). You can:
- Install InfluxDB locally: https://docs.influxdata.com/influxdb/v2.7/install/
- Use InfluxDB Cloud: https://www.influxdata.com/products/influxdb-cloud/
- Use an existing InfluxDB server

Create:
- Organization: `smartstop`
- Bucket: `sensor_data`
- API Token with write permissions

### 3. Server Setup

#### Using Docker (Recommended)
```bash
# Configure environment variables
cp .env.example .env
# Edit .env with your InfluxDB URL and token

# Start MQTT broker and Telegraf
docker-compose up -d

# Install Python dependencies (for Flask)
pip install -r requirements.txt

# Start Flask server
python server/flask/app.py
```

**Or use the convenience script:**
```bash
./start.sh
```

#### Manual Setup (without Docker)
```bash
# Install Python dependencies
pip install -r requirements.txt

# Install Telegraf
# See docs/TELEGRAF.md for installation instructions

# Install and configure Mosquitto MQTT broker
# Ubuntu/Debian: sudo apt-get install mosquitto
# macOS: brew install mosquitto

# Configure environment variables
cp .env.example .env
# Edit .env with your InfluxDB and MQTT settings

# Start Telegraf with config
telegraf --config telegraf.conf

# Start Flask server
python server/flask/app.py
```
### 4. ESP32 Setup

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
   - Install: PubSubClient, ArduinoJson

3. **Configure WiFi and Server Details**:
   - Open `esp32/smartstop_main.ino`
   - Update:
     ```cpp
     const char* ssid = "YOUR_WIFI_SSID";
     const char* password = "YOUR_WIFI_PASSWORD";
     const char* mqtt_server = "YOUR_MQTT_BROKER_IP";
     const char* flask_server = "http://YOUR_FLASK_SERVER_IP:5000";
     ```

4. **Upload to ESP32**:
   - Connect ESP32 via USB
   - Select Board: Tools > Board > ESP32 Dev Module
   - Select Port: Tools > Port > (your port)
   - Click Upload

## 🔧 Configuration

### Environment Variables (.env)

```bash
# InfluxDB Configuration
INFLUXDB_URL=http://localhost:8086
INFLUXDB_TOKEN=your-super-secret-token
INFLUXDB_ORG=smartstop
INFLUXDB_BUCKET=sensor_data

# MQTT Configuration
MQTT_BROKER=localhost
MQTT_PORT=1883
MQTT_TOPIC_SENSORS=smartstop/sensors
MQTT_TOPIC_CAMERA=smartstop/camera

# Flask Server Configuration
FLASK_HOST=0.0.0.0
FLASK_PORT=5000
FLASK_DEBUG=True
UPLOAD_FOLDER=./uploads
MAX_CONTENT_LENGTH=16777216

# ML Model Configuration
MODEL_PATH=./ml_models/
CONFIDENCE_THRESHOLD=0.5
```

## 📡 API Endpoints

### Flask Server

#### Health Check
```
GET /health
Response: {"status": "healthy", "timestamp": "..."}
```

#### Upload Image
```
POST /api/upload
Content-Type: multipart/form-data
Body: 
  - image: (file)
  - device_id: (optional)
  - location: (optional)
Response: {"status": "success", "filename": "...", "metadata": {...}}
```

#### Get Image
```
GET /api/images/<filename>
Response: Image file
```

#### List Images
```
GET /api/images
Response: {"count": 10, "images": ["...", "..."]}
```

#### Run Inference
```
POST /api/inference
Content-Type: application/json
Body: {"filename": "image.jpg"}
Response: {"filename": "...", "predictions": [...], "confidence": 0.95}
```

## 🔌 MQTT Topics

### Sensor Data (Published by ESP32)
```
Topic: smartstop/sensors/{sensor_type}/{device_id}
Payload: {
  "device_id": "esp32_001",
  "location": "bus_stop_01",
  "distance": 123.45,
  "timestamp": 1234567890
}
```

### Camera Events (Published by ESP32)
```
Topic: smartstop/camera/{device_id}
Payload: {
  "device_id": "esp32_001",
  "location": "bus_stop_01",
  "event_type": "capture",
  "image_count": 1
}
```

### Commands (Subscribe on ESP32)
```
Topic: smartstop/command/{device_id}
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
- **InfluxDB UI**: Built-in at http://localhost:8086
- **Grafana**: Connect to InfluxDB datasource
- **Custom dashboards**: Query via InfluxDB API

## 🧪 Testing

```bash
# Test Flask server
curl http://localhost:5000/health

# Test image upload
curl -X POST -F "image=@test_image.jpg" http://localhost:5000/api/upload

# Test MQTT (using mosquitto_pub)
mosquitto_pub -h localhost -t "smartstop/sensors/test/device1" \
  -m '{"device_id":"test1","distance":100}'

# Test InfluxDB connection
python server/influxdb/client.py
```

## 🔐 Security Considerations

- Change default passwords in `.env` and `docker-compose.yml`
- Use MQTT authentication in production
- Secure InfluxDB with proper tokens
- Implement HTTPS for Flask server in production
- Validate and sanitize all inputs

## 🐛 Troubleshooting

### ESP32 Connection Issues
- Verify WiFi credentials
- Check server IP addresses
- Ensure MQTT broker is running
- Check firewall settings

### Camera Issues
- Verify pin configuration matches your camera module
- Check power supply (camera requires stable power)
- Reduce image quality if memory issues occur

### MQTT Connection Failed
- Verify broker is running: `docker-compose ps`
- Check broker logs: `docker-compose logs mosquitto`
- Test with mosquitto_sub: `mosquitto_sub -h localhost -t "#"`

### InfluxDB Connection Failed
- Verify InfluxDB is running: `docker-compose ps`
- Check token and organization settings
- Access InfluxDB UI: http://localhost:8086

## 📚 Additional Resources

- [ESP32 Documentation](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/)
- [InfluxDB Documentation](https://docs.influxdata.com/influxdb/v2.7/)
- [MQTT Protocol](https://mqtt.org/)
- [Flask Documentation](https://flask.palletsprojects.com/)
- [PyTorch Documentation](https://pytorch.org/docs/)

## 👥 Team

CS3237 Team 10 - AY2526S1
National University of Singapore

## 📄 License

This project is for educational purposes as part of CS3237 coursework.

## 🤝 Contributing

1. Fork the repository
2. Create a feature branch
3. Commit your changes
4. Push to the branch
5. Create a Pull Request

## 📞 Support

For issues and questions, please open an issue on GitHub or contact the team members.

---

**Note**: This is an educational project. Always follow safety guidelines when working with electronics and IoT devices.