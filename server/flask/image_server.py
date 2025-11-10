from flask import Flask, request, jsonify, send_file
from datetime import datetime
import os
import csv
import json

app = Flask(__name__)
import torch
import torchvision.transforms as transforms
from torchvision import models
import torch.nn as nn
from PIL import Image
import paho.mqtt.client as mqtt

# Directories
BASE_DIR = '/root/cs3237_server'
IMAGE_FOLDER = os.path.join(BASE_DIR, 'images')
METADATA_FILE = os.path.join(BASE_DIR, 'metadata.csv')
MODEL_PATH = os.path.join(BASE_DIR, 'smartstop_mobilenet_v2_esp32cam.pth')
CLASSES_PATH = os.path.join(BASE_DIR, 'class_names.json')

# MQTT Config
MQTT_BROKER = "127.0.0.1"  # Localhost because it's on the same server
MQTT_PORT = 1883
MQTT_TOPIC = "nus-smartstop/crowd/data"

os.makedirs(IMAGE_FOLDER, exist_ok=True)

# Initialize metadata CSV
if not os.path.exists(METADATA_FILE):
    with open(METADATA_FILE, 'w', newline='') as f:
        writer = csv.writer(f)
        writer.writerow(['timestamp', 'filename', 'size_bytes', 'device_id'])
        
# =========================================
# ========= GLOBAL ML OBJECTS ============
# =========================================
ML_MODEL = None
ML_DEVICE = torch.device("cpu") # Assume server is CPU-only for stability
ML_CLASSES = None
ML_TRANSFORM = None

# =========================================
# ========= ML HELPER FUNCTIONS ==========
# =========================================
class SquarePad:
    """Matches the training preprocessing exactly"""
    def __call__(self, image):
        w, h = image.size
        max_wh = max(w, h)
        hp = int((max_wh - w) / 2)
        vp = int((max_wh - h) / 2)
        padding = (hp, vp, hp, vp)
        return transforms.functional.pad(image, padding, 0, 'constant')

def initialize_ml():
    """Loads model and classes once on startup"""
    global ML_MODEL, ML_CLASSES, ML_TRANSFORM
    print("🤖 [ML] Initializing model...")

    # 1. Define Transforms (MUST match training exactly)
    ML_TRANSFORM = transforms.Compose([
        SquarePad(),
        transforms.Resize((224, 224)),
        transforms.ToTensor(),
        transforms.Normalize(mean=[0.485, 0.456, 0.406], std=[0.229, 0.224, 0.225])
    ])

    # 2. Load Classes
    try:
        with open(CLASSES_PATH, 'r') as f:
            ML_CLASSES = json.load(f)
        print(f"🤖 [ML] Loaded classes: {ML_CLASSES}")
    except Exception as e:
        print(f"⚠️ [ML Error] Could not load class_names.json: {e}")
        return

    # 3. Load Model
    try:
        # Re-create the exact same model structure used in training
        model = models.mobilenet_v2(weights=None)
        model.classifier = nn.Sequential(
            nn.Dropout(p=0.2),
            nn.Linear(model.last_channel, len(ML_CLASSES))
        )
        # Load trained weights (map to CPU to be safe on server)
        state_dict = torch.load(MODEL_PATH, map_location=ML_DEVICE)
        model.load_state_dict(state_dict)
        model.to(ML_DEVICE)
        model.eval() # CRITICAL: Set to eval mode for inference
        ML_MODEL = model
        print("✅ [ML] Model loaded successfully!")
    except Exception as e:
        print(f"❌ [ML Error] Failed to load model: {e}")

def run_inference(image_path):
    """Runs prediction on a single image file"""
    if ML_MODEL is None: return None, 0.0

    try:
        img = Image.open(image_path).convert('RGB')
        input_tensor = ML_TRANSFORM(img).unsqueeze(0).to(ML_DEVICE)
        
        with torch.no_grad():
            output = ML_MODEL(input_tensor)
        
        probs = torch.nn.functional.softmax(output[0], dim=0)
        top_prob, top_catid = torch.max(probs, 0)
        
        result = ML_CLASSES[top_catid.item()]
        confidence = top_prob.item() * 100
        return result, confidence
    except Exception as e:
        print(f"❌ [Inference Error] {e}")
        return None, 0.0

def publish_mqtt(device_id, result, confidence):
    """Publishes result to MQTT for InfluxDB/Telegraf"""
    try:
        # Create standard JSON payload for Telegraf
        payload = {
            "measurement": "crowd_analytics",
            "deviceId": device_id,
            "crowd_level": result,
            "confidence": round(confidence, 2),
            "source": "cloud_cnn"
        }
        # One-shot publish
        mqtt.Client().connect(MQTT_BROKER, MQTT_PORT).publish(MQTT_TOPIC, json.dumps(payload))
        print(f"📡 [MQTT] Published: {result} ({confidence:.1f}%)")
    except Exception as e:
        print(f"⚠️ [MQTT Error] {e}")

@app.route('/upload', methods=['POST'])
def upload_image():
    try:
        # Get raw image data
        image_data = request.get_data()
        
        if len(image_data) == 0:
            return jsonify({'error': 'No image data'}), 400
        
        # Get device ID from headers (optional)
        device_id = request.headers.get('Device-ID', 'esp32cam')
        
        # Generate filename
        timestamp = datetime.now()
        filename = f"{device_id}_{timestamp.strftime('%Y%m%d_%H%M%S')}.jpg"
        filepath = os.path.join(IMAGE_FOLDER, filename)
        
        # Save image
        with open(filepath, 'wb') as f:
            f.write(image_data)
            print(f"✅ Image saved: {filename} ({len(image_data)} bytes)")
            
        # Run ML Inference immediately
        ml_result, ml_conf = run_inference(filepath)
        if ml_result:
            print(f"🤖 [ML] {filename} -> {ml_result} ({ml_conf:.1f}%)")
            # 3. Publish to MQTT for InfluxDB
            publish_mqtt(device_id, ml_result, ml_conf)
        
        # Log to CSV (updated with ML results)
        with open(METADATA_FILE, 'a', newline='') as f:
            writer = csv.writer(f)
            writer.writerow([
                timestamp.isoformat(), filename, len(image_data), 
                device_id, ml_result, round(ml_conf, 2)
            ])
        
        # Log metadata
        with open(METADATA_FILE, 'a', newline='') as f:
            writer = csv.writer(f)
            writer.writerow([
                timestamp.isoformat(),
                filename,
                len(image_data),
                device_id
            ])
        
        return jsonify({
            'success': True,
            'filename': filename,
            'size': len(image_data),
            'url': f'/images/{filename}',
            'ml_result': ml_result,
            'confidence': ml_conf
        }), 200
        
    except Exception as e:
        print(f"❌ Error: {str(e)}")
        return jsonify({'error': str(e)}), 500

@app.route('/images/<filename>', methods=['GET'])
def get_image(filename):
    """Serve image file"""
    try:
        filepath = os.path.join(IMAGE_FOLDER, filename)
        if os.path.exists(filepath):
            return send_file(filepath, mimetype='image/jpeg')
        else:
            return jsonify({'error': 'Image not found'}), 404
    except Exception as e:
        return jsonify({'error': str(e)}), 500

@app.route('/images', methods=['GET'])
def list_images():
    """List all images"""
    try:
        images = sorted(os.listdir(IMAGE_FOLDER), reverse=True)
        image_list = []
        for img in images[:50]:  # Last 50 images
            filepath = os.path.join(IMAGE_FOLDER, img)
            image_list.append({
                'filename': img,
                'size': os.path.getsize(filepath),
                'url': f'/images/{img}'
            })
        return jsonify({
            'count': len(images),
            'images': image_list
        }), 200
    except Exception as e:
        return jsonify({'error': str(e)}), 500

@app.route('/health', methods=['GET'])
def health():
    """Health check endpoint"""
    import shutil
    image_count = len(os.listdir(IMAGE_FOLDER))
    disk = shutil.disk_usage(IMAGE_FOLDER)
    
    return jsonify({
        'status': 'running',
        'service': 'CS3237 Image Server',
        'images_stored': image_count,
        'disk_free_gb': round(disk.free / (1024**3), 2)
    }), 200

@app.route('/', methods=['GET'])
def index():
    """Simple web gallery"""
    images = sorted(os.listdir(IMAGE_FOLDER), reverse=True)
    
    html = f"""
    <!DOCTYPE html>
    <html>
    <head>
        <title>CS3237 Image Gallery</title>
        <style>
            body {{ font-family: Arial; margin: 20px; background: #f0f0f0; }}
            h1 {{ color: #333; }}
            .info {{ background: white; padding: 15px; border-radius: 8px; margin-bottom: 20px; }}
            .gallery {{ display: grid; grid-template-columns: repeat(auto-fill, minmax(300px, 1fr)); gap: 20px; }}
            .image-card {{ background: white; padding: 10px; border-radius: 8px; box-shadow: 0 2px 4px rgba(0,0,0,0.1); }}
            img {{ width: 100%; height: auto; border-radius: 4px; }}
            .filename {{ font-size: 12px; color: #666; margin-top: 5px; word-break: break-all; }}
        </style>
    </head>
    <body>
        <h1>📸 CS3237 Group 10 - Image Gallery</h1>
        <div class="info">
            <p><strong>Total images:</strong> {len(images)}</p>
            <p><strong>Endpoints:</strong></p>
            <ul>
                <li><a href="/health">/health</a> - Server status</li>
                <li><a href="/images">/images</a> - List images (JSON)</li>
                <li>/upload - POST endpoint for images</li>
            </ul>
        </div>
        <div class="gallery">
    """
    
    for img in images[:30]:  # Show last 30 images
        html += f"""
            <div class="image-card">
                <a href="/images/{img}" target="_blank">
                    <img src="/images/{img}" alt="{img}">
                </a>
                <div class="filename">{img}</div>
            </div>
        """
    
    html += """
        </div>
    </body>
    </html>
    """
    
    return html

if __name__ == '__main__':
    print("🚀 Starting CS3237 Image Server...")
    print(f"📁 Images saved to: {IMAGE_FOLDER}")
    print(f"📊 Metadata logged to: {METADATA_FILE}")
    app.run(host='0.0.0.0', port=5000, debug=False)
