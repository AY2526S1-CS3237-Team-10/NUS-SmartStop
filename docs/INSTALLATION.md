# Installation Guide

## Prerequisites

### For Development Machine (Server)
- Python 3.8 or higher
- Docker and Docker Compose (optional but recommended)
- Git

### For ESP32 Development
- Arduino IDE 1.8+ or PlatformIO
- USB cable for ESP32 connection
- ESP32 DOIT DevKit V1

## Step-by-Step Installation

### 1. Server Installation

#### Install Python Dependencies

```bash
# Create virtual environment (recommended)
python -m venv venv
source venv/bin/activate  # On Windows: venv\Scripts\activate

# Install dependencies
pip install -r requirements.txt
```

#### Install Docker (Optional but Recommended)

**Ubuntu/Debian:**
```bash
sudo apt-get update
sudo apt-get install docker.io docker-compose
sudo usermod -aG docker $USER
```

**macOS:**
```bash
brew install docker docker-compose
```

**Windows:**
Download and install [Docker Desktop](https://www.docker.com/products/docker-desktop)

#### Setup Environment Variables

```bash
# Copy example environment file
cp .env.example .env

# Edit .env file with your configuration
nano .env  # or use your preferred editor
```

Key configurations to update:
- `INFLUXDB_TOKEN`: Generate a secure token
- `MQTT_BROKER`: Set to your machine's IP if ESP32 is on different network
- `FLASK_HOST`: Set to `0.0.0.0` to accept connections from ESP32

#### Start Services

**Using Docker (Recommended):**
```bash
# Start InfluxDB and MQTT broker
docker-compose up -d

# Check services are running
docker-compose ps

# View logs
docker-compose logs -f
```

**Manual Installation:**

1. **Install InfluxDB 2.x**
   ```bash
   # Ubuntu/Debian
   wget https://dl.influxdata.com/influxdb/releases/influxdb2-2.7.1-amd64.deb
   sudo dpkg -i influxdb2-2.7.1-amd64.deb
   sudo systemctl start influxdb
   
   # macOS
   brew install influxdb
   influxd
   ```

2. **Install Mosquitto MQTT**
   ```bash
   # Ubuntu/Debian
   sudo apt-get install mosquitto mosquitto-clients
   
   # macOS
   brew install mosquitto
   brew services start mosquitto
   ```

3. **Configure InfluxDB**
   - Open http://localhost:8086
   - Complete initial setup
   - Create organization: `smartstop`
   - Create bucket: `sensor_data`
   - Generate API token and update `.env`

#### Start Application Servers

```bash
# Terminal 1: Start Flask server
python server/flask/app.py

# Terminal 2: Start MQTT client
python server/mqtt/mqtt_client.py
```

### 2. ESP32 Installation

#### Install Arduino IDE

1. Download from [Arduino Website](https://www.arduino.cc/en/software)
2. Install for your operating system

#### Add ESP32 Board Support

1. Open Arduino IDE
2. Go to **File → Preferences**
3. In "Additional Board Manager URLs", add:
   ```
   https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
   ```
4. Click OK
5. Go to **Tools → Board → Boards Manager**
6. Search for "esp32"
7. Install "esp32 by Espressif Systems"

#### Install Required Libraries

1. Go to **Tools → Manage Libraries**
2. Install the following libraries:
   - **PubSubClient** by Nick O'Leary
   - **ArduinoJson** by Benoit Blanchon

#### Configure and Upload Code

1. Open `esp32/smartstop_main.ino` in Arduino IDE

2. Update WiFi credentials:
   ```cpp
   const char* ssid = "YOUR_WIFI_SSID";
   const char* password = "YOUR_WIFI_PASSWORD";
   ```

3. Update server addresses (use your machine's IP):
   ```cpp
   const char* mqtt_server = "192.168.1.100";  // Your computer's IP
   const char* flask_server = "http://192.168.1.100:5000";
   ```

4. Connect ESP32 via USB

5. Select board: **Tools → Board → ESP32 Dev Module**

6. Select port: **Tools → Port → (your port)**

7. Click **Upload** button

8. Open **Serial Monitor** (Tools → Serial Monitor) to view output

### 3. Hardware Connections

#### Ultrasonic Sensor (HC-SR04)
```
VCC  → 5V
GND  → GND
TRIG → GPIO 12
ECHO → GPIO 14
```

#### Speaker/Buzzer
```
Positive → GPIO 13 (through resistor if needed)
Negative → GND
```

#### Camera Module (ESP32-CAM)
If using ESP32-CAM, refer to pin definitions in the code.
Most ESP32-CAM modules have cameras pre-wired.

## Verification

### Test Server Components

1. **Test Flask Server:**
   ```bash
   curl http://localhost:5000/health
   ```
   Expected: `{"status": "healthy", ...}`

2. **Test MQTT:**
   ```bash
   # Subscribe to all topics
   mosquitto_sub -h localhost -t "#" -v
   
   # In another terminal, publish test message
   mosquitto_pub -h localhost -t "smartstop/test" -m "Hello"
   ```

3. **Test InfluxDB:**
   - Open http://localhost:8086
   - Login with credentials from docker-compose.yml
   - Navigate to Data Explorer

### Test ESP32

1. Open Serial Monitor in Arduino IDE
2. Verify connection messages:
   - WiFi connected
   - MQTT connected
   - Camera initialized
3. Check sensor readings appear in serial output

### End-to-End Test

1. ESP32 should publish sensor data to MQTT
2. MQTT client should receive and store data in InfluxDB
3. Check InfluxDB for data:
   ```bash
   # Or use InfluxDB UI at http://localhost:8086
   ```
4. Upload test image via ESP32 or curl
5. Verify image appears in uploads folder

## Troubleshooting

### ESP32 Not Connecting to WiFi
- Verify SSID and password are correct
- Check WiFi is 2.4GHz (ESP32 doesn't support 5GHz)
- Verify no special characters in credentials

### Cannot Upload to ESP32
- Check correct board selected
- Check correct port selected
- Try pressing BOOT button while uploading
- Check USB cable supports data (not just charging)

### MQTT Connection Failed
- Verify Mosquitto is running: `docker-compose ps` or `systemctl status mosquitto`
- Check firewall isn't blocking port 1883
- Verify IP address is correct and reachable

### InfluxDB Connection Issues
- Verify InfluxDB is running
- Check token is correct in .env
- Verify organization and bucket exist

### Camera Not Working
- Verify pin configuration matches your module
- Check camera module is properly connected
- Reduce image size if out of memory errors occur

## Next Steps

After successful installation:
1. Configure device IDs and locations in ESP32 code
2. Set up data visualization in InfluxDB or Grafana
3. Train and integrate ML models
4. Customize MQTT topics and data formats
5. Implement additional sensors or features

## Support

For issues:
1. Check Serial Monitor output from ESP32
2. Check server logs
3. Review troubleshooting section
4. Open issue on GitHub with logs and error messages
