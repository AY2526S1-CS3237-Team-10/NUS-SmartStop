# Quick Start Guide

## Prerequisites
- Python 3.8+ installed
- Docker and Docker Compose (recommended)
- InfluxDB instance (external, not in Docker)
- ESP32 DOIT DevKit V1
- Arduino IDE or PlatformIO

## 5-Minute Setup

### 1. Setup InfluxDB (2 minutes)

**Option A: Local Installation**
```bash
# Install InfluxDB 2.x from https://docs.influxdata.com/influxdb/v2.7/install/
```

**Option B: Use InfluxDB Cloud**
- Sign up at https://www.influxdata.com/products/influxdb-cloud/

**Configure InfluxDB:**
1. Create organization: `smartstop`
2. Create bucket: `sensor_data`
3. Generate API token with write permissions
4. Note down the URL and token

### 2. Server Setup (2 minutes)

```bash
# Clone the repository
git clone https://github.com/AY2526S1-CS3237-Team-10/NUS-SmartStop.git
cd NUS-SmartStop

# Install Python dependencies
pip install -r requirements.txt

# Setup environment
cp .env.example .env
# Edit .env: Set INFLUXDB_URL and INFLUXDB_TOKEN

# Start services (Mosquitto MQTT + Telegraf)
docker-compose up -d

# Start Flask server
python server/flask/app.py
```

**Or use the convenience script:**
```bash
./start.sh
```

### 3. ESP32 Setup (1 minute)

1. Open `esp32/smartstop_main.ino` in Arduino IDE
2. Update these lines:
   ```cpp
   const char* ssid = "YOUR_WIFI_SSID";
   const char* password = "YOUR_WIFI_PASSWORD";
   const char* mqtt_server = "YOUR_COMPUTER_IP";  // e.g., "192.168.1.100"
   const char* flask_server = "http://YOUR_COMPUTER_IP:5000";
   ```
3. Select Board: **ESP32 Dev Module**
4. Click **Upload**

### 4. Test Everything

```bash
# Test Flask server
curl http://localhost:5000/health

# Monitor MQTT messages
mosquitto_sub -h localhost -t "#" -v

# Check Telegraf logs
docker-compose logs -f telegraf

# Verify data in InfluxDB
# Open your InfluxDB UI and check the sensor_data bucket
```

## Services URLs

- **MQTT Broker**: localhost:1883
- **Flask API**: http://localhost:5000
- **InfluxDB**: Your external InfluxDB URL
- **Telegraf**: Running in Docker (bridging MQTT to InfluxDB)

## Common Commands

```bash
# Stop all services
./stop.sh

# View Docker logs
docker-compose logs -f

# Check MQTT messages
mosquitto_sub -h localhost -t "#" -v

# Check Telegraf status
docker-compose ps telegraf

# Restart Telegraf after config changes
docker-compose restart telegraf

# Test image upload
curl -X POST -F "image=@test.jpg" http://localhost:5000/api/upload
```

## Troubleshooting

**ESP32 won't connect to WiFi:**
- Check SSID and password
- Ensure WiFi is 2.4GHz (not 5GHz)

**MQTT connection failed:**
- Check broker is running: `docker-compose ps`
- Verify firewall isn't blocking port 1883

**No data in InfluxDB:**
- Check Telegraf logs: `docker-compose logs telegraf`
- Verify InfluxDB URL and token in .env
- Test MQTT: `mosquitto_sub -h localhost -t "#" -v`

**Can't upload to ESP32:**
- Hold BOOT button while uploading
- Check correct port is selected

## Architecture

```
ESP32 → MQTT Broker → Telegraf → InfluxDB (External)
          ↓
    Flask Server (Images)
```

**Key Change**: Using **Telegraf** to bridge MQTT to InfluxDB instead of custom Python client.

## Next Steps

1. Read detailed docs in `docs/` folder:
   - `docs/TELEGRAF.md` - Complete Telegraf setup guide
   - `docs/INSTALLATION.md` - Detailed installation
   - `docs/API.md` - API documentation
2. Customize device IDs and locations
3. Add more sensors
4. Train ML models for your use case
5. Build a dashboard

## Need Help?

- Check `docs/TELEGRAF.md` for Telegraf configuration
- Check `docs/INSTALLATION.md` for detailed setup
- See `docs/API.md` for API documentation
- Review `docs/ARCHITECTURE.md` for system design

**Important**: This project now uses **Telegraf** for MQTT to InfluxDB bridging. The custom Python MQTT client is deprecated.

Happy building! 🚀
