# Quick Start Guide

## Prerequisites
- Python 3.8+ installed
- Docker and Docker Compose (optional but recommended)
- ESP32 DOIT DevKit V1
- Arduino IDE or PlatformIO

## 5-Minute Setup

### 1. Server Setup (3 minutes)

```bash
# Clone the repository
git clone https://github.com/AY2526S1-CS3237-Team-10/NUS-SmartStop.git
cd NUS-SmartStop

# Install Python dependencies
pip install -r requirements.txt

# Setup environment
cp .env.example .env

# Start services (with Docker)
docker-compose up -d

# Start Flask server (Terminal 1)
python server/flask/app.py

# Start MQTT client (Terminal 2)
python server/mqtt/mqtt_client.py
```

**Or use the convenience script:**
```bash
./start.sh
```

### 2. ESP32 Setup (2 minutes)

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

### 3. Test Everything

```bash
# Test Flask server
curl http://localhost:5000/health

# Monitor MQTT messages
mosquitto_sub -h localhost -t "#" -v
```

## Services URLs

- **InfluxDB UI**: http://localhost:8086
- **Flask API**: http://localhost:5000
- **MQTT Broker**: localhost:1883

## Common Commands

```bash
# Stop all services
./stop.sh

# View Docker logs
docker-compose logs -f

# Check MQTT messages
mosquitto_sub -h localhost -t "#" -v

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

**Can't upload to ESP32:**
- Hold BOOT button while uploading
- Check correct port is selected

## Next Steps

1. Read detailed docs in `docs/` folder
2. Customize device IDs and locations
3. Add more sensors
4. Train ML models for your use case
5. Build a dashboard

## Need Help?

- Check `docs/INSTALLATION.md` for detailed setup
- See `docs/API.md` for API documentation
- Review `docs/ARCHITECTURE.md` for system design

Happy building! 🚀
