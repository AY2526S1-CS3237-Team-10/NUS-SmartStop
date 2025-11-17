#!/usr/bin/env python3
"""
Background prediction service - runs every 30 seconds
Updates cached prediction file for instant ESP32 responses
Run this with: nohup python3 background_predictor.py &
"""

import subprocess
import sys
import json
import time
from datetime import datetime

CACHE_FILE = '/root/cs3237_server/prediction_cache.json' # Path to cache file
INFERENCE_SCRIPT = '/root/cs3237_server/Deployment/run_inference.py' # Path to inference script
PYTHON_PATH = '/root/venv/bin/python' # Path to Python interpreter
UPDATE_INTERVAL = 30  # Seconds between predictions

def run_prediction():
    """Runs inference and returns capacity"""
    try:
        print(f"[{datetime.now().strftime('%H:%M:%S')}] Running prediction...")
        
        result = subprocess.run(
            [PYTHON_PATH, INFERENCE_SCRIPT],
            cwd='/root/cs3237_server/Deployment',
            capture_output=True,
            text=True,
            timeout=45
        )
        
        # Parse capacity from output
        capacity = -1
        if result.returncode == 0:
            for line in result.stdout.split('\n'):
                if 'Predicted Capacity:' in line:
                    try:
                        capacity = float(line.split(':')[1].strip().split()[0])
                        break
                    except:
                        pass
        
        return capacity
        
    except Exception as e:
        print(f"[ERROR] Prediction failed: {e}")
        return -1

def update_cache(capacity):
    """Writes prediction to cache file"""
    try:
        cache_data = {
            "success": capacity >= 0,
            "capacity": round(capacity, 2),
            "timestamp": datetime.now().isoformat(),
            "message": "Prediction successful" if capacity >= 0 else "No data available"
        }
        
        with open(CACHE_FILE, 'w') as f:
            json.dump(cache_data, f, indent=2)
        
        print(f"[OK] Cache updated: capacity={capacity}")
        return True
        
    except Exception as e:
        print(f"[ERROR] Cache write failed: {e}")
        return False

def main():
    print("=" * 70)
    print("BACKGROUND PREDICTION SERVICE")
    print("=" * 70)
    print(f"Cache file: {CACHE_FILE}")
    print(f"Update interval: {UPDATE_INTERVAL} seconds")
    print(f"Press Ctrl+C to stop")
    print("=" * 70)
    print()
    
    while True:
        try:
            # Run prediction
            capacity = run_prediction()
            
            # Update cache
            update_cache(capacity)
            
            # Wait before next update
            print(f"Waiting {UPDATE_INTERVAL} seconds...\n")
            time.sleep(UPDATE_INTERVAL)
            
        except KeyboardInterrupt:
            print("\nStopping background service...")
            break
        except Exception as e:
            print(f"[ERROR] {e}")
            time.sleep(UPDATE_INTERVAL)

if __name__ == "__main__":
    main()
