"""
MQTT Client for Smart Bus Stop
Subscribes to MQTT topics and writes sensor data to InfluxDB
"""

import os
import json
import sys
import paho.mqtt.client as mqtt
from datetime import datetime
from dotenv import load_dotenv
import logging

# Add parent directory to path for imports
sys.path.append(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from influxdb.client import InfluxDBHandler

# Load environment variables
load_dotenv()

# Setup logging
logging.basicConfig(level=logging.INFO)
logger = logging.getLogger(__name__)


class MQTTHandler:
    """Handler for MQTT operations"""
    
    def __init__(self):
        self.broker = os.getenv('MQTT_BROKER', 'localhost')
        self.port = int(os.getenv('MQTT_PORT', 1883))
        self.topic_sensors = os.getenv('MQTT_TOPIC_SENSORS', 'smartstop/sensors')
        self.topic_camera = os.getenv('MQTT_TOPIC_CAMERA', 'smartstop/camera')
        
        # Initialize InfluxDB handler
        try:
            self.influx = InfluxDBHandler()
        except Exception as e:
            logger.error(f"Failed to initialize InfluxDB handler: {str(e)}")
            self.influx = None
        
        # Initialize MQTT client
        self.client = mqtt.Client()
        self.client.on_connect = self.on_connect
        self.client.on_message = self.on_message
        self.client.on_disconnect = self.on_disconnect
    
    def on_connect(self, client, userdata, flags, rc):
        """Callback for when client connects to MQTT broker"""
        if rc == 0:
            logger.info(f"Connected to MQTT broker at {self.broker}:{self.port}")
            
            # Subscribe to topics
            client.subscribe(f"{self.topic_sensors}/#")
            client.subscribe(f"{self.topic_camera}/#")
            logger.info(f"Subscribed to topics: {self.topic_sensors}/# and {self.topic_camera}/#")
        else:
            logger.error(f"Failed to connect to MQTT broker, return code: {rc}")
    
    def on_disconnect(self, client, userdata, rc):
        """Callback for when client disconnects from MQTT broker"""
        if rc != 0:
            logger.warning(f"Unexpected MQTT disconnect, return code: {rc}")
        else:
            logger.info("Disconnected from MQTT broker")
    
    def on_message(self, client, userdata, msg):
        """Callback for when a message is received from MQTT broker"""
        try:
            topic = msg.topic
            payload = msg.payload.decode('utf-8')
            
            logger.info(f"Received message on topic '{topic}'")
            logger.debug(f"Payload: {payload}")
            
            # Parse JSON payload
            try:
                data = json.loads(payload)
            except json.JSONDecodeError:
                logger.error(f"Invalid JSON payload: {payload}")
                return
            
            # Process sensor data
            if topic.startswith(self.topic_sensors):
                self.process_sensor_data(topic, data)
            
            # Process camera data
            elif topic.startswith(self.topic_camera):
                self.process_camera_data(topic, data)
            
            else:
                logger.warning(f"Unknown topic: {topic}")
        
        except Exception as e:
            logger.error(f"Error processing message: {str(e)}")
    
    def process_sensor_data(self, topic, data):
        """Process and store sensor data"""
        try:
            # Extract sensor type from topic
            # Format: smartstop/sensors/{sensor_type}/{device_id}
            parts = topic.split('/')
            sensor_type = parts[2] if len(parts) > 2 else 'unknown'
            device_id = parts[3] if len(parts) > 3 else 'unknown'
            
            # Prepare tags and fields
            tags = {
                'device_id': data.get('device_id', device_id),
                'sensor_type': sensor_type,
                'location': data.get('location', 'unknown')
            }
            
            # Extract fields (all numeric values)
            fields = {}
            for key, value in data.items():
                if key not in ['device_id', 'location', 'timestamp']:
                    if isinstance(value, (int, float)):
                        fields[key] = value
            
            if fields and self.influx:
                # Write to InfluxDB
                self.influx.write_sensor_data(
                    measurement=sensor_type,
                    tags=tags,
                    fields=fields
                )
                logger.info(f"Stored {sensor_type} data from {device_id}")
        
        except Exception as e:
            logger.error(f"Error processing sensor data: {str(e)}")
    
    def process_camera_data(self, topic, data):
        """Process camera metadata (images handled by Flask server)"""
        try:
            logger.info(f"Camera event: {data}")
            
            # Store camera event metadata in InfluxDB
            if self.influx:
                tags = {
                    'device_id': data.get('device_id', 'unknown'),
                    'location': data.get('location', 'unknown')
                }
                
                fields = {
                    'event_type': data.get('event_type', 'capture'),
                    'image_count': data.get('image_count', 1)
                }
                
                self.influx.write_sensor_data(
                    measurement='camera_event',
                    tags=tags,
                    fields=fields
                )
        
        except Exception as e:
            logger.error(f"Error processing camera data: {str(e)}")
    
    def connect(self):
        """Connect to MQTT broker"""
        try:
            self.client.connect(self.broker, self.port, 60)
            logger.info("MQTT connection initiated")
        except Exception as e:
            logger.error(f"Failed to connect to MQTT broker: {str(e)}")
            raise
    
    def start(self):
        """Start MQTT client loop"""
        try:
            self.client.loop_forever()
        except KeyboardInterrupt:
            logger.info("MQTT client stopped by user")
            self.stop()
    
    def stop(self):
        """Stop MQTT client"""
        self.client.loop_stop()
        self.client.disconnect()
        if self.influx:
            self.influx.close()
        logger.info("MQTT handler stopped")


if __name__ == '__main__':
    handler = MQTTHandler()
    handler.connect()
    handler.start()
