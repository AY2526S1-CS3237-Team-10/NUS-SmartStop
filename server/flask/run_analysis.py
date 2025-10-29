"""
Lightweight Bus Stop Crowd Detection for Flask Server
Analyzes image and publishes to MQTT/InfluxDB without verbose output

Usage:
    python run_analysis.py reference.jpg test.jpg
"""

import sys
import cv2
import numpy as np
import json
import paho.mqtt.client as mqtt
from pathlib import Path
from datetime import datetime


# MQTT Configuration
MQTT_BROKER = "157.230.250.226"
MQTT_PORT = 1883
MQTT_TOPIC = "nus-smartstop/weather/data"
DEVICE_ID = "ESP32-CAM"


def analyze_images(reference_path, test_path):
    """
    Lightweight image analysis - only pixel and contour methods
    
    Args:
        reference_path: Path to reference (empty) image
        test_path: Path to test image
    
    Returns:
        dict with pixel_coverage and contour_coverage
    """
    print(f"[DEBUG] Loading reference image: {reference_path}")
    reference = cv2.imread(str(reference_path))
    if reference is None:
        print(f"[ERROR] Failed to load reference image: {reference_path}")
        return None
    print(f"[DEBUG] Reference image loaded: {reference.shape[1]}x{reference.shape[0]} pixels")
    
    print(f"[DEBUG] Loading test image: {test_path}")
    test = cv2.imread(str(test_path))
    if test is None:
        print(f"[ERROR] Failed to load test image: {test_path}")
        return None
    print(f"[DEBUG] Test image loaded: {test.shape[1]}x{test.shape[0]} pixels")
    
    # Resize test if needed
    if test.shape[:2] != reference.shape[:2]:
        print(f"[DEBUG] Resizing test image from {test.shape[1]}x{test.shape[0]} to {reference.shape[1]}x{reference.shape[0]}")
        test = cv2.resize(test, (reference.shape[1], reference.shape[0]))
    
    print("[DEBUG] Converting images to grayscale...")
    # Convert to grayscale
    ref_gray = cv2.cvtColor(reference, cv2.COLOR_BGR2GRAY)
    test_gray = cv2.cvtColor(test, cv2.COLOR_BGR2GRAY)
    
    print("[DEBUG] Calculating pixel difference...")
    # Method 1: Pixel difference
    diff = cv2.absdiff(ref_gray, test_gray)
    _, binary_diff = cv2.threshold(diff, 30, 255, cv2.THRESH_BINARY)
    changed_pixels = np.count_nonzero(binary_diff)
    total_pixels = ref_gray.shape[0] * ref_gray.shape[1]
    pixel_coverage = round((changed_pixels / total_pixels) * 100, 2)
    print(f"[DEBUG] Pixel coverage: {pixel_coverage}% ({changed_pixels}/{total_pixels} pixels)")
    
    print("[DEBUG] Calculating contour detection...")
    # Method 2: Contour detection
    kernel = cv2.getStructuringElement(cv2.MORPH_ELLIPSE, (5, 5))
    cleaned = cv2.morphologyEx(binary_diff, cv2.MORPH_CLOSE, kernel, iterations=2)
    contours, _ = cv2.findContours(cleaned, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)
    valid_contours = [c for c in contours if cv2.contourArea(c) > 50]
    total_filled_area = sum(cv2.contourArea(c) for c in valid_contours)
    contour_coverage = round((total_filled_area / total_pixels) * 100, 2)
    print(f"[DEBUG] Contour coverage: {contour_coverage}% ({len(valid_contours)} contours detected)")
    
    print("[DEBUG] Analysis complete!")
    return {
        "pixel_coverage": pixel_coverage,
        "contour_coverage": contour_coverage,
        "timestamp": datetime.now().isoformat()
    }


def publish_to_mqtt(pixel_coverage, contour_coverage, timestamp):
    """
    Publish results to MQTT broker
    
    Args:
        pixel_coverage: Pixel difference percentage
        contour_coverage: Contour detection percentage
        timestamp: ISO format timestamp
    
    Returns:
        True if successful, False otherwise
    """
    payload = {
        "deviceId": DEVICE_ID,
        "pixelDifference": pixel_coverage,
        "contourDetection": contour_coverage,
        "timestamp": timestamp
    }
    
    json_payload = json.dumps(payload)
    print(f"[DEBUG] MQTT Payload: {json_payload}")
    
    try:
        print(f"[DEBUG] Connecting to MQTT broker: {MQTT_BROKER}:{MQTT_PORT}")
        # Try new API first, fallback to old API for compatibility
        try:
            client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION1)
        except:
            client = mqtt.Client()
        client.connect(MQTT_BROKER, MQTT_PORT, 60)
        client.loop_start()
        print(f"[DEBUG] Publishing to topic: {MQTT_TOPIC}")
        result = client.publish(MQTT_TOPIC, json_payload, qos=0, retain=False)
        result.wait_for_publish()
        client.loop_stop()
        client.disconnect()
        print("[DEBUG] MQTT publish successful!")
        return True
    except Exception as e:
        print(f"[ERROR] MQTT publish failed: {e}")
        return False


def main():
    """Lightweight main function for Flask server"""
    print("[DEBUG] ========== Bus Stop Crowd Detection Started ==========")
    
    if len(sys.argv) < 3:
        print("[ERROR] Missing arguments!")
        print("[USAGE] python run_analysis.py <reference_image> <test_image>")
        print("[EXAMPLE] python run_analysis.py bus_stop_empty.jpg captured.jpg")
        sys.exit(1)
    
    reference_img = sys.argv[1]
    test_img = sys.argv[2]
    
    print(f"[DEBUG] Reference image: {reference_img}")
    print(f"[DEBUG] Test image: {test_img}")
    
    # Validate files exist
    if not Path(reference_img).exists():
        print(f"[ERROR] Reference image not found: {reference_img}")
        sys.exit(1)
    
    if not Path(test_img).exists():
        print(f"[ERROR] Test image not found: {test_img}")
        sys.exit(1)
    
    print("[DEBUG] Both images found, starting analysis...")
    
    # Analyze images
    results = analyze_images(reference_img, test_img)
    if not results:
        print("[ERROR] Image analysis failed!")
        sys.exit(1)
    
    # Publish to MQTT
    success = publish_to_mqtt(
        results['pixel_coverage'],
        results['contour_coverage'],
        results['timestamp']
    )
    
    if success:
        print("[DEBUG] ========== Pipeline Complete (SUCCESS) ==========")
        sys.exit(0)
    else:
        print("[DEBUG] ========== Pipeline Complete (MQTT FAILED) ==========")
        sys.exit(1)


if __name__ == "__main__":
    main()
