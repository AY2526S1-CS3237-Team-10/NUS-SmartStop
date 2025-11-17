"""
Simple Inference Script for Bus Stop Capacity Prediction
"""

import pandas as pd
import xgboost as xgb
import joblib

class BusStopPredictor:
    """Simple wrapper for bus stop capacity prediction"""
    
    def __init__(self, model_path='busstop_xgboost.json', scaler_path='busstop_xgboost_scaler.pkl'):
        """Load trained model and scaler"""
        # Load XGBoost model
        self.model = xgb.XGBRegressor()
        self.model.load_model(model_path)
        
        # Load scaler and metadata
        scaler_data = joblib.load(scaler_path)
        self.scaler = scaler_data['scaler']
        self.feature_columns = scaler_data['feature_columns']
        self.use_scaling = scaler_data['use_scaling']
    
    def predict(self, sensor_data):
        """
        Make prediction from sensor data
        
        Args:
            sensor_data: dict, Series, or DataFrame with sensor readings
        
        Returns:
            predicted_capacity (float): Predicted number of people
        """
        # Convert to DataFrame if needed
        if isinstance(sensor_data, dict):
            df = pd.DataFrame([sensor_data])
        elif isinstance(sensor_data, pd.Series):
            df = pd.DataFrame([sensor_data.to_dict()])
        else:
            df = sensor_data.copy()
        
        # Remove timestamp if present
        if '_time' in df.columns:
            df = df.drop('_time', axis=1)
        
        # Ensure correct column order
        df = df[self.feature_columns]
        
        # Make prediction
        prediction = self.model.predict(df)
        
        return prediction[0]
    
    def predict_from_csv(self, csv_file):
        """Predict from CSV file"""
        df = pd.read_csv(csv_file)
        return self.predict(df)


def main():
    """Example usage"""
    predictor = BusStopPredictor()
    
    try:
        prediction = predictor.predict_from_csv('single_sample.csv')
        print(f"Predicted capacity: {prediction:.1f} people")
    except FileNotFoundError:
        print("Error: single_sample.csv not found")


if __name__ == "__main__":
    main()
