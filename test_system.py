#!/usr/bin/env python3
"""
Test script for NUS-SmartStop system
Tests all components to verify setup
"""

import requests
import json
import sys
import time
from datetime import datetime

# Configuration
FLASK_URL = "http://localhost:5000"
MQTT_BROKER = "localhost"
MQTT_PORT = 1883

def print_test(test_name):
    """Print test header"""
    print(f"\n{'='*60}")
    print(f"Testing: {test_name}")
    print('='*60)

def test_flask_health():
    """Test Flask server health endpoint"""
    print_test("Flask Server Health Check")
    try:
        response = requests.get(f"{FLASK_URL}/health", timeout=5)
        if response.status_code == 200:
            data = response.json()
            print(f"✓ Flask server is healthy")
            print(f"  Status: {data.get('status')}")
            print(f"  Timestamp: {data.get('timestamp')}")
            return True
        else:
            print(f"✗ Health check failed with status {response.status_code}")
            return False
    except Exception as e:
        print(f"✗ Failed to connect to Flask server: {e}")
        print(f"  Make sure Flask is running on {FLASK_URL}")
        return False

def test_flask_list_images():
    """Test Flask list images endpoint"""
    print_test("Flask List Images")
    try:
        response = requests.get(f"{FLASK_URL}/api/images", timeout=5)
        if response.status_code == 200:
            data = response.json()
            print(f"✓ Successfully retrieved image list")
            print(f"  Total images: {data.get('count', 0)}")
            if data.get('images'):
                print(f"  Sample images:")
                for img in data['images'][:3]:
                    print(f"    - {img}")
            return True
        else:
            print(f"✗ Failed to list images: {response.status_code}")
            return False
    except Exception as e:
        print(f"✗ Error listing images: {e}")
        return False

def test_mqtt_connection():
    """Test MQTT broker connection"""
    print_test("MQTT Broker Connection")
    try:
        import paho.mqtt.client as mqtt
        
        connected = [False]
        
        def on_connect(client, userdata, flags, rc):
            if rc == 0:
                connected[0] = True
        
        client = mqtt.Client()
        client.on_connect = on_connect
        
        client.connect(MQTT_BROKER, MQTT_PORT, 10)
        client.loop_start()
        
        # Wait for connection
        timeout = 5
        start_time = time.time()
        while not connected[0] and (time.time() - start_time) < timeout:
            time.sleep(0.1)
        
        client.loop_stop()
        client.disconnect()
        
        if connected[0]:
            print(f"✓ Successfully connected to MQTT broker")
            print(f"  Broker: {MQTT_BROKER}:{MQTT_PORT}")
            return True
        else:
            print(f"✗ Failed to connect to MQTT broker")
            return False
            
    except ImportError:
        print(f"✗ paho-mqtt library not installed")
        print(f"  Install with: pip install paho-mqtt")
        return False
    except Exception as e:
        print(f"✗ MQTT connection error: {e}")
        print(f"  Make sure Mosquitto is running on {MQTT_BROKER}:{MQTT_PORT}")
        return False

def test_influxdb_connection():
    """Test InfluxDB connection"""
    print_test("InfluxDB Connection")
    try:
        import os
        from dotenv import load_dotenv
        
        load_dotenv()
        
        influxdb_url = os.getenv('INFLUXDB_URL', 'http://localhost:8086')
        
        # Try to reach InfluxDB health endpoint
        response = requests.get(f"{influxdb_url}/health", timeout=5)
        
        if response.status_code == 200:
            print(f"✓ InfluxDB is accessible")
            print(f"  URL: {influxdb_url}")
            data = response.json()
            print(f"  Status: {data.get('status', 'unknown')}")
            return True
        else:
            print(f"✗ InfluxDB health check failed: {response.status_code}")
            return False
            
    except ImportError:
        print(f"✗ Required libraries not installed")
        print(f"  Install with: pip install python-dotenv")
        return False
    except Exception as e:
        print(f"✗ InfluxDB connection error: {e}")
        print(f"  Make sure InfluxDB is running (docker-compose up -d)")
        return False

def test_python_imports():
    """Test if all required Python packages are installed"""
    print_test("Python Dependencies")
    
    packages = [
        ('flask', 'Flask'),
        ('flask_cors', 'Flask-CORS'),
        ('paho.mqtt.client', 'Paho MQTT'),
        ('influxdb_client', 'InfluxDB Client'),
        ('PIL', 'Pillow'),
        ('cv2', 'OpenCV'),
        ('numpy', 'NumPy'),
        ('torch', 'PyTorch'),
        ('dotenv', 'python-dotenv'),
    ]
    
    missing = []
    installed = []
    
    for module, name in packages:
        try:
            __import__(module)
            installed.append(name)
            print(f"  ✓ {name}")
        except ImportError:
            missing.append(name)
            print(f"  ✗ {name} (not installed)")
    
    print(f"\nInstalled: {len(installed)}/{len(packages)}")
    
    if missing:
        print(f"\nMissing packages:")
        for pkg in missing:
            print(f"  - {pkg}")
        print(f"\nInstall with: pip install -r requirements.txt")
        return False
    
    return True

def test_file_structure():
    """Test if all required files exist"""
    print_test("Project File Structure")
    
    required_files = [
        'requirements.txt',
        '.env.example',
        'docker-compose.yml',
        'server/flask/app.py',
        'server/influxdb/client.py',
        'server/mqtt/mqtt_client.py',
        'ml_models/inference.py',
        'esp32/smartstop_main.ino',
    ]
    
    import os
    
    missing = []
    for file in required_files:
        if os.path.exists(file):
            print(f"  ✓ {file}")
        else:
            print(f"  ✗ {file} (missing)")
            missing.append(file)
    
    if missing:
        print(f"\n✗ Missing {len(missing)} required files")
        return False
    else:
        print(f"\n✓ All required files present")
        return True

def main():
    """Run all tests"""
    print("\n" + "="*60)
    print(" NUS-SmartStop System Test Suite")
    print("="*60)
    print(f" Timestamp: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}")
    print("="*60)
    
    results = {
        'File Structure': test_file_structure(),
        'Python Dependencies': test_python_imports(),
        'Flask Server': test_flask_health(),
        'Flask API': test_flask_list_images(),
        'MQTT Broker': test_mqtt_connection(),
        'InfluxDB': test_influxdb_connection(),
    }
    
    # Summary
    print_test("Test Summary")
    passed = sum(1 for v in results.values() if v)
    total = len(results)
    
    for test_name, result in results.items():
        status = "✓ PASS" if result else "✗ FAIL"
        print(f"  {status:8} - {test_name}")
    
    print(f"\n{'='*60}")
    print(f" Results: {passed}/{total} tests passed")
    print('='*60)
    
    if passed == total:
        print("\n🎉 All tests passed! System is ready to use.")
        return 0
    else:
        print(f"\n⚠️  {total - passed} test(s) failed. Please check the errors above.")
        return 1

if __name__ == '__main__':
    sys.exit(main())
