"""
Standalone inference script for bus stop capacity prediction.
Queries InfluxDB for latest sensor data and makes predictions using XGBoost model.
No Flask server required - can be run directly or called from other scripts.
"""

import os
import sys
import subprocess
import pandas as pd
from datetime import datetime

# Add parent directory to path to import BusStopPredictor
# sys.path.append(os.path.join(os.path.dirname(__file__), '..'))

from predict import BusStopPredictor

def query_influxdb(measurement='smartstop', minutes=None, hours=None, window='5s', aggregation='last'):
    """
    Query InfluxDB for latest sensor data using query_influxdb.py script.
    
    Args:
        measurement: InfluxDB measurement name
        minutes: Time window in minutes to query
        hours: Time window in hours to query
        window: Aggregation window
        aggregation: Aggregation function (last, mean, etc.)
    
    Returns:
        pandas DataFrame with sensor data
    """
    print(f"   Measurement: {measurement}")
    if minutes:
        print(f"   Time window: {minutes} minute(s)")
    elif hours:
        print(f"   Time window: {hours} hour(s)")
    print(f"   Aggregation: {aggregation} per {window}")
    
    # Build command to call query_influxdb.py
    script_path = os.path.join(os.path.dirname(__file__), 'query_influxdb.py')
    output_file = 'influx_latest.csv'
    
    # Delete old CSV file to avoid reading stale data
    if os.path.exists(output_file):
        try:
            os.remove(output_file)
            print(f"   Cleaned old cache file")
        except:
            pass
    
    cmd = [
        sys.executable,  # Use the same Python interpreter that's running this script
        script_path,
        '--measurement', measurement,
    ]
    
    if minutes:
        cmd.extend(['--minutes', str(minutes)])
    elif hours:
        cmd.extend(['--hours', str(hours)])
    
    cmd.extend([
        '--window', window,
        '--aggregation', aggregation,
        '--output', output_file
    ])
    
    try:
        # Run query script
        result = subprocess.run(cmd, capture_output=True, text=True, check=True)
        
        if result.returncode != 0:
            print(f"[ERROR] InfluxDB query error: {result.stderr}")
            return None
        
        # Check if query created a new file (means data was found)
        if not os.path.exists(output_file):
            print(f"[ERROR] No data found - output file not created")
            return None
        
        # Read the output CSV
        df = pd.read_csv(output_file)
        
        # Double-check it's not empty
        if len(df) == 0:
            print(f"[ERROR] Query returned empty dataset")
            return None
            
        print(f"[OK] Retrieved {len(df)} records from InfluxDB")
        return df
    
    except subprocess.CalledProcessError as e:
        print(f"[ERROR] Query failed: {e}")
        return None
    except Exception as e:
        print(f"[ERROR] Error querying InfluxDB: {e}")
        return None


def clean_and_extract_latest(df):
    """
    Clean and extract the latest sensor reading from InfluxDB data.
    Merges data from multiple devices (camera, sensors) within the last time window.
    
    Args:
        df: pandas DataFrame from InfluxDB query
    
    Returns:
        dict with sensor data ready for prediction
    """
    if df is None or len(df) == 0:
        print("[ERROR] No data to clean")
        return None
    
    try:
        # Convert crowd_level from string format to numeric
        if 'crowd_level' in df.columns:
            def convert_crowd_level(val):
                if pd.isna(val) or val == '' or val == 'None':
                    return -1
                if isinstance(val, str):
                    # Handle "0_empty", "1_low", etc.
                    if '_' in val:
                        return int(val.split('_')[0])
                    # Handle just numbers
                    try:
                        return int(val)
                    except:
                        return -1
                return int(val) if not pd.isna(val) else -1
            
            df['crowd_level'] = df['crowd_level'].apply(convert_crowd_level)
        
        # Convert voice from string to numeric
        if 'voice' in df.columns:
            def convert_voice(val):
                if pd.isna(val) or val == '' or val == 'None':
                    return -1
                if isinstance(val, str):
                    val = val.lower()
                    if val == 'true' or val == '1':
                        return 1
                    elif val == 'false' or val == '0':
                        return 0
                    else:
                        return -1
                return int(val) if not pd.isna(val) else -1
            
            df['voice'] = df['voice'].apply(convert_voice)
        
        # Sort by timestamp and get latest
        if '_time' in df.columns:
            # Convert to datetime if not already (handle flexible formats)
            df['_time'] = pd.to_datetime(df['_time'], format='ISO8601', errors='coerce')
            df = df.sort_values('_time', ascending=False)
        
        # Instead of taking just the first row, merge all recent rows (last 60 seconds)
        if '_time' in df.columns:
            latest_time = df['_time'].iloc[0]
            time_threshold = latest_time - pd.Timedelta(seconds=60)
            recent_df = df[df['_time'] >= time_threshold]
            print(f"[INFO] Merging {len(recent_df)} rows from last 60 seconds")
        else:
            recent_df = df
        
        # Helper function to get the most recent non-null value for a field
        def get_latest_value(field_name, default=-1):
            if field_name not in recent_df.columns:
                return default
            
            # Iterate through rows from most recent to oldest
            for idx in recent_df.index:
                val = recent_df.loc[idx, field_name]
                
                # Skip NaN, None, empty strings, and -1 (missing data indicator)
                if pd.isna(val) or val == '' or val == 'None' or val == 'none' or val == -1:
                    continue
                
                # Found a valid value - check if it's numeric or string
                if pd.api.types.is_numeric_dtype(type(val)) or isinstance(val, (int, float)):
                    return float(val)  # Convert to Python float for consistency
                elif isinstance(val, str) and val.strip():
                    return val
            
            # No valid value found
            return default
        
        # Extract sensor data using ACTUAL InfluxDB column names
        # Merge from all devices in the recent time window
        sensor_data = {
            'density': get_latest_value('density'),
            'sensors_CENTER_distance': get_latest_value('sensors_CENTER_distance'),
            'sensors_CENTER_occupied': get_latest_value('sensors_CENTER_occupied'),
            'sensors_LEFT_distance': get_latest_value('sensors_LEFT_distance'),
            'sensors_LEFT_occupied': get_latest_value('sensors_LEFT_occupied'),
            'sensors_RIGHT_distance': get_latest_value('sensors_RIGHT_distance'),
            'sensors_RIGHT_occupied': get_latest_value('sensors_RIGHT_occupied'),
            'people_count': get_latest_value('people_count'),
            'voice': get_latest_value('voice'),
            'crowd_level': get_latest_value('crowd_level'),
            'confidence': get_latest_value('confidence')
        }
        
        # Add timestamp for reference (use the most recent)
        if '_time' in df.columns:
            sensor_data['timestamp'] = df['_time'].iloc[0]
        
        return sensor_data
    
    except Exception as e:
        print(f"[ERROR] Error cleaning data: {e}")
        import traceback
        traceback.print_exc()
        return None


def main():
    """
    Main execution function.
    Returns dict with prediction results.
    """
    print("=" * 70)
    print("BUS STOP CAPACITY PREDICTION - STANDALONE")
    print("=" * 70)
    print()
    
    # Model paths
    model_path = os.path.join(os.path.dirname(__file__), 'busstop_xgboost.json')
    scaler_path = os.path.join(os.path.dirname(__file__), 'busstop_xgboost_scaler.pkl')
    
    # Step 1: Load model
    print("Step 1: Loading XGBoost model...")
    if not os.path.exists(model_path):
        print(f"[ERROR] Model file not found: {model_path}")
        print("  Please train the model first: python train_xgboost.py synthetic_busstop_data.csv")
        return {"success": False, "error": "Model not found", "capacity": -1}
    
    try:
        predictor = BusStopPredictor(model_path=model_path, scaler_path=scaler_path)
        print("[OK] Model loaded successfully")
    except Exception as e:
        print(f"[ERROR] Error loading model: {e}")
        return {"success": False, "error": f"Model loading failed: {str(e)}", "capacity": -1}
    
    print()
    
    # Step 2: Query InfluxDB
    print("Step 2: Querying InfluxDB for latest data...")
    df = query_influxdb(
        measurement='smartstop',
        minutes=1,
        #hours=1,
        window='5s',
        aggregation='last'
    )
    
    if df is None or df.empty:
        print("[ERROR] No data found in the specified time window")
        return {"success": False, "error": "No data in time window", "capacity": -1}
    
    print()
    
    # Step 3: Clean and extract latest
    print("Step 3: Processing sensor data...")
    sensor_data = clean_and_extract_latest(df)
    
    if sensor_data is None:
        print("[ERROR] Failed to extract sensor data")
        return {"success": False, "error": "Data extraction failed", "capacity": -1}
    
    print(f"[OK] Latest reading from: {sensor_data.get('timestamp', 'Unknown time')}")
    
    # Debug: Print sensor data
    print("\nSensor data extracted:")
    for key, val in sensor_data.items():
        if key != 'timestamp':
            print(f"  {key}: {val}")
    
    print()
    
    # Step 4: Make prediction
    print("Step 4: Generating capacity prediction...")
    try:
        capacity = predictor.predict(sensor_data)
        print("[OK] Prediction complete")
    except Exception as e:
        print(f"[ERROR] Prediction failed: {e}")
        return {"success": False, "error": f"Prediction failed: {str(e)}", "capacity": -1}
    
    # Step 5: Format results
    print()
    print("PREDICTION RESULTS")
    
    # Calculate occupancy percentage (max capacity = 30)
    max_capacity = 30
    occupancy_percent = (capacity / max_capacity) * 100
    
    # Determine occupancy status
    if occupancy_percent < 25:
        status = "EMPTY (Plenty of space)"
        emoji = "[GREEN]"
    elif occupancy_percent < 50:
        status = "LOW (Comfortable)"
        emoji = "[YELLOW]"
    elif occupancy_percent < 75:
        status = "MODERATE (Getting busy)"
        emoji = "[ORANGE]"
    else:
        status = "HIGH (Very crowded)"
        emoji = "[RED]"
    
    print(f"\nTimestamp:          {sensor_data.get('timestamp', 'N/A')}")
    print(f"Predicted Capacity: {capacity:.1f} people")
    print(f"Occupancy:          {occupancy_percent:.1f}%")
    print(f"Status:             {emoji} {status}")
    print()
    print("=" * 70)
    
    # Return structured result
    result = {
        "success": True,
        "timestamp": sensor_data.get('timestamp', datetime.now().isoformat()),
        "capacity": round(capacity, 2),
        "occupancy_percent": round(occupancy_percent, 2),
        "status": status,
        "sensor_data": sensor_data
    }
    
    return result


if __name__ == "__main__":
    try:
        result = main()
        
        # Exit with appropriate code
        if result.get("success", False):
            sys.exit(0)
        else:
            print(f"\n[ERROR] {result.get('error', 'Unknown error')}")
            sys.exit(1)
    
    except KeyboardInterrupt:
        print("\n\n[INFO] Interrupted by user")
        sys.exit(1)
    
    except Exception as e:
        print(f"\n[ERROR] Unexpected error: {e}")
        sys.exit(1)
