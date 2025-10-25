"""
InfluxDB Client for Smart Bus Stop
Handles writing sensor data from MQTT to InfluxDB
"""

import os
from datetime import datetime
from influxdb_client import InfluxDBClient, Point, WritePrecision
from influxdb_client.client.write_api import SYNCHRONOUS
from dotenv import load_dotenv
import logging

# Load environment variables
load_dotenv()

# Setup logging
logging.basicConfig(level=logging.INFO)
logger = logging.getLogger(__name__)


class InfluxDBHandler:
    """Handler for InfluxDB operations"""
    
    def __init__(self):
        self.url = os.getenv('INFLUXDB_URL', 'http://localhost:8086')
        self.token = os.getenv('INFLUXDB_TOKEN')
        self.org = os.getenv('INFLUXDB_ORG', 'smartstop')
        self.bucket = os.getenv('INFLUXDB_BUCKET', 'sensor_data')
        
        try:
            self.client = InfluxDBClient(url=self.url, token=self.token, org=self.org)
            self.write_api = self.client.write_api(write_options=SYNCHRONOUS)
            self.query_api = self.client.query_api()
            logger.info(f"Connected to InfluxDB at {self.url}")
        except Exception as e:
            logger.error(f"Failed to connect to InfluxDB: {str(e)}")
            raise
    
    def write_sensor_data(self, measurement, tags, fields, timestamp=None):
        """
        Write sensor data to InfluxDB
        
        Args:
            measurement (str): Measurement name (e.g., 'ultrasonic', 'temperature')
            tags (dict): Tags for the data point (e.g., {'sensor_id': 'us1', 'location': 'front'})
            fields (dict): Field values (e.g., {'distance': 123.45})
            timestamp (datetime): Optional timestamp, defaults to now
        """
        try:
            point = Point(measurement)
            
            # Add tags
            for key, value in tags.items():
                point = point.tag(key, value)
            
            # Add fields
            for key, value in fields.items():
                point = point.field(key, value)
            
            # Set timestamp
            if timestamp:
                point = point.time(timestamp, WritePrecision.NS)
            else:
                point = point.time(datetime.utcnow(), WritePrecision.NS)
            
            self.write_api.write(bucket=self.bucket, org=self.org, record=point)
            logger.debug(f"Written to InfluxDB: {measurement}")
            
        except Exception as e:
            logger.error(f"Error writing to InfluxDB: {str(e)}")
            raise
    
    def query_sensor_data(self, measurement, start_time='-1h', stop_time='now()'):
        """
        Query sensor data from InfluxDB
        
        Args:
            measurement (str): Measurement name
            start_time (str): Start time (e.g., '-1h', '-30m')
            stop_time (str): Stop time (default: 'now()')
        
        Returns:
            list: Query results
        """
        try:
            query = f'''
            from(bucket: "{self.bucket}")
                |> range(start: {start_time}, stop: {stop_time})
                |> filter(fn: (r) => r._measurement == "{measurement}")
            '''
            
            result = self.query_api.query(query=query, org=self.org)
            
            data = []
            for table in result:
                for record in table.records:
                    data.append({
                        'time': record.get_time(),
                        'measurement': record.get_measurement(),
                        'field': record.get_field(),
                        'value': record.get_value(),
                        'tags': {k: v for k, v in record.values.items() if k not in ['_time', '_measurement', '_field', '_value', 'result', 'table']}
                    })
            
            return data
        
        except Exception as e:
            logger.error(f"Error querying InfluxDB: {str(e)}")
            raise
    
    def close(self):
        """Close InfluxDB client"""
        if self.client:
            self.client.close()
            logger.info("InfluxDB connection closed")


if __name__ == '__main__':
    # Test connection
    try:
        handler = InfluxDBHandler()
        
        # Test write
        handler.write_sensor_data(
            measurement='test',
            tags={'sensor': 'test_sensor'},
            fields={'value': 42.0}
        )
        logger.info("Test write successful")
        
        # Test query
        data = handler.query_sensor_data('test', start_time='-5m')
        logger.info(f"Test query returned {len(data)} records")
        
        handler.close()
        
    except Exception as e:
        logger.error(f"Test failed: {str(e)}")
