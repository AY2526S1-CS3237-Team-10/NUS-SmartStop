"""
Data Preparation for XGBoost Model
Cleans and prepares sensor data for bus stop capacity prediction

Expected input CSV columns from InfluxDB:
- _time: timestamp
- sensors_CENTER_distance: Ultrasonic CENTER sensor distance (cm)
- sensors_CENTER_occupied: Ultrasonic CENTER occupied (0/1)
- sensors_LEFT_distance: Ultrasonic LEFT sensor distance (cm)
- sensors_LEFT_occupied: Ultrasonic LEFT occupied (0/1)
- sensors_RIGHT_distance: Ultrasonic RIGHT sensor distance (cm)
- sensors_RIGHT_occupied: Ultrasonic RIGHT occupied (0/1)
- people_count: IR sensor people count (float)
- voice: Microphone voice detection ("true"/"false" or 0/1)
- crowd_level: ESP32 camera crowd level (0/1/2 or "empty"/"medium"/"full")
- confidence: ESP32 camera confidence score (0-1 float)
- density: People density metric (float)

Columns to OMIT (not used for training):
- humidity: Weather data (excluded)
- temperature: Weather data (excluded)

You need to add:
- actual_capacity: Ground truth bus stop capacity (target variable)
"""

import pandas as pd
import numpy as np
import argparse
from datetime import datetime
import pytz


def load_data(filepath):
    """Load CSV data from InfluxDB query"""
    print("=" * 70)
    print("📂 LOADING DATA")
    print("=" * 70)
    print(f"File: {filepath}")
    
    # Try to load with timestamp as index
    try:
        df = pd.read_csv(filepath, index_col=0, parse_dates=True)
        print(f"✅ Loaded {len(df)} rows with timestamp index")
    except:
        df = pd.read_csv(filepath)
        print(f"✅ Loaded {len(df)} rows")
    
    print(f"📋 Columns: {list(df.columns)}")
    print()
    
    return df


def clean_data(df):
    """Remove unnecessary columns and handle missing values"""
    print("=" * 70)
    print("🧹 CLEANING DATA")
    print("=" * 70)
    
    # Columns to remove (metadata from InfluxDB + excluded sensors)
    cols_to_remove = ['result', 'table', '_start', '_stop', '_measurement', 
                     'host', 'topic', 'deviceId', 'humidity', 'temperature']
    
    removed = []
    for col in cols_to_remove:
        if col in df.columns:
            df = df.drop(columns=[col])
            removed.append(col)
    
    if removed:
        print(f"🗑️  Removed columns: {removed}")
    
    # Convert voice column from "true"/"false" strings to 1/0
    if 'voice' in df.columns:
        print(f"\n🔄 Converting 'voice' column...")
        if df['voice'].dtype == 'object':
            df['voice'] = df['voice'].map({'true': 1, 'True': 1, 'false': 0, 'False': 0, '': -1})
            df['voice'] = df['voice'].fillna(-1).astype(int)
            print(f"   ✅ Converted voice (true/false → 1/0, empty → -1)")
    
    # Convert crowd_level to numeric if needed
    if 'crowd_level' in df.columns:
        print(f"\n🔄 Converting 'crowd_level' column...")
        if df['crowd_level'].dtype == 'object':
            # Map text values to numbers (handles both "0_empty" and "empty" formats)
            crowd_map = {
                # Format: "0_empty", "1_low", "2_medium", "3_high"
                '0_empty': 0, '0_Empty': 0, '0_EMPTY': 0,
                '1_low': 1, '1_Low': 1, '1_LOW': 1,
                '2_medium': 2, '2_Medium': 2, '2_MEDIUM': 2,
                '3_high': 3, '3_High': 3, '3_HIGH': 3,
                '4_full': 4, '4_Full': 4, '4_FULL': 4,
                # Format: "empty", "low", "medium", "high" (fallback)
                'empty': 0, 'Empty': 0, 'EMPTY': 0,
                'low': 1, 'Low': 1, 'LOW': 1,
                'medium': 2, 'Medium': 2, 'MEDIUM': 2,
                'high': 3, 'High': 3, 'HIGH': 3,
                'full': 4, 'Full': 4, 'FULL': 4,
                '': -1
            }
            df['crowd_level'] = df['crowd_level'].map(crowd_map)
            print(f"   ✅ Converted crowd_level (text → numeric)")
            
            # Show unique values for debugging
            unique_vals = sorted(df['crowd_level'].dropna().unique())
            print(f"   📊 Unique values: {unique_vals}")
    
    # Define sensor columns to check (include ALL sensor columns including occupied flags)
    sensor_cols = ['sensors_CENTER_distance', 'sensors_CENTER_occupied',
                   'sensors_LEFT_distance', 'sensors_LEFT_occupied', 
                   'sensors_RIGHT_distance', 'sensors_RIGHT_occupied',
                   'people_count', 'voice', 'crowd_level', 'confidence', 'density']
    sensor_cols_present = [col for col in sensor_cols if col in df.columns]
    
    # First, drop rows where ALL sensor columns are NaN (only weather data rows)
    before_drop = len(df)
    df = df.dropna(subset=sensor_cols_present, how='all')
    after_drop = len(df)
    
    if before_drop > after_drop:
        print(f"\n🗑️  Dropped {before_drop - after_drop} rows with ONLY weather data (no sensor data)")
    
    # Check for missing values
    missing = df.isnull().sum()
    if missing.sum() > 0:
        print(f"\n⚠️  Missing values found:")
        for col in missing[missing > 0].index:
            print(f"   - {col}: {missing[col]} ({missing[col]/len(df)*100:.1f}%)")
        
        print("\n🔄 Filling missing values with -1 (no data/timing)...")
        # Fill sensor values with -1 (means no data at this timestamp)
        # This distinguishes from 0 which could be a valid sensor reading
        for col in sensor_cols_present:
            if col in df.columns:
                df[col] = df[col].fillna(-1)
        
        # After filling with -1, drop rows where ALL sensors are -1 
        # (these had only humidity/temperature data before)
        print("\n🗑️  Removing rows where all sensor fields are -1...")
        # Check if all sensor columns are -1
        all_minus_one = (df[sensor_cols_present] == -1).all(axis=1)
        rows_to_drop = all_minus_one.sum()
        
        if rows_to_drop > 0:
            df = df[~all_minus_one]
            print(f"   ✅ Removed {rows_to_drop} rows with all sensors = -1")
        else:
            print("   ✅ No rows with all sensors = -1")
    else:
        print("✅ No missing values")
    
    print()
    return df


def convert_timezone(df, from_tz='UTC', to_tz='Asia/Singapore'):
    """Convert timestamp timezone"""
    
    if not isinstance(df.index, pd.DatetimeIndex):
        print("No datetime index found, skipping timezone conversion")
        print()
        return df
    
    print(f"Converting: {from_tz} → {to_tz}")
    
    # Check if already timezone-aware
    if df.index.tz is None:
        # Localize to UTC first
        df.index = df.index.tz_localize(from_tz)
    
    # Convert to target timezone
    df.index = df.index.tz_convert(to_tz)
    
    print(f"Converted to {to_tz}")
    print(f"   First timestamp: {df.index[0]}")
    print(f"   Last timestamp: {df.index[-1]}")
    print()
    
    return df


def add_time_features(df):
    """Add time-based features for better predictions"""
    print("=" * 70)
    print("ADDING TIME FEATURES")
    print("=" * 70)
    
    if not isinstance(df.index, pd.DatetimeIndex):
        print("No datetime index, skipping time features")
        print()
        return df
    
    # Extract time features
    df['hour'] = df.index.hour
    df['day_of_week'] = df.index.dayofweek  # 0=Monday, 6=Sunday
    df['is_weekend'] = (df.index.dayofweek >= 5).astype(int)
    
    # Business hours (7 AM - 7 PM on weekdays)
    df['is_business_hours'] = (
        (df['hour'] >= 7) & 
        (df['hour'] < 19) & 
        (df['day_of_week'] < 5)
    ).astype(int)
    
    # Rush hour (7-9 AM, 5-7 PM on weekdays)
    df['is_rush_hour'] = (
        ((df['hour'] >= 7) & (df['hour'] < 9)) | 
        ((df['hour'] >= 17) & (df['hour'] < 19))
    ) & (df['day_of_week'] < 5)
    df['is_rush_hour'] = df['is_rush_hour'].astype(int)
    
    print("Added time features:")
    print("   - hour (0-23)")
    print("   - day_of_week (0=Mon, 6=Sun)")
    print("   - is_weekend (0/1)")
    print("   - is_business_hours (0/1)")
    print("   - is_rush_hour (0/1)")
    print()
    
    return df


def validate_sensor_columns(df):
    """Check if required sensor columns exist"""
    
    # Required sensor columns (updated for your actual data)
    required = {
        'sensors_CENTER_distance': 'Ultrasonic CENTER distance (cm)',
        'sensors_CENTER_occupied': 'Ultrasonic CENTER occupied (0/1)',
        'sensors_LEFT_distance': 'Ultrasonic LEFT distance (cm)',
        'sensors_LEFT_occupied': 'Ultrasonic LEFT occupied (0/1)',
        'sensors_RIGHT_distance': 'Ultrasonic RIGHT distance (cm)',
        'sensors_RIGHT_occupied': 'Ultrasonic RIGHT occupied (0/1)',
        'people_count': 'IR sensor count (float)',
        'voice': 'Microphone voice (0/1)',
        'crowd_level': 'ESP32 camera crowd level (0-4)',
        'confidence': 'ESP32 camera confidence (0-1)',
        'density': 'People density (float)'
    }
    
    missing = []
    present = []
    
    for col, description in required.items():
        if col in df.columns:
            non_null = df[col].notna().sum()
            total = len(df)
            pct = (non_null / total * 100) if total > 0 else 0
            present.append(f"{col:30s} - {description:40s} ({non_null}/{total} = {pct:.1f}% filled)")
        else:
            missing.append(f"{col:30s} - {description}")
    
    if present:
        print("Present columns:")
        for line in present:
            print(f"   {line}")
    
    if missing:
        print("\nMISSING columns:")
        for line in missing:
            print(f"   {line}")
        print("\n   These columns are missing from your data!")
    
    # Warn about excluded columns
    excluded = ['humidity', 'temperature']
    excluded_present = [col for col in excluded if col not in df.columns]
    if excluded_present:
        print(f"\n✅ Excluded columns removed: {excluded}")
    
    print()
    
    # Check for target column
    if 'actual_capacity' in df.columns:
        print("Target column 'actual_capacity' found")
        non_null = df['actual_capacity'].notna().sum()
        if non_null > 0:
            print(f"   Samples: {non_null}/{len(df)}")
            print(f"   Min: {df['actual_capacity'].min():.2f}")
            print(f"   Max: {df['actual_capacity'].max():.2f}")
            print(f"   Mean: {df['actual_capacity'].mean():.2f}")
        else:
            print(" All values are NaN! You need to add actual capacity values.")
    else:
        print("Target column 'actual_capacity' NOT FOUND")
        print("   You need to add this column manually based on ground truth!")
        print("\nTip: You can create a temporary formula:")
        print("   actual_capacity = (people_count * 0.3) + (density * 0.5) + (crowd_level * 5)")
    
    print()


def save_prepared_data(df, output_file):
    """Save prepared data to CSV"""
    
    try:
        df.to_csv(output_file)
        print(f"Saved to: {output_file}")
        print(f"   Rows: {len(df)}")
        print(f"   Columns: {len(df.columns)}")
    except PermissionError:
        # File is locked, add timestamp
        timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
        new_file = output_file.replace('.csv', f'_{timestamp}.csv')
        df.to_csv(new_file)
        print(f"Original file locked, saved to: {new_file}")
    
    print()


def main():
    parser = argparse.ArgumentParser(
        description='Prepare sensor data for XGBoost training'
    )
    parser.add_argument('--input', type=str, default='influxdb_data.csv',
                       help='Input CSV file from InfluxDB query')
    parser.add_argument('--output', type=str, default='prepared_data_xgboost.csv',
                       help='Output CSV file for XGBoost training')
    parser.add_argument('--no-time-features', action='store_true',
                       help='Skip adding time-based features')
    parser.add_argument('--timezone', type=str, default='Asia/Singapore',
                       help='Target timezone (default: Asia/Singapore)')
    
    args = parser.parse_args()
    
    print("\n" + "=" * 70)
    print("DATA PREPARATION FOR XGBOOST")
    print("=" * 70)
    print()
    
    # 1. Load data
    df = load_data(args.input)
    
    # 2. Convert timezone
    df = convert_timezone(df, to_tz=args.timezone)
    
    # 3. Clean data
    df = clean_data(df)
    
    # 4. Add time features (if datetime index exists)
    if not args.no_time_features:
        df = add_time_features(df)
    
    # 5. Validate sensor columns
    validate_sensor_columns(df)
    
    # 6. Save prepared data
    save_prepared_data(df, args.output)
    
    print("=" * 70)
    print(" DATA PREPARATION COMPLETE!")
    print("=" * 70)


if __name__ == "__main__":
    main()
