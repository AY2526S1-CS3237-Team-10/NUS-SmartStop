"""
Simple InfluxDB Query Script
Pull sensor data from InfluxDB for analysis

Usage:
    python query_influxdb.py --hours 24
    python query_influxdb.py --days 7
"""

from influxdb_client import InfluxDBClient
import pandas as pd
from datetime import datetime
import argparse
import warnings
from influxdb_client.client.warnings import MissingPivotFunction

# Suppress the pivot warning
warnings.simplefilter("ignore", MissingPivotFunction)


# InfluxDB Configuration (InfluxDB 2.x)
INFLUXDB_URL = "" # Replace with your InfluxDB URL
INFLUXDB_TOKEN = ""  # Replace with your actual token
INFLUXDB_ORG = "" # Replace with your organization name
INFLUXDB_BUCKET = "" # Replace with your bucket name
DEFAULT_MEASUREMENT = "mqtt_consumer"  # Default measurement name


def test_connection():
    """Test if we can connect to InfluxDB (helps find the right config)"""
    print("=" * 70)
    print("[INFO] TESTING INFLUXDB CONNECTION")
    print("=" * 70)
    print(f"[SERVER] Server: {INFLUXDB_URL}")
    print()
    
    # Try different connection methods
    
    # Method 1: Try with token (if provided)
    if INFLUXDB_TOKEN:
        print("Trying token-based authentication (InfluxDB 2.x)...")
        try:
            client = InfluxDBClient(
                url=INFLUXDB_URL,
                token=INFLUXDB_TOKEN,
                org=INFLUXDB_ORG
            )
            # Try to list buckets
            buckets_api = client.buckets_api()
            buckets = buckets_api.find_buckets()
            print("[OK] Token authentication works!")
            print(f"[BUCKET] Available buckets:")
            for bucket in buckets.buckets:
                print(f"   - {bucket.name}")
            client.close()
            return True
        except Exception as e:
            print(f"[ERROR] Token auth failed: {e}")
    
    # Method 2: Try without auth (open access)
    print("\nTrying without authentication (open access)...")
    try:
        client = InfluxDBClient(url=INFLUXDB_URL, token="", org="")
        query_api = client.query_api()
        # Simple test query
        result = query_api.query('buckets()')
        print("[OK] Open access works!")
        client.close()
        return True
    except Exception as e:
        print(f"[ERROR] Open access failed: {e}")
    
    print("\n[WARNING]  Could not connect. You may need:")
    print("   1. Token (for InfluxDB 2.x)")
    print("   2. Username/Password (for InfluxDB 1.x)")
    print("   3. Check if server is accessible")
    return False


def query_data(time_range="-24h", fields=None, device_id=None, window="5m", aggregation="median", measurement=None):
    """
    Query data from InfluxDB
    
    Args:
        time_range: Time range string (e.g., "-24h", "-7d", "-30d")
        fields: List of specific fields to query (default: all fields)
        device_id: Filter by specific device ID (e.g., "esp32-smartstop-01")
        window: Aggregation window period (e.g., "5s", "1m", "5m", "1h")
        aggregation: Aggregation function ("mean", "median", "max", "min", "sum", "last", "first")
        measurement: Measurement name (default: mqtt_consumer, or use "smartstop" etc.)
    
    Returns:
        pandas DataFrame with the data
    """
    # Use default measurement if not specified
    if measurement is None:
        measurement = DEFAULT_MEASUREMENT
    
    print("=" * 70)
    print("[QUERY] QUERYING INFLUXDB")
    print("=" * 70)
    print(f"[SERVER] Server: {INFLUXDB_URL}")
    print(f"[BUCKET] Bucket: {INFLUXDB_BUCKET}")
    print(f"[MEASUREMENT] Measurement: {measurement}")
    print(f"[TIME] Time Range: {time_range}")
    print(f"[QUERY] Window: {window}")
    print(f"[AGGREGATION] Aggregation: {aggregation}")
    if fields:
        print(f"[FIELDS]  Fields: {', '.join(fields)}")
    if device_id:
        print(f"[DEVICE] Device ID: {device_id}")
    print()
    
    # Create InfluxDB client
    client = InfluxDBClient(
        url=INFLUXDB_URL,
        token=INFLUXDB_TOKEN,
        org=INFLUXDB_ORG
    )
    
    # Query API
    query_api = client.query_api()
    
    # First, let's see what fields exist in the measurement
    print(f"[INFO] Checking available fields in {measurement}...")
    
    fields_query = f'''
    import "influxdata/influxdb/schema"
    schema.measurementFieldKeys(
      bucket: "{INFLUXDB_BUCKET}",
      measurement: "{measurement}"
    )
    '''
    
    try:
        fields_result = query_api.query(fields_query, org=INFLUXDB_ORG)
        print(f"[LIST] Available fields in {measurement}:")
        for table in fields_result:
            for record in table.records:
                print(f"   - {record.get_value()}")
        print()
    except Exception as e:
        print(f"[WARNING]  Could not list fields: {e}")
    
    # Build query with optional field filter
    if fields:
        # Create field filter
        field_filter = ' or '.join([f'r["_field"] == "{field}"' for field in fields])
        
        # Add device ID filter if specified
        device_filter = f'|> filter(fn: (r) => r["deviceId"] == "{device_id}")' if device_id else ''
        
        query = f'''
        from(bucket: "{INFLUXDB_BUCKET}")
          |> range(start: {time_range})
          |> filter(fn: (r) => r["_measurement"] == "{measurement}")
          |> filter(fn: (r) => {field_filter})
          {device_filter}
          |> aggregateWindow(every: {window}, fn: {aggregation}, createEmpty: false)
          |> pivot(rowKey:["_time"], columnKey: ["_field"], valueColumn: "_value")
        '''
    else:
        # Add device ID filter if specified
        device_filter = f'|> filter(fn: (r) => r["deviceId"] == "{device_id}")' if device_id else ''
        
        # Flux query to get measurement data with all fields
        query = f'''
        from(bucket: "{INFLUXDB_BUCKET}")
          |> range(start: {time_range})
          |> filter(fn: (r) => r["_measurement"] == "{measurement}")
          {device_filter}
          |> aggregateWindow(every: {window}, fn: {aggregation}, createEmpty: false)
          |> pivot(rowKey:["_time"], columnKey: ["_field"], valueColumn: "_value")
        '''
    
    print("[INFO] Executing query...")
    
    try:
        # Execute query and convert to DataFrame
        result = query_api.query_data_frame(query)
        
        if isinstance(result, list):
            if len(result) == 0:
                print("[ERROR] No data found!")
                return pd.DataFrame()
            result = pd.concat(result, ignore_index=True)
        
        if result.empty:
            print("[ERROR] No data found!")
            return result
        
        print(f"[OK] Retrieved {len(result)} records")
        print()
        
        # Display column names
        print("[LIST] Available columns:")
        for col in result.columns:
            print(f"   - {col}")
        print()
        
        # Display summary
        print("[AGGREGATION] Data Summary:")
        print(result.head(10))
        print()
        
        # Close client
        client.close()
        
        return result
        
    except Exception as e:
        print(f"[ERROR] Error querying data: {e}")
        import traceback
        traceback.print_exc()
        client.close()
        return pd.DataFrame()


def query_specific_measurement(measurement_name, field_name=None, time_range="-24h", window="5m", aggregation="median"):
    """
    Query specific measurement and field
    
    Args:
        measurement_name: Name of measurement (e.g., "sensor_data", "image_analysis")
        field_name: Specific field to query (optional)
        time_range: Time range string
        window: Aggregation window period (e.g., "5s", "1m", "5m", "1h")
        aggregation: Aggregation function ("mean", "median", "max", "min", "sum")
    
    Returns:
        pandas DataFrame
    """
    print("=" * 70)
    print("[QUERY] QUERYING SPECIFIC DATA")
    print("=" * 70)
    print(f"[MEASUREMENT] Measurement: {measurement_name}")
    if field_name:
        print(f"[FIELDS]  Field: {field_name}")
    print(f"[TIME] Time Range: {time_range}")
    print(f"[QUERY] Window: {window}")
    print(f"[AGGREGATION] Aggregation: {aggregation}")
    print()
    
    client = InfluxDBClient(
        url=INFLUXDB_URL,
        token=INFLUXDB_TOKEN,
        org=INFLUXDB_ORG
    )
    
    query_api = client.query_api()
    
    # Build query
    if field_name:
        query = f'''
        from(bucket: "{INFLUXDB_BUCKET}")
          |> range(start: {time_range})
          |> filter(fn: (r) => r["_measurement"] == "{measurement_name}")
          |> filter(fn: (r) => r["_field"] == "{field_name}")
          |> aggregateWindow(every: {window}, fn: {aggregation}, createEmpty: false)
        '''
    else:
        query = f'''
        from(bucket: "{INFLUXDB_BUCKET}")
          |> range(start: {time_range})
          |> filter(fn: (r) => r["_measurement"] == "{measurement_name}")
          |> aggregateWindow(every: {window}, fn: {aggregation}, createEmpty: false)
        '''
    
    print("[INFO] Executing query...")
    
    try:
        result = query_api.query_data_frame(query)
        
        if isinstance(result, list):
            if len(result) == 0:
                print("[ERROR] No data found!")
                return pd.DataFrame()
            result = pd.concat(result, ignore_index=True)
        
        if not result.empty:
            print(f"[OK] Retrieved {len(result)} records")
            print()
            print("[AGGREGATION] First 10 records:")
            print(result[['_time', '_field', '_value']].head(10))
            print()
        else:
            print("[ERROR] No data found!")
        
        client.close()
        return result
        
    except Exception as e:
        print(f"[ERROR] Error: {e}")
        import traceback
        traceback.print_exc()
        client.close()
        return pd.DataFrame()


def save_to_csv(df, filename="influxdb_data.csv"):
    """Save DataFrame to CSV file"""
    if df.empty:
        print("[WARNING]  No data to save")
        return
    
    # If file exists and can't be opened, try with timestamp
    import os
    from datetime import datetime
    
    try:
        df.to_csv(filename, index=False)
        print(f"[SAVE] Data saved to: {filename}")
        print()
    except PermissionError:
        # File is open, create new one with timestamp
        timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
        base_name = filename.replace('.csv', '')
        new_filename = f"{base_name}_{timestamp}.csv"
        df.to_csv(new_filename, index=False)
        print(f"[WARNING]  Original file was locked, saved to: {new_filename}")
        print()


def main():
    parser = argparse.ArgumentParser(description='Query data from InfluxDB')
    parser.add_argument('--test', action='store_true', help='Test connection only')
    parser.add_argument('--min', '--minutes', type=int, dest='minutes', help='Query last N minutes')
    parser.add_argument('--hours', type=int, help='Query last N hours')
    parser.add_argument('--days', type=int, help='Query last N days')
    parser.add_argument('--measurement', type=str, 
                       help='Measurement name (default: mqtt_consumer). Examples: smartstop, sensor_data')
    parser.add_argument('--field', type=str, help='Specific field name')
    parser.add_argument('--fields', type=str, nargs='+', help='Multiple specific fields (space-separated)')
    parser.add_argument('--device-id', type=str, 
                       help='Filter by device ID (e.g., esp32-smartstop-01, ESP32-CAM)')
    parser.add_argument('--window', type=str, default='5m',
                       help='Aggregation window period (default: 5m). Examples: 5s, 30s, 1m, 5m, 1h')
    parser.add_argument('--aggregation', type=str, default='median',
                       choices=['mean', 'median', 'max', 'min', 'sum', 'first', 'last'],
                       help='Aggregation function (default: median)')
    parser.add_argument('--output', type=str, default='influxdb_data.csv', 
                       help='Output CSV filename')
    
    args = parser.parse_args()
    
    # Test connection first
    if args.test:
        test_connection()
        return 0
    
    # Determine time range
    if args.minutes:
        time_range = f"-{args.minutes}m"
    elif args.hours:
        time_range = f"-{args.hours}h"
    elif args.days:
        time_range = f"-{args.days}d"
    else:
        time_range = "-24h"  # Default: last 24 hours
    
    # Determine fields to query
    fields_to_query = None
    if args.fields:
        fields_to_query = args.fields
    elif args.field:
        fields_to_query = [args.field]
    
    # Query data
    # Use unified query_data function that supports both measurement and device_id
    df = query_data(time_range, fields=fields_to_query, device_id=args.device_id,
                   window=args.window, aggregation=args.aggregation, 
                   measurement=args.measurement)
    
    # Save to CSV
    if not df.empty:
        save_to_csv(df, args.output)
        
        print("=" * 70)
        print("[OK] QUERY COMPLETE")
        print("=" * 70)
    
    return 0


if __name__ == "__main__":
    exit(main())
