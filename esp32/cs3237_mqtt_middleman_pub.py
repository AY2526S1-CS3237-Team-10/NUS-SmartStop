import paho.mqtt.client as mqtt
from influxdb_client import InfluxDBClient, Point
from influxdb_client.client.write_api import SYNCHRONOUS
import json

# InfluxDB Config
INFLUXDB_URL = "http://157.230.250.226:8086/"
INFLUXDB_TOKEN = "Q-6kOBwftjXd0JPStBAXelHFc52COUnJVfJqXokMv8MH5yVRmhuuHYkG07hudLjsEZmEdJWnJHV8FzZV7f6Q2w=="
INFLUXDB_ORG = "NUS SmartStop"
INFLUXDB_BUCKET = "sensor_data"

# MQTT Config
MQTT_BROKER = "157.230.250.226"
MQTT_PORT = 1883
MQTT_TOPIC = "nus-smartstop/ir-sensor/data"

# Initialize InfluxDB client
influx_client = InfluxDBClient(url=INFLUXDB_URL, token=INFLUXDB_TOKEN, org=INFLUXDB_ORG)
write_api = influx_client.write_api(write_options=SYNCHRONOUS)

def on_connect(client, userdata, flags, rc):
    print(f"Connected to MQTT broker with result code {rc}")
    client.subscribe(MQTT_TOPIC)

def on_message(client, userdata, msg):
    try:
        # Parse JSON payload
        data = json.loads(msg.payload.decode())
        people_count = data.get("people_count")
        location = data.get("location", "unknown")
        
        # Create InfluxDB point
        point = Point("ir_sensor") \
            .tag("location", location) \
            .field("people_count", float(people_count))
        
        # Write to InfluxDB
        write_api.write(bucket=INFLUXDB_BUCKET, org=INFLUXDB_ORG, record=point)
        print(f"Written to InfluxDB: {data}")
        
    except Exception as e:
        print(f"Error: {e}")

# Setup MQTT client
mqtt_client = mqtt.Client()
mqtt_client.on_connect = on_connect
mqtt_client.on_message = on_message

mqtt_client.connect(MQTT_BROKER, MQTT_PORT, 60)
mqtt_client.loop_forever()