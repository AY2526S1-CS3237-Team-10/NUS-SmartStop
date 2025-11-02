"""
ESP32 Camera Image Analysis - Area Coverage Detection
Compares captured images with reference image to detect fill percentage
Uses multiple methods: pixel difference, contour detection, and object counting
"""

import cv2
import numpy as np
from pathlib import Path
import json
from datetime import datetime

class ImageAnalyzer:
    def __init__(self, reference_image_path):
        """
        Initialize analyzer with reference image
        
        Args:
            reference_image_path: Path to reference (empty/baseline) image
        """
        self.reference_image = cv2.imread(reference_image_path)
        if self.reference_image is None:
            raise ValueError(f"Could not load reference image: {reference_image_path}")
        
        # Convert to grayscale for analysis
        self.reference_gray = cv2.cvtColor(self.reference_image, cv2.COLOR_BGR2GRAY)
        self.height, self.width = self.reference_gray.shape
        print(f"Reference image loaded: {self.width}x{self.height}")
    
    def analyze_image(self, test_image_path, method='all'):
        """
        Analyze test image against reference
        
        Args:
            test_image_path: Path to captured image to analyze
            method: 'pixel', 'contour', 'object', or 'all'
        
        Returns:
            dict with analysis results
        """
        test_image = cv2.imread(test_image_path)
        if test_image is None:
            return {"error": f"Could not load test image: {test_image_path}"}
        
        # Resize test image to match reference if needed
        if test_image.shape[:2] != self.reference_image.shape[:2]:
            test_image = cv2.resize(test_image, (self.width, self.height))
        
        test_gray = cv2.cvtColor(test_image, cv2.COLOR_BGR2GRAY)
        
        results = {
            "timestamp": datetime.now().isoformat(),
            "test_image": test_image_path,
            "image_size": f"{self.width}x{self.height}"
        }
        
        if method in ['pixel', 'all']:
            results['pixel_analysis'] = self._analyze_pixel_difference(test_gray)
        
        if method in ['contour', 'all']:
            results['contour_analysis'] = self._analyze_contours(test_image, test_gray)
        
        if method in ['object', 'all']:
            results['object_analysis'] = self._analyze_objects(test_image, test_gray)
        
        return results
    
    def _analyze_pixel_difference(self, test_gray):
        """
        Calculate coverage based on pixel-level differences
        """
        # Calculate absolute difference between images
        diff = cv2.absdiff(self.reference_gray, test_gray)
        
        # Apply threshold to get binary difference
        _, thresh = cv2.threshold(diff, 30, 255, cv2.THRESH_BINARY)
        
        # Calculate percentage of changed pixels
        changed_pixels = np.count_nonzero(thresh)
        total_pixels = self.width * self.height
        coverage_percentage = (changed_pixels / total_pixels) * 100
        
        return {
            "method": "Pixel Difference",
            "coverage_percentage": round(coverage_percentage, 2),
            "changed_pixels": int(changed_pixels),
            "total_pixels": int(total_pixels),
            "description": "Percentage of pixels that differ from reference"
        }
    
    def _analyze_contours(self, test_image, test_gray):
        """
        Calculate coverage based on detected contours (shapes/objects)
        """
        # Calculate difference
        diff = cv2.absdiff(self.reference_gray, test_gray)
        
        # Apply threshold and morphological operations
        _, thresh = cv2.threshold(diff, 30, 255, cv2.THRESH_BINARY)
        kernel = np.ones((5, 5), np.uint8)
        thresh = cv2.morphologyEx(thresh, cv2.MORPH_CLOSE, kernel)
        thresh = cv2.morphologyEx(thresh, cv2.MORPH_OPEN, kernel)
        
        # Find contours
        contours, _ = cv2.findContours(thresh, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)
        
        # Calculate total area of contours
        total_contour_area = sum(cv2.contourArea(cnt) for cnt in contours)
        total_area = self.width * self.height
        coverage_percentage = (total_contour_area / total_area) * 100
        
        # Filter significant contours (area > 100 pixels)
        significant_contours = [cnt for cnt in contours if cv2.contourArea(cnt) > 100]
        
        return {
            "method": "Contour Detection",
            "coverage_percentage": round(coverage_percentage, 2),
            "total_contours": len(contours),
            "significant_contours": len(significant_contours),
            "filled_area_pixels": int(total_contour_area),
            "total_area_pixels": int(total_area),
            "description": "Percentage based on detected object contours"
        }
    
    def _analyze_objects(self, test_image, test_gray):
        """
        Count and analyze distinct objects in the image
        """
        # Calculate difference
        diff = cv2.absdiff(self.reference_gray, test_gray)
        
        # Apply adaptive threshold for better object detection
        thresh = cv2.adaptiveThreshold(diff, 255, cv2.ADAPTIVE_THRESH_GAUSSIAN_C,
                                       cv2.THRESH_BINARY, 11, 2)
        
        # Apply morphological operations to clean up
        kernel = np.ones((3, 3), np.uint8)
        thresh = cv2.morphologyEx(thresh, cv2.MORPH_CLOSE, kernel, iterations=2)
        thresh = cv2.morphologyEx(thresh, cv2.MORPH_OPEN, kernel, iterations=1)
        
        # Find contours (objects)
        contours, _ = cv2.findContours(thresh, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)
        
        # Filter objects by minimum size
        min_object_area = 200  # Minimum 200 pixels to be considered an object
        objects = []
        total_object_area = 0
        
        for cnt in contours:
            area = cv2.contourArea(cnt)
            if area >= min_object_area:
                x, y, w, h = cv2.boundingRect(cnt)
                objects.append({
                    "area": int(area),
                    "position": {"x": int(x), "y": int(y)},
                    "size": {"width": int(w), "height": int(h)}
                })
                total_object_area += area
        
        total_area = self.width * self.height
        coverage_percentage = (total_object_area / total_area) * 100
        
        return {
            "method": "Object Detection",
            "coverage_percentage": round(coverage_percentage, 2),
            "object_count": len(objects),
            "objects": objects,
            "min_object_size": min_object_area,
            "description": f"Detected {len(objects)} distinct objects"
        }
    
    def create_visualization(self, test_image_path, output_path='analysis_result.jpg'):
        """
        Create a visualization showing the difference between reference and test image
        """
        test_image = cv2.imread(test_image_path)
        if test_image is None:
            return None
        
        if test_image.shape[:2] != self.reference_image.shape[:2]:
            test_image = cv2.resize(test_image, (self.width, self.height))
        
        test_gray = cv2.cvtColor(test_image, cv2.COLOR_BGR2GRAY)
        
        # Calculate difference
        diff = cv2.absdiff(self.reference_gray, test_gray)
        _, thresh = cv2.threshold(diff, 30, 255, cv2.THRESH_BINARY)
        
        # Create colored difference overlay
        diff_colored = cv2.cvtColor(thresh, cv2.COLOR_GRAY2BGR)
        diff_colored[:, :, 0] = 0  # Remove blue channel
        diff_colored[:, :, 1] = 0  # Remove green channel
        # Red channel remains, highlighting differences in red
        
        # Overlay on test image
        result = cv2.addWeighted(test_image, 0.7, diff_colored, 0.3, 0)
        
        # Find contours for visualization
        contours, _ = cv2.findContours(thresh, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)
        cv2.drawContours(result, contours, -1, (0, 255, 0), 2)
        
        # Create side-by-side comparison
        comparison = np.hstack([self.reference_image, test_image, result])
        
        cv2.imwrite(output_path, comparison)
        print(f"Visualization saved to: {output_path}")
        return output_path


def main():
    """
    Example usage of ImageAnalyzer
    """
    import sys
    
    if len(sys.argv) < 3:
        print("Usage: python image_analysis.py <reference_image.jpg> <test_image.jpg>")
        print("\nExample:")
        print("  python image_analysis.py empty_container.jpg captured_image.jpg")
        return
    
    reference_path = sys.argv[1]
    test_path = sys.argv[2]
    
    print("=" * 60)
    print("ESP32 Camera Image Analysis - Area Coverage Detection")
    print("=" * 60)
    
    try:
        # Initialize analyzer
        analyzer = ImageAnalyzer(reference_path)
        
        # Analyze image
        print(f"\nAnalyzing: {test_path}")
        results = analyzer.analyze_image(test_path, method='all')
        
        # Print results
        print("\n" + "=" * 60)
        print("ANALYSIS RESULTS")
        print("=" * 60)
        
        if 'pixel_analysis' in results:
            print("\n1. PIXEL DIFFERENCE ANALYSIS:")
            pa = results['pixel_analysis']
            print(f"   Coverage: {pa['coverage_percentage']}%")
            print(f"   Changed Pixels: {pa['changed_pixels']:,} / {pa['total_pixels']:,}")
        
        if 'contour_analysis' in results:
            print("\n2. CONTOUR ANALYSIS:")
            ca = results['contour_analysis']
            print(f"   Coverage: {ca['coverage_percentage']}%")
            print(f"   Total Contours: {ca['total_contours']}")
            print(f"   Significant Objects: {ca['significant_contours']}")
        
        if 'object_analysis' in results:
            print("\n3. OBJECT DETECTION:")
            oa = results['object_analysis']
            print(f"   Coverage: {oa['coverage_percentage']}%")
            print(f"   Objects Detected: {oa['object_count']}")
            if oa['object_count'] > 0:
                print(f"   Largest Object: {max(obj['area'] for obj in oa['objects']):,} pixels")
        
        # Save detailed results to JSON
        output_json = test_path.replace('.jpg', '_analysis.json')
        with open(output_json, 'w') as f:
            json.dump(results, f, indent=2)
        print(f"\nDetailed results saved to: {output_json}")
        
        # Create visualization
        viz_path = test_path.replace('.jpg', '_visualization.jpg')
        analyzer.create_visualization(test_path, viz_path)
        
        print("\n" + "=" * 60)
        
    except Exception as e:
        print(f"Error: {e}")
        import traceback
        traceback.print_exc()


if __name__ == "__main__":
    main()
