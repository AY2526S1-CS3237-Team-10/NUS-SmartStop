"""
Flask Image Server with multipart and raw body upload support.
Includes gallery UI, metadata persistence, and inference endpoint stub.
"""
import os
import logging
import sqlite3
import shutil
from datetime import datetime, timezone
from pathlib import Path

from flask import Flask, request, jsonify, send_from_directory, render_template_string
from flask_cors import CORS
from werkzeug.utils import secure_filename
from werkzeug.exceptions import NotFound
from PIL import Image

# Try to load environment variables from .env file
try:
    from dotenv import load_dotenv
    load_dotenv()
except ImportError:
    pass

# Configuration from environment variables
UPLOAD_FOLDER = os.path.abspath(os.getenv('UPLOAD_FOLDER', './uploads'))
MAX_CONTENT_LENGTH = int(os.getenv('MAX_CONTENT_LENGTH', 16 * 1024 * 1024))  # 16MB default
FLASK_HOST = os.getenv('FLASK_HOST', '0.0.0.0')
FLASK_PORT = int(os.getenv('FLASK_PORT', 5000))
FLASK_DEBUG = os.getenv('FLASK_DEBUG', 'False').lower() == 'true'
API_KEY = os.getenv('API_KEY', 'default-secret-key')

ALLOWED_EXTENSIONS = {'png', 'jpg', 'jpeg', 'gif'}
DB_PATH = os.path.join(os.path.dirname(__file__), 'metadata.db')

# Setup logging
logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s - %(name)s - %(levelname)s - %(message)s'
)
logger = logging.getLogger(__name__)

# Initialize Flask app
app = Flask(__name__)
app.config['UPLOAD_FOLDER'] = UPLOAD_FOLDER
app.config['MAX_CONTENT_LENGTH'] = MAX_CONTENT_LENGTH
CORS(app)

# Create upload folder if it doesn't exist
Path(UPLOAD_FOLDER).mkdir(parents=True, exist_ok=True)


def init_db():
    """Initialize SQLite database with metadata table."""
    conn = sqlite3.connect(DB_PATH)
    cursor = conn.cursor()
    cursor.execute('''
        CREATE TABLE IF NOT EXISTS metadata (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            timestamp TEXT NOT NULL,
            filename TEXT NOT NULL,
            size_bytes INTEGER NOT NULL,
            device_id TEXT
        )
    ''')
    conn.commit()
    conn.close()
    logger.info(f"Database initialized at {DB_PATH}")


def allowed_file(filename):
    """Check if filename has an allowed extension."""
    return '.' in filename and filename.rsplit('.', 1)[1].lower() in ALLOWED_EXTENSIONS


def validate_image(file_path):
    """Validate that the file is a valid image using Pillow."""
    try:
        with Image.open(file_path) as img:
            img.verify()
        return True
    except Exception as e:
        logger.error(f"Image validation failed for {file_path}: {e}")
        return False


def save_metadata(timestamp, filename, size_bytes, device_id=None):
    """Save metadata to SQLite database."""
    try:
        conn = sqlite3.connect(DB_PATH)
        cursor = conn.cursor()
        cursor.execute('''
            INSERT INTO metadata (timestamp, filename, size_bytes, device_id)
            VALUES (?, ?, ?, ?)
        ''', (timestamp, filename, size_bytes, device_id))
        conn.commit()
        conn.close()
        logger.info(f"Metadata saved for {filename}")
    except Exception as e:
        logger.error(f"Failed to save metadata: {e}")


def get_disk_free_gb():
    """Get free disk space in GB."""
    try:
        stat = shutil.disk_usage(UPLOAD_FOLDER)
        return round(stat.free / (1024 ** 3), 2)
    except Exception as e:
        logger.error(f"Failed to get disk space: {e}")
        return 0


def get_images_count():
    """Get count of stored images from database."""
    try:
        conn = sqlite3.connect(DB_PATH)
        cursor = conn.cursor()
        cursor.execute('SELECT COUNT(*) FROM metadata')
        count = cursor.fetchone()[0]
        conn.close()
        return count
    except Exception as e:
        logger.error(f"Failed to get images count: {e}")
        return 0


@app.route('/health', methods=['GET'])
def health():
    """Health check endpoint."""
    return jsonify({
        'status': 'healthy',
        'timestamp': datetime.now(timezone.utc).isoformat().replace('+00:00', 'Z'),
        'images_stored': get_images_count(),
        'disk_free_gb': get_disk_free_gb()
    })


@app.route('/api/upload', methods=['POST'])
def upload_image():
    """
    Upload image endpoint supporting both multipart and raw body uploads.
    - Multipart: expects 'image' field in request.files
    - Raw body: expects raw image bytes with Device-ID header or device_id query/form param
    - Requires X-API-Key header for authentication
    """
    # Check API key authentication
    if request.headers.get('X-API-Key') != API_KEY:
        logger.warning(f"Unauthorized upload attempt from {request.remote_addr}")
        return jsonify({'error': 'Unauthorized'}), 401
    
    try:
        timestamp = datetime.now(timezone.utc).strftime('%Y%m%d_%H%M%S')
        device_id = None
        filename = None
        
        # Try multipart upload first
        if 'image' in request.files:
            file = request.files['image']
            if file.filename == '':
                return jsonify({'error': 'No selected file'}), 400
            
            if file and allowed_file(file.filename):
                # Use secure filename with timestamp prefix
                original_filename = secure_filename(file.filename)
                filename = f"{timestamp}_{original_filename}"
                filepath = os.path.join(app.config['UPLOAD_FOLDER'], filename)
                
                # Save file
                file.save(filepath)
                logger.info(f"Multipart upload saved: {filename}")
            else:
                return jsonify({'error': 'File type not allowed'}), 400
        else:
            # Handle raw body upload
            raw_data = request.get_data()
            if not raw_data:
                return jsonify({'error': 'No image data provided'}), 400
            
            # Get device_id from header, query param, or form
            device_id = request.headers.get('Device-ID') or \
                       request.args.get('device_id') or \
                       request.form.get('device_id')
            
            if not device_id:
                device_id = 'unknown'
            
            # Sanitize device_id to prevent path injection
            # Only allow alphanumeric, hyphens, and underscores
            # This prevents path traversal attacks (e.g., "../../../etc/passwd")
            device_id = ''.join(c for c in device_id if c.isalnum() or c in '-_')
            if not device_id:
                device_id = 'unknown'
            
            # Assume JPEG if no extension specified
            filename = f"{device_id}_{timestamp}.jpg"
            # Safe: UPLOAD_FOLDER is absolute, filename is sanitized
            filepath = os.path.join(app.config['UPLOAD_FOLDER'], filename)
            
            # Save raw bytes
            with open(filepath, 'wb') as f:
                f.write(raw_data)
            logger.info(f"Raw body upload saved: {filename}")
        
        # Validate the saved image
        if not validate_image(filepath):
            os.remove(filepath)
            return jsonify({'error': 'Invalid image file'}), 400
        
        # Get file size
        file_size = os.path.getsize(filepath)
        
        # Save metadata to database
        save_metadata(
            timestamp=datetime.now(timezone.utc).isoformat().replace('+00:00', 'Z'),
            filename=filename,
            size_bytes=file_size,
            device_id=device_id
        )
        
        return jsonify({
            'status': 'success',
            'filename': filename,
            'size': file_size,
            'url': f"/api/images/{filename}",
            'metadata': {
                'timestamp': datetime.now(timezone.utc).isoformat().replace('+00:00', 'Z'),
                'device_id': device_id
            }
        }), 201
        
    except Exception as e:
        logger.error(f"Upload failed: {e}")
        return jsonify({'error': 'Upload failed'}), 500


@app.route('/upload', methods=['POST'])
def upload_image_legacy():
    """
    Legacy upload endpoint for backward compatibility with ESP32-CAM.
    Redirects to /api/upload with the same request context.
    """
    return upload_image()


@app.route('/api/images', methods=['GET'])
def list_images():
    """List images with pagination."""
    try:
        limit = request.args.get('limit', default=50, type=int)
        offset = request.args.get('offset', default=0, type=int)
        
        # Get images from upload folder that match allowed extensions
        all_files = []
        for filename in os.listdir(app.config['UPLOAD_FOLDER']):
            if allowed_file(filename):
                filepath = os.path.join(app.config['UPLOAD_FOLDER'], filename)
                all_files.append({
                    'filename': filename,
                    'size': os.path.getsize(filepath),
                    'url': f"/api/images/{filename}"
                })
        
        # Sort by filename (which includes timestamp) in reverse order
        all_files.sort(key=lambda x: x['filename'], reverse=True)
        
        # Apply pagination
        paginated_files = all_files[offset:offset + limit]
        
        return jsonify({
            'images': paginated_files,
            'total': len(all_files),
            'limit': limit,
            'offset': offset
        })
        
    except Exception as e:
        logger.error(f"Failed to list images: {e}")
        return jsonify({'error': 'Failed to list images'}), 500


@app.route('/api/images/<filename>', methods=['GET'])
def get_image(filename):
    """Serve image file."""
    try:
        return send_from_directory(app.config['UPLOAD_FOLDER'], filename)
    except (NotFound, FileNotFoundError):
        return jsonify({'error': 'Image not found'}), 404
    except Exception as e:
        logger.error(f"Failed to serve image {filename}: {e}")
        return jsonify({'error': 'Failed to serve image'}), 500


@app.route('/api/inference', methods=['POST'])
def inference():
    """ML inference endpoint stub."""
    try:
        data = request.get_json()
        if not data or 'filename' not in data:
            return jsonify({'error': 'Missing filename'}), 400
        
        filename = data['filename']
        return jsonify({
            'message': 'ML inference not yet implemented',
            'filename': filename
        })
        
    except Exception as e:
        logger.error(f"Inference request failed: {e}")
        return jsonify({'error': 'Inference request failed'}), 500


# Gallery HTML template
GALLERY_TEMPLATE = '''
<!DOCTYPE html>
<html>
<head>
    <title>Image Gallery - NUS SmartStop</title>
    <style>
        body {
            font-family: Arial, sans-serif;
            margin: 0;
            padding: 20px;
            background-color: #f5f5f5;
        }
        h1 {
            color: #333;
            text-align: center;
        }
        .gallery {
            display: grid;
            grid-template-columns: repeat(auto-fill, minmax(250px, 1fr));
            gap: 20px;
            max-width: 1400px;
            margin: 0 auto;
        }
        .image-card {
            background: white;
            border-radius: 8px;
            box-shadow: 0 2px 4px rgba(0,0,0,0.1);
            overflow: hidden;
            transition: transform 0.2s;
        }
        .image-card:hover {
            transform: translateY(-5px);
            box-shadow: 0 4px 8px rgba(0,0,0,0.2);
        }
        .image-card img {
            width: 100%;
            height: 200px;
            object-fit: cover;
        }
        .image-info {
            padding: 10px;
        }
        .filename {
            font-size: 12px;
            color: #666;
            word-break: break-all;
        }
        .stats {
            text-align: center;
            margin: 20px 0;
            color: #666;
        }
        .error {
            color: red;
            text-align: center;
            padding: 20px;
        }
    </style>
</head>
<body>
    <h1>NUS SmartStop - Image Gallery</h1>
    <div class="stats">
        <p>Showing latest {{ images|length }} images (Total: {{ total }})</p>
    </div>
    {% if images %}
    <div class="gallery">
        {% for image in images %}
        <div class="image-card">
            <a href="{{ image.url }}" target="_blank">
                <img src="{{ image.url }}" alt="{{ image.filename }}">
            </a>
            <div class="image-info">
                <div class="filename">{{ image.filename }}</div>
            </div>
        </div>
        {% endfor %}
    </div>
    {% else %}
    <div class="error">
        <p>No images found. Upload some images to get started!</p>
    </div>
    {% endif %}
</body>
</html>
'''


@app.route('/', methods=['GET'])
def gallery():
    """Display gallery of latest images."""
    try:
        # Get latest 30 images
        all_files = []
        for filename in os.listdir(app.config['UPLOAD_FOLDER']):
            if allowed_file(filename):
                all_files.append({
                    'filename': filename,
                    'url': f"/api/images/{filename}"
                })
        
        # Sort by filename (timestamp) in reverse order
        all_files.sort(key=lambda x: x['filename'], reverse=True)
        
        # Take latest 30
        latest_images = all_files[:30]
        
        return render_template_string(GALLERY_TEMPLATE, 
                                     images=latest_images, 
                                     total=len(all_files))
        
    except Exception as e:
        logger.error(f"Failed to render gallery: {e}")
        return render_template_string(GALLERY_TEMPLATE, images=[], total=0)


if __name__ == '__main__':
    # Initialize database
    init_db()
    
    logger.info(f"Starting Flask server on {FLASK_HOST}:{FLASK_PORT}")
    logger.info(f"Upload folder: {UPLOAD_FOLDER}")
    logger.info(f"Max content length: {MAX_CONTENT_LENGTH} bytes")
    
    app.run(host=FLASK_HOST, port=FLASK_PORT, debug=FLASK_DEBUG)
