# NUS-SmartStop

Flask-based image server for IoT devices with gallery UI and metadata persistence.

## Features

- **Dual Upload Support**: Accepts both multipart/form-data and raw body uploads (ESP32 compatible)
- **Secure File Handling**: Uses secure filenames, UTC timestamps, and validates image files
- **Gallery UI**: Web interface showing latest uploaded images
- **Metadata Persistence**: SQLite database for tracking uploads
- **CORS Enabled**: Cross-origin requests supported
- **ML Inference Endpoint**: Stub endpoint for future ML integration
- **Environment Configuration**: Flexible configuration via environment variables

## Quick Start

1. **Install dependencies**:
   ```bash
   pip install -r requirements.txt
   ```

2. **Configure environment** (optional):
   ```bash
   cp .env.example .env
   # Edit .env with your settings
   ```

3. **Run the server**:
   ```bash
   python server/flask/app.py
   ```

4. **Access the gallery**:
   Open http://localhost:5000 in your browser

## API Endpoints

### Health Check
```bash
GET /health
```
Returns server status, image count, and disk space.

### Upload Image (Multipart)
```bash
curl -F "image=@photo.jpg" http://localhost:5000/api/upload
```

### Upload Image (Raw Body - ESP32 Compatible)
```bash
curl -X POST -H "Device-ID: ESP32_001" \
     -H "Content-Type: image/jpeg" \
     --data-binary "@photo.jpg" \
     http://localhost:5000/api/upload
```

### List Images
```bash
GET /api/images?limit=50&offset=0
```
Supports pagination with `limit` and `offset` query parameters.

### Get Image
```bash
GET /api/images/<filename>
```

### ML Inference (Stub)
```bash
POST /api/inference
Content-Type: application/json

{"filename": "image.jpg"}
```

## Configuration

Environment variables (see `.env.example`):

- `UPLOAD_FOLDER`: Directory for uploaded images (default: `./uploads`)
- `MAX_CONTENT_LENGTH`: Maximum upload size in bytes (default: 16777216 = 16MB)
- `FLASK_HOST`: Server host (default: `0.0.0.0`)
- `FLASK_PORT`: Server port (default: `5000`)
- `FLASK_DEBUG`: Debug mode (default: `False`)

## File Naming Convention

- **Multipart uploads**: `{timestamp}_{secure_filename}`
- **Raw body uploads**: `{device_id}_{timestamp}.jpg`

Timestamps use UTC in format: `YYYYMMDD_HHMMSS`

## Security Features

- Validates uploaded files are actual images using Pillow
- Sanitizes filenames using `secure_filename()`
- Enforces allowed file extensions: png, jpg, jpeg, gif
- Content-length limits configurable via `MAX_CONTENT_LENGTH`
- CORS enabled for cross-origin access

## Database

Uses SQLite (`server/flask/metadata.db`) to store:
- Upload timestamp
- Filename
- File size in bytes
- Device ID (if provided)

## Gallery

The root endpoint (`/`) displays a responsive gallery of the latest 30 images with:
- Grid layout
- Image thumbnails
- Filename display
- Click to view full image