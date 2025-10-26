# Deployment Guide for NUS SmartStop

This guide explains how to deploy the NUS SmartStop system on Ubuntu 24.04 using systemd services.

## Server Information

- **Deployment Server**: 157.230.250.226 (DigitalOcean Droplet)
- **OS**: Ubuntu 24.04 LTS
- **Services**: Mosquitto, Telegraf, InfluxDB, Flask (all running natively with systemd)

## Prerequisites

```bash
# Update system
sudo apt update && sudo apt upgrade -y

# Install required packages
sudo apt install -y mosquitto mosquitto-clients telegraf influxdb2 python3-pip python3-venv
```

## File Deployment Mapping

The repository has a clean project structure, but files are deployed to specific system locations:

| Repository File | Deployment Location | Purpose |
|----------------|---------------------|---------|
| `server/flask/app.py` | `/root/cs3237_server/image_server.py` | Flask image upload server |
| `telegraf.conf` | `/etc/telegraf/telegraf.conf` | Telegraf MQTT→InfluxDB bridge config |
| `server/systemd/mosquitto-cs3237.conf` | `/etc/mosquitto/conf.d/cs3237.conf` | Mosquitto MQTT broker config |
| `server/systemd/flask-image-server.service` | `/etc/systemd/system/flask-image-server.service` | Flask systemd service |
| `server/systemd/telegraf.service` | `/etc/systemd/system/telegraf.service` | Telegraf systemd service |

## Step-by-Step Deployment

### 1. Setup InfluxDB

```bash
# InfluxDB is installed locally on the same droplet
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

# Generate API token and save it
influx auth create --org "NUS SmartStop" --all-access
# Save the token for telegraf.conf
```

### 2. Deploy Mosquitto MQTT Broker

```bash
# Copy configuration
sudo cp server/systemd/mosquitto-cs3237.conf /etc/mosquitto/conf.d/cs3237.conf

# Test configuration
mosquitto -c /etc/mosquitto/mosquitto.conf -v

# Start and enable service
sudo systemctl restart mosquitto
sudo systemctl enable mosquitto

# Verify it's running
sudo systemctl status mosquitto
mosquitto_sub -h localhost -t "#" -v
```

### 3. Deploy Telegraf

```bash
# Copy configuration
sudo cp telegraf.conf /etc/telegraf/telegraf.conf

# IMPORTANT: Edit the config to add your InfluxDB token
sudo nano /etc/telegraf/telegraf.conf
# Update the line: token = "YOUR_INFLUXDB_TOKEN_HERE"

# Copy systemd service file
sudo cp server/systemd/telegraf.service /etc/systemd/system/telegraf.service

# Reload systemd and start service
sudo systemctl daemon-reload
sudo systemctl start telegraf
sudo systemctl enable telegraf

# Verify it's running
sudo systemctl status telegraf
sudo journalctl -u telegraf -f
```

### 4. Deploy Flask Image Server

```bash
# Create deployment directory
sudo mkdir -p /root/cs3237_server/uploads

# Copy Flask application
sudo cp server/flask/app.py /root/cs3237_server/image_server.py

# Install Python dependencies
cd /root/cs3237_server
sudo python3 -m venv venv
source venv/bin/activate
pip install flask flask-cors python-dotenv werkzeug

# Update systemd service file with correct Python path
sudo cp server/systemd/flask-image-server.service /etc/systemd/system/flask-image-server.service

# Edit service file to use venv Python
sudo nano /etc/systemd/system/flask-image-server.service
# Change ExecStart to: /root/cs3237_server/venv/bin/python3 /root/cs3237_server/image_server.py

# Create environment file (optional)
cat << EOF | sudo tee /root/cs3237_server/.env
FLASK_HOST=0.0.0.0
FLASK_PORT=5000
UPLOAD_FOLDER=/root/cs3237_server/uploads
API_KEY=CS3237-Group10-SecretKey
EOF

# Start and enable service
sudo systemctl daemon-reload
sudo systemctl start flask-image-server
sudo systemctl enable flask-image-server

# Verify it's running
sudo systemctl status flask-image-server
curl http://localhost:5000/health
```

## Service Management

### Check Service Status

```bash
# Check all services
sudo systemctl status mosquitto
sudo systemctl status telegraf
sudo systemctl status influxdb
sudo systemctl status flask-image-server

# View logs
sudo journalctl -u mosquitto -f
sudo journalctl -u telegraf -f
sudo journalctl -u influxdb -f
sudo journalctl -u flask-image-server -f
```

### Restart Services

```bash
# Restart individual services
sudo systemctl restart mosquitto
sudo systemctl restart telegraf
sudo systemctl restart influxdb
sudo systemctl restart flask-image-server

# Restart all at once
sudo systemctl restart mosquitto telegraf influxdb flask-image-server
```

### Update Configuration

When updating configurations from the repository:

```bash
# Update Mosquitto config
sudo cp server/systemd/mosquitto-cs3237.conf /etc/mosquitto/conf.d/cs3237.conf
sudo systemctl restart mosquitto

# Update Telegraf config
sudo cp telegraf.conf /etc/telegraf/telegraf.conf
# Don't forget to re-add your InfluxDB token!
sudo systemctl restart telegraf

# Update Flask application
sudo cp server/flask/app.py /root/cs3237_server/image_server.py
sudo systemctl restart flask-image-server
```

## Testing the Deployment

### Test MQTT

```bash
# Subscribe to all topics
mosquitto_sub -h 127.0.0.1 -t "#" -v

# In another terminal, publish test message
mosquitto_pub -h 127.0.0.1 \
  -t "nus-smartstop/sensors/ultrasonic" \
  -m '{"deviceId":"test_device","location":"test","distance":100,"timestamp":1234567890}'
```

### Test Flask Server

```bash
# Health check
curl http://127.0.0.1:5000/health

# Test image upload (with API key)
curl -X POST \
  -H "X-API-Key: CS3237-Group10-SecretKey" \
  -F "image=@test_image.jpg" \
  -F "device_id=esp32_001" \
  http://127.0.0.1:5000/upload

# Test without API key (should fail)
curl -X POST \
  -F "image=@test_image.jpg" \
  http://127.0.0.1:5000/upload
```

### Test Telegraf → InfluxDB Pipeline

```bash
# Publish test sensor data
mosquitto_pub -h 127.0.0.1 \
  -t "nus-smartstop/sensors/ultrasonic" \
  -m '{"deviceId":"esp32_001","location":"bus_stop_01","distance":125.5,"timestamp":1698765432}'

# Check Telegraf logs to confirm it received the message
sudo journalctl -u telegraf -n 50

# Query InfluxDB to verify data was written
influx query 'from(bucket: "sensor_data") |> range(start: -1h) |> limit(n: 10)'
```

## Firewall Configuration

```bash
# Allow required ports
sudo ufw allow 1883/tcp  # MQTT
sudo ufw allow 5000/tcp  # Flask
sudo ufw allow 8086/tcp  # InfluxDB (if accessing remotely)
sudo ufw enable
```

## Security Considerations

1. **Change Default Passwords**:
   - Update InfluxDB admin password
   - Change Flask API key in `/root/cs3237_server/.env`

2. **Restrict MQTT Access** (optional):
   - Edit `/etc/mosquitto/conf.d/cs3237.conf`
   - Set `allow_anonymous false`
   - Configure username/password authentication

3. **HTTPS for Flask** (production):
   - Use nginx as reverse proxy
   - Configure SSL certificates with Let's Encrypt

4. **InfluxDB Access**:
   - InfluxDB runs locally (127.0.0.1:8086)
   - Access from external tools via SSH tunnel if needed

## Monitoring

### System Resources

```bash
# Check CPU/Memory usage
htop

# Check disk space
df -h

# Check service resource usage
systemctl status mosquitto telegraf influxdb flask-image-server
```

### Application Logs

```bash
# Real-time log monitoring
sudo journalctl -u flask-image-server -u telegraf -u mosquitto -f

# Check for errors
sudo journalctl -u flask-image-server --since "1 hour ago" | grep -i error
sudo journalctl -u telegraf --since "1 hour ago" | grep -i error
```

### InfluxDB Metrics

```bash
# Check data ingestion rate
influx query 'from(bucket: "sensor_data") |> range(start: -1h) |> count()'

# List measurements
influx query 'from(bucket: "sensor_data") |> range(start: -24h) |> group() |> distinct(column: "_measurement")'
```

## Backup and Recovery

### Backup InfluxDB Data

```bash
# Backup all data
influx backup /root/backups/influxdb-$(date +%Y%m%d)

# Backup specific bucket
influx backup --bucket sensor_data /root/backups/sensor_data-$(date +%Y%m%d)
```

### Restore InfluxDB Data

```bash
influx restore /root/backups/influxdb-20231025
```

### Backup Configuration Files

```bash
# Create backup directory
mkdir -p /root/backups/configs

# Backup all configs
sudo cp /etc/telegraf/telegraf.conf /root/backups/configs/
sudo cp /etc/mosquitto/conf.d/cs3237.conf /root/backups/configs/
sudo cp /root/cs3237_server/.env /root/backups/configs/
```

## Troubleshooting

### Service Won't Start

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

### No Data in InfluxDB

1. Check Telegraf logs: `sudo journalctl -u telegraf -f`
2. Verify MQTT messages: `mosquitto_sub -h localhost -t "#" -v`
3. Test Telegraf config: `telegraf --config /etc/telegraf/telegraf.conf --test`
4. Check InfluxDB token in telegraf.conf

### Flask Upload Fails

1. Check Flask logs: `sudo journalctl -u flask-image-server -f`
2. Verify API key: Check `X-API-Key` header matches environment variable
3. Check file permissions: `ls -la /root/cs3237_server/uploads`
4. Test endpoint: `curl -v http://localhost:5000/health`

## Updating the System

### Update from Repository

```bash
# Pull latest changes
cd /path/to/NUS-SmartStop
git pull origin main

# Follow deployment steps above to copy updated files
# Remember to restart affected services
```

### System Updates

```bash
# Update packages
sudo apt update && sudo apt upgrade -y

# Restart services after updates
sudo systemctl restart mosquitto telegraf influxdb flask-image-server
```

## Additional Resources

- [Mosquitto Documentation](https://mosquitto.org/documentation/)
- [Telegraf Documentation](https://docs.influxdata.com/telegraf/)
- [InfluxDB Documentation](https://docs.influxdata.com/influxdb/v2.7/)
- [Flask Documentation](https://flask.palletsprojects.com/)
- [Systemd Service Documentation](https://www.freedesktop.org/software/systemd/man/systemd.service.html)

## Support

For deployment issues:
1. Check service logs first
2. Verify all services are running: `sudo systemctl status mosquitto telegraf influxdb flask-image-server`
3. Test each component individually
4. Review this deployment guide step-by-step

---

**Note**: This deployment assumes Ubuntu 24.04 LTS on DigitalOcean droplet at 157.230.250.226. Adjust paths and configurations as needed for different environments.
