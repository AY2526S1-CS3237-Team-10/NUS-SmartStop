# Telegraf Setup Guide for NUS-SmartStop

## What is Telegraf?

Telegraf is a plugin-driven server agent for collecting and sending metrics and events. For NUS-SmartStop, it bridges MQTT messages to InfluxDB, replacing the custom Python MQTT client.

## Why Use Telegraf?

✅ **Native MQTT Support**: Built-in MQTT consumer plugin
✅ **Direct InfluxDB Integration**: Native InfluxDB v2 output plugin
✅ **High Performance**: Written in Go, efficient and reliable
✅ **Buffering & Retry**: Handles network failures gracefully
✅ **Configuration**: Simple TOML configuration file
✅ **Production Ready**: Battle-tested by thousands of users

## Architecture

```
ESP32 Devices → MQTT Broker → Telegraf → InfluxDB
                                ↓
                          Flask Server (for images)
```

## Installation

### Option 1: Docker (Recommended)

Already configured in `docker-compose.yml`:

```bash
# Start services
docker-compose up -d

# Check Telegraf logs
docker-compose logs -f telegraf

# Restart Telegraf after config changes
docker-compose restart telegraf
```

### Option 2: Native Installation

#### Ubuntu/Debian
```bash
# Add InfluxData repository
wget -q https://repos.influxdata.com/influxdata-archive_compat.key
echo '393e8779c89ac8d958f81f942f9ad7fb82a25e133faddaf92e15b16e6ac9ce4c influxdata-archive_compat.key' | sha256sum -c && cat influxdata-archive_compat.key | gpg --dearmor | sudo tee /etc/apt/trusted.gpg.d/influxdata-archive_compat.gpg > /dev/null
echo 'deb [signed-by=/etc/apt/trusted.gpg.d/influxdata-archive_compat.gpg] https://repos.influxdata.com/debian stable main' | sudo tee /etc/apt/sources.list.d/influxdata.list

sudo apt-get update
sudo apt-get install telegraf

# Copy configuration
sudo cp telegraf.conf /etc/telegraf/telegraf.conf

# Start service
sudo systemctl enable telegraf
sudo systemctl start telegraf

# Check status
sudo systemctl status telegraf
```

#### macOS
```bash
# Install via Homebrew
brew install telegraf

# Copy configuration
cp telegraf.conf /usr/local/etc/telegraf.conf

# Start service
brew services start telegraf

# Check logs
tail -f /usr/local/var/log/telegraf.log
```

#### Windows
1. Download from https://portal.influxdata.com/downloads/
2. Extract to `C:\Program Files\Telegraf\`
3. Copy `telegraf.conf` to `C:\Program Files\Telegraf\`
4. Run as service:
   ```powershell
   cd "C:\Program Files\Telegraf"
   .\telegraf.exe --service install
   .\telegraf.exe --service start
   ```

## Configuration

### Environment Variables

Telegraf reads from `.env` file. Ensure these are set:

```bash
# InfluxDB Configuration (your external InfluxDB instance)
INFLUXDB_URL=http://your-influxdb-server:8086
INFLUXDB_TOKEN=your-influxdb-token
INFLUXDB_ORG=smartstop
INFLUXDB_BUCKET=sensor_data

# MQTT Configuration
MQTT_BROKER=localhost
MQTT_PORT=1883
```

### telegraf.conf

The configuration file (`telegraf.conf`) includes:

1. **Input Plugin: MQTT Consumer**
   - Subscribes to `smartstop/sensors/#` and `smartstop/camera/#`
   - Parses JSON payloads
   - Extracts tags (device_id, location, sensor_type)
   - Routes topics to measurements

2. **Output Plugin: InfluxDB v2**
   - Writes to your InfluxDB instance
   - Uses token authentication
   - Batches writes for efficiency

3. **Processors** (optional)
   - Rename measurements
   - Add default tags
   - Transform field values

## Data Flow

### Sensor Data Flow

1. **ESP32 publishes to MQTT**:
   ```
   Topic: smartstop/sensors/ultrasonic/esp32_001
   Payload: {
     "device_id": "esp32_001",
     "location": "bus_stop_01",
     "distance": 125.5,
     "timestamp": 1634567890
   }
   ```

2. **Telegraf consumes message**:
   - Subscribes to `smartstop/sensors/#`
   - Parses JSON
   - Extracts tags: `device_id=esp32_001`, `location=bus_stop_01`
   - Extracts fields: `distance=125.5`
   - Uses timestamp from payload

3. **Telegraf writes to InfluxDB**:
   ```
   Measurement: ultrasonic
   Tags: device_id=esp32_001, location=bus_stop_01
   Fields: distance=125.5
   Timestamp: 2021-10-18T12:34:50Z
   ```

## Testing

### 1. Test MQTT to Telegraf

Publish a test message:
```bash
mosquitto_pub -h localhost \
  -t "smartstop/sensors/ultrasonic/test_device" \
  -m '{"device_id":"test_device","location":"test","distance":100.5,"timestamp":1634567890}'
```

Check Telegraf logs:
```bash
# Docker
docker-compose logs -f telegraf

# Native
sudo journalctl -u telegraf -f
```

### 2. Verify Data in InfluxDB

Using InfluxDB UI (http://your-influxdb:8086):
1. Go to Data Explorer
2. Select bucket: `sensor_data`
3. Query:
   ```flux
   from(bucket: "sensor_data")
     |> range(start: -1h)
     |> filter(fn: (r) => r._measurement == "ultrasonic")
   ```

Using InfluxDB CLI:
```bash
influx query 'from(bucket: "sensor_data") |> range(start: -1h) |> filter(fn: (r) => r._measurement == "ultrasonic")'
```

## Monitoring Telegraf

### Health Check

```bash
# Check if Telegraf is running
docker-compose ps telegraf  # Docker
sudo systemctl status telegraf  # Native Linux
brew services list | grep telegraf  # macOS
```

### View Logs

```bash
# Docker
docker-compose logs -f telegraf

# Native Linux
sudo journalctl -u telegraf -f

# Native macOS
tail -f /usr/local/var/log/telegraf.log
```

### Metrics

Telegraf exposes internal metrics. Add to telegraf.conf:

```toml
[[inputs.internal]]
  collect_memstats = true
```

## Troubleshooting

### Telegraf Can't Connect to MQTT

**Symptoms**: Logs show connection errors

**Solutions**:
1. Check MQTT broker is running: `docker-compose ps mosquitto`
2. Verify MQTT_BROKER in .env: should be `localhost` or `mosquitto` (service name)
3. Check firewall: `sudo ufw status`
4. Test MQTT directly: `mosquitto_sub -h localhost -t "#" -v`

### Telegraf Can't Write to InfluxDB

**Symptoms**: Logs show "failed to write to InfluxDB"

**Solutions**:
1. Verify InfluxDB is accessible: `curl http://your-influxdb:8086/health`
2. Check INFLUXDB_TOKEN is correct
3. Verify organization and bucket exist
4. Check network connectivity between Telegraf and InfluxDB

### No Data Appearing in InfluxDB

**Solutions**:
1. Check MQTT messages are being published: `mosquitto_sub -h localhost -t "#" -v`
2. Verify JSON format matches expected schema
3. Check Telegraf logs for parsing errors
4. Ensure timestamp field is valid Unix timestamp
5. Test with a simple message (see Testing section)

### High Memory Usage

**Solutions**:
1. Reduce `metric_buffer_limit` in telegraf.conf
2. Increase `flush_interval` to batch more writes
3. Monitor with `docker stats` (Docker) or `top` (native)

## Advanced Configuration

### Multiple InfluxDB Outputs

Add additional output blocks:

```toml
[[outputs.influxdb_v2]]
  urls = ["http://backup-influxdb:8086"]
  token = "$INFLUXDB_BACKUP_TOKEN"
  organization = "$INFLUXDB_ORG"
  bucket = "$INFLUXDB_BUCKET"
```

### Topic-Based Routing

Customize topic parsing in telegraf.conf:

```toml
[[inputs.mqtt_consumer.topic_parsing]]
  topic = "smartstop/sensors/+/+"
  measurement = "_/_/measurement/_"
  tags = "_/_/_/device_id"
```

### Data Transformation

Add processors:

```toml
[[processors.converter]]
  [processors.converter.fields]
    float = ["distance", "temperature"]
    integer = ["image_count"]
```

## Comparison: Telegraf vs Custom MQTT Client

| Feature | Telegraf | Custom Python Client |
|---------|----------|---------------------|
| Performance | ⚡ High (Go) | 🐌 Medium (Python) |
| Reliability | ✅ Production-tested | ⚠️ Custom code |
| Buffering | ✅ Built-in | ❌ Manual |
| Retry Logic | ✅ Automatic | ❌ Manual |
| Configuration | ✅ TOML file | ⚠️ Code changes |
| Monitoring | ✅ Built-in metrics | ❌ Custom |
| Community | ✅ Large | ❌ None |

## Migration from Custom MQTT Client

The custom Python MQTT client (`server/mqtt/mqtt_client.py`) is **deprecated** in favor of Telegraf.

**Migration steps**:
1. ✅ Stop custom MQTT client
2. ✅ Configure telegraf.conf with your settings
3. ✅ Update .env with InfluxDB details
4. ✅ Start Telegraf: `docker-compose up -d telegraf`
5. ✅ Verify data flow (see Testing section)
6. ✅ Remove custom client from startup scripts

## Additional Resources

- [Telegraf Documentation](https://docs.influxdata.com/telegraf/)
- [MQTT Consumer Plugin](https://github.com/influxdata/telegraf/tree/master/plugins/inputs/mqtt_consumer)
- [InfluxDB v2 Output Plugin](https://github.com/influxdata/telegraf/tree/master/plugins/outputs/influxdb_v2)
- [Configuration Reference](https://github.com/influxdata/telegraf/blob/master/docs/CONFIGURATION.md)

## Support

For issues with Telegraf setup:
1. Check Telegraf logs (see Monitoring section)
2. Verify configuration syntax: `telegraf --config telegraf.conf --test`
3. Test components individually (MQTT, InfluxDB)
4. Review troubleshooting section above
5. Consult Telegraf documentation

---

**Note**: Telegraf is the recommended and supported method for MQTT to InfluxDB bridging in this project.
