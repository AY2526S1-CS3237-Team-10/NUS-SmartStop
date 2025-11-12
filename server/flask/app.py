"""
Flask Server for Smart Bus Stop Image Handling
This server receives images from ESP32 cameras, stores them,
and provides endpoints for ML model inference.
"""

import os
import json
from datetime import datetime
from flask import Flask, request, jsonify, send_from_directory
from flask_cors import CORS
from werkzeug.utils import secure_filename
from dotenv import load_dotenv
import logging

# Load environment variables
load_dotenv()

# Initialize Flask app
app = Flask(__name__)
CORS(app)

# Configuration
app.config['UPLOAD_FOLDER'] = os.getenv('UPLOAD_FOLDER', './uploads')
app.config['MAX_CONTENT_LENGTH'] = int(os.getenv('MAX_CONTENT_LENGTH', 16 * 1024 * 1024))
ALLOWED_EXTENSIONS = {'png', 'jpg', 'jpeg', 'gif'}

# API Key for authentication
API_KEY = os.getenv('API_KEY', 'CHANGE_ME_INSECURE_DEFAULT')

# Create upload folder if it doesn't exist
os.makedirs(app.config['UPLOAD_FOLDER'], exist_ok=True)

# Setup logging
logging.basicConfig(level=logging.INFO)
logger = logging.getLogger(__name__)


def allowed_file(filename):
    """Check if file extension is allowed"""
    return '.' in filename and \
           filename.rsplit('.', 1)[1].lower() in ALLOWED_EXTENSIONS


def check_api_key():
    """Check if request has valid API key"""
    api_key = request.headers.get('X-API-Key')
    return api_key == API_KEY


@app.route('/health', methods=['GET'])
def health_check():
    """Health check endpoint"""
    return jsonify({
        'status': 'healthy',
        'timestamp': datetime.utcnow().isoformat()
    })


@app.route('/upload', methods=['POST'])
def upload_image():
    """
    Upload image from ESP32 camera
    Expected: multipart/form-data with 'image' field
    Requires: X-API-Key header for authentication
    """
    # Check API key authentication
    if not check_api_key():
        logger.warning(f"Unauthorized upload attempt from {request.remote_addr}")
        return jsonify({'error': 'Unauthorized'}), 401
    
    try:
        if 'image' not in request.files:
            return jsonify({'error': 'No image file provided'}), 400
        
        file = request.files['image']
        
        if file.filename == '':
            return jsonify({'error': 'No file selected'}), 400
        
        if file and allowed_file(file.filename):
            filename = secure_filename(file.filename)
            timestamp = datetime.utcnow().strftime('%Y%m%d_%H%M%S')
            filename = f"{timestamp}_{filename}"
            filepath = os.path.join(app.config['UPLOAD_FOLDER'], filename)
            
            file.save(filepath)
            logger.info(f"Image saved: {filename}")
            
            # Get optional metadata from request
            metadata = {
                'device_id': request.form.get('device_id', 'unknown'),
                'location': request.form.get('location', 'unknown'),
                'timestamp': timestamp
            }
            
            return jsonify({
                'status': 'success',
                'filename': filename,
                'filepath': filepath,
                'metadata': metadata
            }), 200
        
        return jsonify({'error': 'Invalid file type'}), 400
    
    except Exception as e:
        logger.error(f"Error uploading image: {str(e)}")
        return jsonify({'error': str(e)}), 500


@app.route('/api/images/<filename>', methods=['GET'])
def get_image(filename):
    """Retrieve uploaded image"""
    try:
        return send_from_directory(app.config['UPLOAD_FOLDER'], filename)
    except Exception as e:
        logger.error(f"Error retrieving image: {str(e)}")
        return jsonify({'error': 'Image not found'}), 404


@app.route('/api/images', methods=['GET'])
def list_images():
    """List all uploaded images"""
    try:
        files = os.listdir(app.config['UPLOAD_FOLDER'])
        images = [f for f in files if allowed_file(f)]
        return jsonify({
            'count': len(images),
            'images': images
        })
    except Exception as e:
        logger.error(f"Error listing images: {str(e)}")
        return jsonify({'error': str(e)}), 500


@app.route('/api/inference', methods=['POST'])
def run_inference():
    """
    Run ML model inference on uploaded image
    Expected JSON: {'filename': 'image.jpg'}
    """
    try:
        data = request.get_json()
        
        if not data or 'filename' not in data:
            return jsonify({'error': 'Filename required'}), 400
        
        filename = data['filename']
        filepath = os.path.join(app.config['UPLOAD_FOLDER'], filename)
        
        if not os.path.exists(filepath):
            return jsonify({'error': 'Image not found'}), 404
        
        # Placeholder for ML inference
        # This will be implemented with actual ML models
        result = {
            'filename': filename,
            'predictions': [],
            'confidence': 0.0,
            'message': 'ML inference not yet implemented'
        }
        
        logger.info(f"Inference requested for: {filename}")
        return jsonify(result), 200
    
    except Exception as e:
        logger.error(f"Error during inference: {str(e)}")
        return jsonify({'error': str(e)}), 500


if __name__ == '__main__':
    host = os.getenv('FLASK_HOST', '0.0.0.0')
    port = int(os.getenv('FLASK_PORT', 5000))
    debug = os.getenv('FLASK_DEBUG', 'True') == 'True'
    
    logger.info(f"Starting Flask server on {host}:{port}")
    app.run(host=host, port=port, debug=debug)
