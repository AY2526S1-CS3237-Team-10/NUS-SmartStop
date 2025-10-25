"""
MQTT Client for Smart Bus Stop (DEPRECATED)
============================================

NOTE: This custom MQTT client is DEPRECATED. 
Use Telegraf instead for MQTT to InfluxDB bridging.

Telegraf provides:
- Native MQTT consumer support
- Direct InfluxDB v2 output
- Better performance and reliability
- Built-in retry and buffering

See telegraf.conf for configuration.

This file is kept for reference or custom processing needs.
"""

import os
import json
import paho.mqtt.client as mqtt
from datetime import datetime
from dotenv import load_dotenv
import logging

# Load environment variables
load_dotenv()

# Setup logging
logging.basicConfig(level=logging.INFO)
logger = logging.getLogger(__name__)


class MQTTHandler:
    """
    DEPRECATED: Use Telegraf for MQTT to InfluxDB bridging.
    
    This handler is kept for custom processing or debugging purposes only.
    """
    
    def __init__(self):
        self.broker = os.getenv('MQTT_BROKER', 'localhost')
        self.port = int(os.getenv('MQTT_PORT', 1883))
        self.topic_sensors = os.getenv('MQTT_TOPIC_SENSORS', 'smartstop/sensors')
        self.topic_camera = os.getenv('MQTT_TOPIC_CAMERA', 'smartstop/camera')
        
        logger.warning("This MQTT client is DEPRECATED. Use Telegraf instead.")
        
        # Initialize MQTT client
        self.client = mqtt.Client(client_id='smartstop_debug_client')
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
                logger.info(f"Parsed data: {data}")
            except json.JSONDecodeError:
                logger.error(f"Invalid JSON payload: {payload}")
                return
            
            # Custom processing can be added here
            # Telegraf handles writing to InfluxDB
            
        except Exception as e:
            logger.error(f"Error processing message: {str(e)}")
    
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
        logger.info("MQTT handler stopped")


if __name__ == '__main__':
    logger.warning("=" * 70)
    logger.warning("DEPRECATED: Use Telegraf for MQTT to InfluxDB bridging")
    logger.warning("This is a debug/monitoring client only")
    logger.warning("=" * 70)
    
    handler = MQTTHandler()
    handler.connect()
    handler.start()
