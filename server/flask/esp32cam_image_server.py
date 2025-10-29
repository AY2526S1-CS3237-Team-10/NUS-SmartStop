"""
Minimal Auto Monitor - Polls directory for new images and runs analysis
Calls run_analysis.py for each new image (publishes to MQTT automatically)
"""

import os
import time
import json
import subprocess
from pathlib import Path
from datetime import datetime

# ============================================================================
# CONFIGURATION
# ============================================================================
REFERENCE_IMAGE_PATH = "images/reference_image.jpg"
WATCH_DIRECTORY = "images"
POLL_INTERVAL = 60  # Check for new images every 1 minute
PROCESSED_HISTORY_FILE = "processed_files.json"


# ============================================================================
# FILE TRACKING
# ============================================================================
def load_processed_files():
    """Load list of already processed files"""
    if os.path.exists(PROCESSED_HISTORY_FILE):
        try:
            with open(PROCESSED_HISTORY_FILE, 'r') as f:
                data = json.load(f)
                return set(data.get('files', []))
        except:
            pass
    return set()


def save_processed_files(processed_set):
    """Save list of processed files"""
    try:
        with open(PROCESSED_HISTORY_FILE, 'w') as f:
            json.dump({
                'files': list(processed_set),
                'last_updated': datetime.now().isoformat()
            }, f)
    except Exception as e:
        print(f"Warning: Could not save history: {e}")


def get_new_images(watch_dir, processed_set, reference_path):
    """Find new JPG images that haven't been processed"""
    new_images = []
    
    try:
        for file in os.listdir(watch_dir):
            if file.lower().endswith(('.jpg', '.jpeg')):
                file_path = os.path.join(watch_dir, file)
                
                # Skip if already processed
                if file_path in processed_set:
                    continue
                
                # Skip reference image
                if os.path.abspath(file_path) == os.path.abspath(reference_path):
                    continue
                
                # Skip if it's a directory
                if os.path.isdir(file_path):
                    continue
                
                new_images.append(file_path)
    
    except Exception as e:
        print(f"Error scanning directory: {e}")
    
    return new_images


# ============================================================================
# MAIN MONITORING LOOP
# ============================================================================
def main():
    print("=" * 70)
    print("🔍 MINIMAL AUTO MONITOR")
    print("=" * 70)
    print(f"Reference: {REFERENCE_IMAGE_PATH}")
    print(f"Watch Dir: {WATCH_DIRECTORY}")
    print(f"Poll Interval: {POLL_INTERVAL}s ({POLL_INTERVAL//60} minute(s))")
    print("=" * 70)
    
    # Check reference image exists
    if not os.path.exists(REFERENCE_IMAGE_PATH):
        print(f"\n❌ Reference image not found: {REFERENCE_IMAGE_PATH}")
        print(f"\nTo debug, run these commands:")
        print(f"  pwd                    # Check current directory")
        print(f"  ls images/             # Check images directory")
        print(f"  ls {REFERENCE_IMAGE_PATH}  # Check reference image")
        return 1
    
    # Check if run_analysis.py exists
    if not os.path.exists("run_analysis.py"):
        print(f"\n❌ run_analysis.py not found in current directory")
        return 1
    
    print("✅ Reference image found")
    print("✅ run_analysis.py found")
    
    # Load processed files history
    processed_files = load_processed_files()
    print(f"📂 Loaded {len(processed_files)} previously processed files")
    
    # On first run, mark all existing images as processed (don't analyze them)
    if len(processed_files) == 0:
        print("\n🔄 First run detected - marking all existing images as processed...")
        existing_images = get_new_images(WATCH_DIRECTORY, processed_files, REFERENCE_IMAGE_PATH)
        
        if existing_images:
            for img in existing_images:
                processed_files.add(img)
            save_processed_files(processed_files)
            print(f"✅ Marked {len(existing_images)} existing images as processed (will not analyze)")
        else:
            print("✅ No existing images found")
    
    print("\n✅ Monitoring started. Press Ctrl+C to stop...\n")
    
    try:
        while True:
            # Check for new images
            new_images = get_new_images(WATCH_DIRECTORY, processed_files, REFERENCE_IMAGE_PATH)
            
            # Process each new image
            for image_path in new_images:
                print(f"🔍 New image: {os.path.basename(image_path)}")
                
                try:
                    # Call run_analysis.py with subprocess
                    result = subprocess.run(
                        ['python', 'run_analysis.py', REFERENCE_IMAGE_PATH, image_path],
                        capture_output=True,
                        text=True,
                        timeout=30  # 30 second timeout
                    )
                    
                    # Check if successful
                    if result.returncode == 0:
                        print(f"  ✅ Analysis complete and published to MQTT")
                        
                        # Mark as processed
                        processed_files.add(image_path)
                        save_processed_files(processed_files)
                    else:
                        print(f"  ❌ Analysis failed (exit code: {result.returncode})")
                        if result.stderr:
                            print(f"  Error: {result.stderr[:200]}")
                    
                    print()  # Blank line for readability
                    
                except subprocess.TimeoutExpired:
                    print(f"  ⚠️ Analysis timeout (>30s)")
                except Exception as e:
                    print(f"  ❌ Error processing {os.path.basename(image_path)}: {e}")
            
            # Sleep before next check
            time.sleep(POLL_INTERVAL)
            
    except KeyboardInterrupt:
        print("\n\n🛑 Monitoring stopped by user")
        return 0
    except Exception as e:
        print(f"\n❌ Fatal error: {e}")
        return 1


if __name__ == "__main__":
    exit(main())
