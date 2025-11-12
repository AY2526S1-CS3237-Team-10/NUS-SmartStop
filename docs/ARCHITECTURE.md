# Architecture Overview

## System Architecture

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
│                                                                  │
│  ┌───────────────────────────────────────────────────────────┐ │
│  │                    MQTT Broker                             │ │
│  │                   (Mosquitto)                              │ │
│  │                                                            │ │
│  │  Topics:                                                   │ │
│  │  - smartstop/sensors/*                                    │ │
│  │  - smartstop/camera/*                                     │ │
│  │  - smartstop/command/*                                    │ │
│  └─────┬────────────────────────────────────────────┬─────────┘ │
│        │                                             │           │
│        │                                             │           │
│  ┌─────▼─────────┐                           ┌──────▼────────┐  │
│  │ MQTT Client   │                           │ Flask Server  │  │
│  │               │                           │               │  │
│  │ - Subscribe   │                           │ - Image API   │  │
│  │ - Process     │                           │ - Inference   │  │
│  │ - Store       │                           │ - Storage     │  │
│  └─────┬─────────┘                           └───────────────┘  │
│        │                                                         │
│        │                                                         │
│  ┌─────▼──────────────────────────────────────────────────┐    │
│  │              InfluxDB (Time Series DB)                 │    │
│  │                                                         │    │
│  │  Buckets:                                              │    │
│  │  - sensor_data (ultrasonic, temp, etc.)               │    │
│  │  - camera_events (capture metadata)                   │    │
│  └──────────────────────────────────────────────────────┘     │
│                                                                  │
│  ┌──────────────────────────────────────────────────────┐      │
│  │            ML Models & Inference                      │      │
│  │                                                        │      │
│  │  - Object Detection                                   │      │
│  │  - Crowd Analysis                                     │      │
│  │  - Classification                                     │      │
│  └────────────────────────────────────────────────────────┘     │
└───────────────────────────────────────────────────────────────────┘
```

## Data Flow

### 1. Sensor Data Flow

```
ESP32 Sensors → MQTT Publish → MQTT Broker → MQTT Client → InfluxDB
```

**Example:**
1. Ultrasonic sensor measures distance: 125 cm
2. ESP32 publishes JSON to `smartstop/sensors/ultrasonic/esp32_001`
3. MQTT client receives message
4. Data is parsed and written to InfluxDB
5. Data can be queried and visualized

### 2. Image Capture Flow

```
ESP32 Camera → HTTP POST → Flask Server → Storage → ML Inference
                    ↓
              MQTT Notification → MQTT Broker → MQTT Client → InfluxDB (metadata)
```

**Example:**
1. ESP32 captures image
2. Image uploaded to Flask server via HTTP POST
3. Flask stores image with timestamp
4. ESP32 publishes event notification via MQTT
5. Metadata stored in InfluxDB
6. Flask can run ML inference on demand

### 3. Command Flow

```
Control System → MQTT Publish → MQTT Broker → ESP32 Subscribe → Action
```

**Example:**
1. User/system publishes "BEEP" to `smartstop/command/esp32_001`
2. ESP32 receives command
3. ESP32 activates speaker

## Component Details

### ESP32 (Edge Device)

**Responsibilities:**
- Sensor data collection
- Image capture
- MQTT communication
- Local processing
- Command execution (speaker, etc.)

**Technologies:**
- Arduino/ESP-IDF framework
- WiFi networking
- MQTT protocol
- Camera drivers

### MQTT Broker (Communication Hub)

**Responsibilities:**
- Message routing
- Topic management
- Client connections
- Message buffering

**Technologies:**
- Eclipse Mosquitto
- MQTT protocol v3.1.1/v5.0
- Managed via systemd on Ubuntu

### Flask Server (Image Handler)

**Responsibilities:**
- Image reception and storage
- REST API endpoints
- ML inference orchestration
- Image serving

**Technologies:**
- Flask web framework
- Python
- PIL/OpenCV for image processing
- PyTorch/TensorFlow for ML

### MQTT Client (Data Processor)

**Responsibilities:**
- Subscribe to sensor topics
- Parse incoming data
- Write to InfluxDB
- Error handling

**Technologies:**
- Paho MQTT client
- Python
- InfluxDB client library

### InfluxDB (Time Series Database)

**Responsibilities:**
- Store time-series sensor data
- Store event metadata
- Provide query interface
- Data retention management

**Technologies:**
- InfluxDB 2.x
- Flux query language
- Native installation via systemd

### ML Models (Intelligence Layer)

**Responsibilities:**
- Image analysis
- Object detection
- Crowd counting
- Anomaly detection

**Technologies:**
- PyTorch
- TensorFlow
- Pre-trained models (YOLO, ResNet, etc.)
- Custom trained models

## Communication Protocols

### MQTT

**Topic Structure:**
```
smartstop/
├── sensors/
│   ├── ultrasonic/{device_id}
│   ├── temperature/{device_id}
│   └── humidity/{device_id}
├── camera/
│   └── {device_id}
└── command/
    └── {device_id}
```

**QoS Levels:**
- Sensor data: QoS 0 (at most once) - acceptable loss
- Camera events: QoS 1 (at least once) - ensure delivery
- Commands: QoS 1 (at least once) - ensure delivery

### HTTP/REST

**Flask API Endpoints:**
- `GET /health` - Health check
- `POST /api/upload` - Image upload
- `GET /api/images/<filename>` - Retrieve image
- `GET /api/images` - List images
- `POST /api/inference` - Run ML inference

## Data Models

### Sensor Data (MQTT/InfluxDB)

```json
{
  "device_id": "esp32_001",
  "location": "bus_stop_01",
  "distance": 125.5,
  "timestamp": 1634567890
}
```

**InfluxDB Schema:**
- Measurement: `ultrasonic`
- Tags: `device_id`, `location`
- Fields: `distance`
- Timestamp: Auto-generated or provided

### Camera Event (MQTT)

```json
{
  "device_id": "esp32_001",
  "location": "bus_stop_01",
  "event_type": "capture",
  "image_count": 1
}
```

### Image Upload (HTTP)

**Request:**
```
POST /api/upload
Content-Type: multipart/form-data

image: (binary)
device_id: "esp32_001"
location: "bus_stop_01"
```

**Response:**
```json
{
  "status": "success",
  "filename": "20231025_143022_capture.jpg",
  "filepath": "./uploads/20231025_143022_capture.jpg",
  "metadata": {
    "device_id": "esp32_001",
    "location": "bus_stop_01",
    "timestamp": "20231025_143022"
  }
}
```

## Scalability Considerations

### Horizontal Scaling
- Multiple ESP32 devices with unique IDs
- Multiple MQTT clients for load distribution
- Flask server can be load-balanced
- InfluxDB clustering for high throughput

### Vertical Scaling
- Increase container resources
- Optimize database queries
- Implement caching strategies
- Batch processing for ML inference

## Security Architecture

### Network Security
- WiFi encryption (WPA2/WPA3)
- Separate VLAN for IoT devices (recommended)
- Firewall rules

### Application Security
- MQTT authentication (configurable)
- InfluxDB token-based auth
- Flask input validation
- HTTPS for production (TLS/SSL)

### Data Security
- Encrypted data in transit
- Secure token storage
- Regular security updates
- Minimal permissions principle

## Monitoring and Logging

### Application Logs
- Flask: Request/response logs
- MQTT Client: Connection and message logs
- ESP32: Serial debug output

### System Metrics
- InfluxDB: Query performance
- MQTT: Message throughput
- Flask: Request latency
- ESP32: Memory usage, WiFi signal

## Deployment Options

### Development
- Local services on development machine
- Single ESP32 device
- Debug mode enabled

### Production
- Native systemd deployment on Ubuntu 24.04
- Multiple ESP32 devices
- Monitoring and alerting
- Data backup strategies
- High availability configuration

## Future Enhancements

1. **Real-time Alerts**: Notifications based on sensor thresholds
2. **Web Dashboard**: React/Vue.js frontend for monitoring
3. **Edge ML**: Run lightweight models on ESP32
4. **Cloud Integration**: AWS/Azure IoT Hub integration
5. **Advanced Analytics**: Historical data analysis and trends
6. **Mobile App**: Companion app for configuration and monitoring
