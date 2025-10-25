# API Documentation

## Flask Server API

Base URL: `http://localhost:5000`

### Authentication

Currently, the API does not require authentication. For production deployment, implement token-based authentication.

---

## Endpoints

### 1. Health Check

Check if the server is running.

**Endpoint:** `GET /health`

**Response:**
```json
{
  "status": "healthy",
  "timestamp": "2023-10-25T14:30:22.123456"
}
```

**Example:**
```bash
curl http://localhost:5000/health
```

---

### 2. Upload Image

Upload an image from ESP32 or other client.

**Endpoint:** `POST /api/upload`

**Content-Type:** `multipart/form-data`

**Parameters:**
- `image` (required): Image file
- `device_id` (optional): Device identifier
- `location` (optional): Location identifier

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

**Error Response:**
```json
{
  "error": "No image file provided"
}
```

**Example:**
```bash
curl -X POST \
  -F "image=@test.jpg" \
  -F "device_id=esp32_001" \
  -F "location=bus_stop_01" \
  http://localhost:5000/api/upload
```

---

### 3. Get Image

Retrieve a previously uploaded image.

**Endpoint:** `GET /api/images/<filename>`

**Parameters:**
- `filename` (path): Name of the image file

**Response:** Image file (JPEG/PNG)

**Error Response:**
```json
{
  "error": "Image not found"
}
```

**Example:**
```bash
curl http://localhost:5000/api/images/20231025_143022_capture.jpg -o downloaded.jpg
```

---

### 4. List Images

Get a list of all uploaded images.

**Endpoint:** `GET /api/images`

**Response:**
```json
{
  "count": 5,
  "images": [
    "20231025_143022_capture.jpg",
    "20231025_143045_capture.jpg",
    "20231025_143102_capture.jpg"
  ]
}
```

**Example:**
```bash
curl http://localhost:5000/api/images
```

---

### 5. Run Inference

Run ML model inference on an uploaded image.

**Endpoint:** `POST /api/inference`

**Content-Type:** `application/json`

**Request Body:**
```json
{
  "filename": "20231025_143022_capture.jpg"
}
```

**Response:**
```json
{
  "filename": "20231025_143022_capture.jpg",
  "predictions": [
    {
      "class": "person",
      "confidence": 0.95,
      "bbox": [100, 150, 200, 300]
    },
    {
      "class": "bus",
      "confidence": 0.87,
      "bbox": [50, 100, 400, 350]
    }
  ],
  "confidence": 0.95
}
```

**Error Response:**
```json
{
  "error": "Filename required"
}
```

```json
{
  "error": "Image not found"
}
```

**Example:**
```bash
curl -X POST \
  -H "Content-Type: application/json" \
  -d '{"filename":"20231025_143022_capture.jpg"}' \
  http://localhost:5000/api/inference
```

---

## MQTT Topics and Payloads

### Topic Structure

```
nus-smartstop/
├── sensors/
│   ├── ultrasonic
│   ├── temperature
│   └── humidity
├── camera
└── command/
    └── {device_id}
```

---

### Sensor Data Topics

**Topic:** `nus-smartstop/sensors/{sensor_type}`

**Payload Example (Ultrasonic):**
```json
{
  "deviceId": "esp32_001",
  "location": "bus_stop_01",
  "distance": 125.5,
  "timestamp": 1634567890
}
```

**Note**: Use `deviceId` (not `device_id`) - it's configured as a tag key in Telegraf for filtering.

**Payload Example (Temperature):**
```json
{
  "deviceId": "esp32_001",
  "location": "bus_stop_01",
  "temperature": 28.5,
  "humidity": 65.0,
  "timestamp": 1634567890
}
```

**Publish Example:**
```bash
mosquitto_pub -h localhost \
  -t "nus-smartstop/sensors/ultrasonic" \
  -m '{"deviceId":"esp32_001","location":"bus_stop_01","distance":125.5,"timestamp":1634567890}'
```

---

### Camera Event Topics

**Topic:** `nus-smartstop/camera`

**Payload Example:**
```json
{
  "deviceId": "esp32_001",
  "location": "bus_stop_01",
  "event_type": "capture",
  "image_count": 1,
  "timestamp": 1634567890
}
```

**Publish Example:**
```bash
mosquitto_pub -h localhost \
  -t "nus-smartstop/camera" \
  -m '{"deviceId":"esp32_001","location":"bus_stop_01","event_type":"capture","image_count":1}'
```

---

### Command Topics

**Topic:** `nus-smartstop/command/{device_id}`

**Payload Examples:**

Play beep sound:
```
BEEP
```

Capture image:
```
CAPTURE
```

Custom command:
```json
{
  "command": "set_interval",
  "value": 60
}
```

**Publish Example:**
```bash
# Simple command
mosquitto_pub -h localhost -t "nus-smartstop/command/esp32_001" -m "BEEP"

# JSON command
mosquitto_pub -h localhost \
  -t "nus-smartstop/command/esp32_001" \
  -m '{"command":"set_interval","value":60}'
```

---

### Subscribe to Topics

**Subscribe to all topics:**
```bash
mosquitto_sub -h localhost -t "#" -v
```

**Subscribe to all nus-smartstop topics:**
```bash
mosquitto_sub -h localhost -t "nus-smartstop/#" -v
```

**Subscribe to sensor topics only:**
```bash
mosquitto_sub -h localhost -t "nus-smartstop/sensors/#" -v
```

---

## InfluxDB Query Examples

### Using Flux Query Language

**Get last hour of ultrasonic sensor data:**
```flux
from(bucket: "sensor_data")
  |> range(start: -1h)
  |> filter(fn: (r) => r._measurement == "ultrasonic")
  |> filter(fn: (r) => r._field == "distance")
```

**Get data from specific device:**
```flux
from(bucket: "sensor_data")
  |> range(start: -24h)
  |> filter(fn: (r) => r._measurement == "ultrasonic")
  |> filter(fn: (r) => r.device_id == "esp32_001")
```

**Calculate average distance:**
```flux
from(bucket: "sensor_data")
  |> range(start: -1h)
  |> filter(fn: (r) => r._measurement == "ultrasonic")
  |> mean()
```

### Using Python Client

```python
from server.influxdb.client import InfluxDBHandler

handler = InfluxDBHandler()

# Query data
data = handler.query_sensor_data(
    measurement='ultrasonic',
    start_time='-1h',
    stop_time='now()'
)

print(data)
```

---

## Status Codes

- `200 OK`: Success
- `400 Bad Request`: Invalid request parameters
- `404 Not Found`: Resource not found
- `500 Internal Server Error`: Server error

---

## Rate Limiting

Currently not implemented. For production:
- Implement rate limiting per device
- Suggested: 10 requests per minute for image upload
- Suggested: 100 requests per minute for sensor data

---

## Best Practices

1. **Device ID Format**: Use consistent format like `esp32_001`, `esp32_002`
2. **Timestamps**: Use Unix timestamp in milliseconds
3. **Image Names**: Let server generate names with timestamps
4. **Error Handling**: Always check response status codes
5. **MQTT QoS**: Use QoS 1 for critical messages, QoS 0 for frequent sensor data

---

## Testing Tools

### HTTP Testing
- cURL
- Postman
- HTTPie
- Python requests

### MQTT Testing
- mosquitto_pub / mosquitto_sub
- MQTT Explorer
- MQTT.fx
- Python paho-mqtt

### Example Test Script

```python
import requests
import json

# Test health endpoint
response = requests.get('http://localhost:5000/health')
print(response.json())

# Test image upload
files = {'image': open('test.jpg', 'rb')}
data = {'device_id': 'esp32_001', 'location': 'bus_stop_01'}
response = requests.post('http://localhost:5000/api/upload', files=files, data=data)
print(response.json())

# Test inference
response = requests.post(
    'http://localhost:5000/api/inference',
    json={'filename': 'test.jpg'}
)
print(response.json())
```
