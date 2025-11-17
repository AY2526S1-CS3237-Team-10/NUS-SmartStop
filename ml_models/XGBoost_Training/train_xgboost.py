"""
XGBoost Regressor for Bus Stop Capacity Prediction
Predicts actual bus stop capacity using multi-sensor data

Input Features (X-values):
1. Ultrasonic Sensors (3 positions):
   - sensors_CENTER_distance (float): CENTER sensor distance in cm, -1 = no data
   - sensors_CENTER_occupied (0/1/-1): CENTER occupied status, -1 = no data
   - sensors_LEFT_distance (float): LEFT sensor distance in cm, -1 = no data
   - sensors_LEFT_occupied (0/1/-1): LEFT occupied status, -1 = no data
   - sensors_RIGHT_distance (float): RIGHT sensor distance in cm, -1 = no data
   - sensors_RIGHT_occupied (0/1/-1): RIGHT occupied status, -1 = no data
2. IR Sensor: people_count (float, -1 = no data)
3. Microphone: voice (0/1/-1, -1 = no data)
4. ESP32 Camera: crowd_level (0-4, -1 = no data), confidence (0-100, -1 = no data)
5. Density: density (float, -1 = no data)

Note: -1 indicates missing sensor data at that timestamp (not sensor malfunction)

Output (Y-value):
- actual_capacity: Actual capacity of bus stop (float)

Model: XGBoost Regressor (Gradient Boosting)
"""

import pandas as pd
import numpy as np
import matplotlib.pyplot as plt
import seaborn as sns
from sklearn.model_selection import train_test_split
from sklearn.metrics import mean_absolute_error, mean_squared_error, r2_score, mean_absolute_percentage_error
from sklearn.preprocessing import StandardScaler
import xgboost as xgb
import joblib
import argparse
from datetime import datetime


class BusStopXGBoostPredictor:
    """XGBoost model for bus stop capacity prediction"""
    
    def __init__(self, n_estimators=100, max_depth=6, learning_rate=0.1, 
                 random_state=10):
        """
        Initialize XGBoost Regressor
        
        Args:
            n_estimators: Number of boosting rounds (trees)
            max_depth: Maximum tree depth
            learning_rate: Step size shrinkage (0.01-0.3)
            random_state: Random seed for reproducibility
        """
        self.n_estimators = n_estimators
        self.max_depth = max_depth
        self.learning_rate = learning_rate
        self.random_state = random_state
        
        # Initialize model
        self.model = xgb.XGBRegressor(
            n_estimators=n_estimators,
            max_depth=max_depth,
            learning_rate=learning_rate,
            random_state=random_state,
            objective='reg:squarederror',  # Regression task
            eval_metric='rmse'
        )
        
        # Scaler (not needed for XGBoost, but kept for compatibility)
        self.scaler = StandardScaler()
        self.use_scaling = False  # XGBoost doesn't need feature scaling
        
        # Feature names
        self.feature_columns = None
        self.target_column = None
        
        print(f"Number of trees: {n_estimators}")
        print(f"Max depth: {max_depth}")
        print(f"Learning rate: {learning_rate}")
        print(f"Random state: {random_state}")
        print()
    
    def prepare_data(self, data, target_col='actual_capacity', test_size=0.2):
        """
        Prepare data for training
        
        Expected columns in data (all use -1 for missing values):
        - sensors_CENTER_distance (float): Ultrasonic CENTER distance, -1 = no data
        - sensors_CENTER_occupied (int): Ultrasonic CENTER occupied (0/1/-1)
        - sensors_LEFT_distance (float): Ultrasonic LEFT distance, -1 = no data
        - sensors_LEFT_occupied (int): Ultrasonic LEFT occupied (0/1/-1)
        - sensors_RIGHT_distance (float): Ultrasonic RIGHT distance, -1 = no data
        - sensors_RIGHT_occupied (int): Ultrasonic RIGHT occupied (0/1/-1)
        - people_count (float): IR sensor people count, -1 = no data
        - voice (int): Microphone voice detection (0/1/-1)
        - crowd_level (int): ESP32 camera crowd level (0-4), -1 = no data
        - confidence (float): ESP32 camera confidence score (0-100), -1 = no data
        - density (float): People density metric, -1 = no data
        - actual_capacity (float): Target variable
        
        Optional time features (if available):
        - hour, day_of_week, is_weekend, is_business_hours, is_rush_hour
        
        Note: -1 values are kept as-is for XGBoost to learn patterns from missing data
        
        Args:
            data: DataFrame with sensor data
            target_col: Target column name
            test_size: Fraction of data for testing (0.0-1.0)
        
        Returns:
            X_train, X_test, y_train, y_test
        """
        print("Preparing data for XGBoost")
        print(f"Total samples: {len(data)}")
        print(f"Target column: {target_col}")
        print()
        
        # Check for required columns
        required_features = ['sensors_CENTER_distance', 'sensors_CENTER_occupied',
                           'sensors_LEFT_distance', 'sensors_LEFT_occupied',
                           'sensors_RIGHT_distance', 'sensors_RIGHT_occupied',
                           'people_count', 'voice', 'crowd_level', 'confidence', 'density']
        
        missing_features = [f for f in required_features if f not in data.columns]
        if missing_features:
            print(f"   Missing required features: {missing_features}")
            print(f"   Available columns: {data.columns.tolist()}")
        
        # Get all feature columns (exclude target and metadata like _time)
        self.target_column = target_col
        exclude_cols = [target_col, '_time', 'time', 'timestamp', 'index']
        self.feature_columns = [col for col in data.columns 
                               if col not in exclude_cols]
        
        print(f"Input features ({len(self.feature_columns)}):")
        for col in self.feature_columns:
            dtype = data[col].dtype
            unique_vals = data[col].nunique()
            has_minus_one = (data[col] == -1).sum()
            pct_minus_one = (has_minus_one / len(data) * 100) if len(data) > 0 else 0
            print(f"   - {col:30s} (type: {dtype}, unique: {unique_vals:3d}, -1: {pct_minus_one:5.1f}%)")
        
        print(f"\nTarget: {target_col}")
        if target_col in data.columns:
            print(f"   - Min: {data[target_col].min():.2f}")
            print(f"   - Max: {data[target_col].max():.2f}")
            print(f"   - Mean: {data[target_col].mean():.2f}")
            print(f"   - Std: {data[target_col].std():.2f}")
        else:
            raise ValueError(f"Target column '{target_col}' not found in data!")
        print()
        
        # Separate features and target
        X = data[self.feature_columns].copy()
        y = data[target_col].copy()
        
        # Check for NaN values (shouldn't exist if data is properly prepared)
        if X.isnull().sum().sum() > 0:
            print("Found NaN values in data (should be -1 instead)!")
            print("Filling NaN with -1...")
            X = X.fillna(-1)
        
        # Report on -1 values (missing data indicator)
        minus_one_counts = (X == -1).sum()
        if minus_one_counts.sum() > 0:
            print("📊 Missing data summary (-1 values):")
            for col in minus_one_counts[minus_one_counts > 0].index:
                count = minus_one_counts[col]
                pct = (count / len(X) * 100)
                print(f"   - {col:30s}: {count:4d} ({pct:5.1f}%)")
            print("   Note: XGBoost will learn to handle -1 as 'no data' indicator")
            print()
        
        # Split data
        X_train, X_test, y_train, y_test = train_test_split(
            X, y, test_size=test_size, random_state=self.random_state
        )
        
        # Scale features (optional but helps with convergence)
        if self.use_scaling:
            print(" Scaling features...")
            X_train = self.scaler.fit_transform(X_train)
            X_test = self.scaler.transform(X_test)
            
            # Convert back to DataFrame with column names
            X_train = pd.DataFrame(X_train, columns=self.feature_columns)
            X_test = pd.DataFrame(X_test, columns=self.feature_columns)
        
        print(f"Data split:")
        print(f"   Training samples: {len(X_train)}")
        print(f"   Testing samples: {len(X_test)}")
        print(f"   Test size: {test_size*100:.0f}%")
        print()
        
        return X_train, X_test, y_train, y_test
    
    def train(self, X_train, y_train, X_val=None, y_val=None, verbose=True):
        """
        Train the XGBoost model
        
        Args:
            X_train: Training features
            y_train: Training targets
            X_val: Validation features (optional)
            y_val: Validation targets (optional)
            verbose: Print training progress
        
        Returns:
            Training history
        """
        
        eval_set = [(X_train, y_train)]
        if X_val is not None and y_val is not None:
            eval_set.append((X_val, y_val))
        
        # Train model
        self.model.fit(X_train, y_train, eval_set=eval_set, verbose=verbose)
        
        print("\nTraining complete!")
        print()
        
        return self.model
    
    def predict(self, X):
        if self.use_scaling and isinstance(X, pd.DataFrame):
            X_scaled = self.scaler.transform(X)
            X_scaled = pd.DataFrame(X_scaled, columns=self.feature_columns)
            return self.model.predict(X_scaled)
        return self.model.predict(X)
    
    def evaluate(self, X_test, y_test):
        """
        Evaluate model performance
        
        Args:
            X_test: Test features
            y_test: True target values
        
        Returns:
            Dictionary with metrics and predictions
        """
        
        # Make predictions
        y_pred = self.predict(X_test)
        
        # Calculate metrics
        mae = mean_absolute_error(y_test, y_pred)
        mse = mean_squared_error(y_test, y_pred)
        rmse = np.sqrt(mse)
        r2 = r2_score(y_test, y_pred)
        
        # MAPE (handle division by zero)
        mask = y_test != 0
        if mask.sum() > 0:
            mape = mean_absolute_percentage_error(y_test[mask], y_pred[mask]) * 100
        else:
            mape = float('inf')
        
        metrics = {
            'MAE': mae,
            'MSE': mse,
            'RMSE': rmse,
            'R2': r2,
            'MAPE': mape
        }
        
        print(f"\n Performance Metrics:")
        print(f"   MAE (Mean Absolute Error): {mae:.4f}")
        print(f"   RMSE (Root Mean Squared Error): {rmse:.4f}")
        print(f"   R² Score: {r2:.4f}")
        print(f"   MAPE: {mape:.2f}%")
        print()
        
        return metrics, y_pred
    
    def get_feature_importance(self, plot=True, top_n=None):
        """
        Get feature importance scores
        
        Args:
            plot: Whether to plot feature importance
            top_n: Show only top N features (None = all)
        
        Returns:
            DataFrame with feature importance
        """
        
        # Get importance scores directly from model
        importance_scores = self.model.feature_importances_
        
        # Create feature importance list
        feature_importance = []
        for i, col in enumerate(self.feature_columns):
            score = importance_scores[i] if i < len(importance_scores) else 0
            feature_importance.append({'feature': col, 'importance': score})
        
        # Create DataFrame and sort
        importance_df = pd.DataFrame(feature_importance)
        importance_df = importance_df.sort_values('importance', ascending=False)
        
        if top_n:
            importance_df = importance_df.head(top_n)
        
        print("\nFeature Importance:")
        for idx, row in importance_df.iterrows():
            print(f"   {row['feature']:30s}: {row['importance']:.4f}")
        print()
        
        # Plot
        if plot and len(importance_df) > 0:
            plt.figure(figsize=(10, 6))
            plt.barh(importance_df['feature'], importance_df['importance'])
            plt.xlabel('Importance Score')
            plt.title('XGBoost Feature Importance')
            plt.gca().invert_yaxis()
            plt.tight_layout()
            plt.savefig('feature_importance.png', dpi=150, bbox_inches='tight')
            print("Feature importance plot saved: feature_importance.png")
            print()
        
        return importance_df
    
    def plot_predictions(self, y_true, y_pred, output_file='predictions.png'):
        """Visualize predictions vs actual values"""
        
        fig, axes = plt.subplots(2, 2, figsize=(15, 12))
        
        # Plot 1: Predictions vs Actual (scatter)
        axes[0, 0].scatter(y_true, y_pred, alpha=0.5, s=20)
        axes[0, 0].plot([y_true.min(), y_true.max()], 
                        [y_true.min(), y_true.max()], 
                        'r--', lw=2, label='Perfect Prediction')
        axes[0, 0].set_xlabel('Actual Capacity')
        axes[0, 0].set_ylabel('Predicted Capacity')
        axes[0, 0].set_title('Predicted vs Actual Capacity')
        axes[0, 0].legend()
        axes[0, 0].grid(True, alpha=0.3)
        
        # Plot 2: Time series comparison
        axes[0, 1].plot(y_true.values, label='Actual', alpha=0.7, linewidth=2)
        axes[0, 1].plot(y_pred, label='Predicted', alpha=0.7, linewidth=2)
        axes[0, 1].set_xlabel('Sample Index')
        axes[0, 1].set_ylabel('Capacity')
        axes[0, 1].set_title('Actual vs Predicted Over Time')
        axes[0, 1].legend()
        axes[0, 1].grid(True, alpha=0.3)
        
        # Plot 3: Residuals (errors)
        residuals = y_true.values - y_pred
        axes[1, 0].scatter(y_pred, residuals, alpha=0.5, s=20)
        axes[1, 0].axhline(y=0, color='r', linestyle='--', linewidth=2)
        axes[1, 0].set_xlabel('Predicted Capacity')
        axes[1, 0].set_ylabel('Residuals (Actual - Predicted)')
        axes[1, 0].set_title('Residual Plot')
        axes[1, 0].grid(True, alpha=0.3)
        
        # Plot 4: Error distribution
        axes[1, 1].hist(residuals, bins=50, alpha=0.7, edgecolor='black')
        axes[1, 1].axvline(x=0, color='r', linestyle='--', linewidth=2)
        axes[1, 1].set_xlabel('Residuals')
        axes[1, 1].set_ylabel('Frequency')
        axes[1, 1].set_title('Distribution of Prediction Errors')
        axes[1, 1].grid(True, alpha=0.3)
        
        plt.tight_layout()
        plt.savefig(output_file, dpi=150, bbox_inches='tight')
        
        print(f"Prediction plots saved: {output_file}")
        print()
    
    def save_model(self, model_path='busstop_xgboost.json', 
                   scaler_path='xgboost_scaler.pkl'):
        """Save trained model and scaler"""
        # Save XGBoost model
        self.model.save_model(model_path)
        
        # Save scaler and metadata
        save_dict = {
            'scaler': self.scaler,
            'feature_columns': self.feature_columns,
            'target_column': self.target_column,
            'use_scaling': self.use_scaling,
            'n_estimators': self.n_estimators,
            'max_depth': self.max_depth,
            'learning_rate': self.learning_rate
        }
        joblib.dump(save_dict, scaler_path)
        
        print(f"Model saved to: {model_path}")
        print(f"Scaler & metadata saved to: {scaler_path}")
        print()
    
    def load_model(self, model_path='busstop_xgboost.json', 
                   scaler_path='xgboost_scaler.pkl'):
        """Load trained model and scaler"""
        # Load XGBoost model
        self.model = xgb.XGBRegressor()
        self.model.load_model(model_path)
        
        # Load scaler and metadata
        save_dict = joblib.load(scaler_path)
        self.scaler = save_dict['scaler']
        self.feature_columns = save_dict['feature_columns']
        self.target_column = save_dict['target_column']
        self.use_scaling = save_dict['use_scaling']
        
        print(f"Model loaded from: {model_path}")
        print(f"Scaler & metadata loaded from: {scaler_path}")
        print()


def main():
    parser = argparse.ArgumentParser(
        description='Train XGBoost model for bus stop capacity prediction'
    )
    parser.add_argument('data_file', type=str, 
                       help='CSV file with sensor data')
    parser.add_argument('--target', type=str, default='actual_capacity',
                       help='Target column name (default: actual_capacity)')
    parser.add_argument('--test-size', type=float, default=0.2,
                       help='Test set size (default: 0.2 = 20%%)')
    parser.add_argument('--n-estimators', type=int, default=100,
                       help='Number of trees (default: 100)')
    parser.add_argument('--max-depth', type=int, default=6,
                       help='Maximum tree depth (default: 6)')
    parser.add_argument('--learning-rate', type=float, default=0.1,
                       help='Learning rate (default: 0.1)')
    parser.add_argument('--early-stopping', type=int, default=10,
                       help='Early stopping rounds (default: 10)')
    parser.add_argument('--no-scaling', action='store_true',
                       help='Disable feature scaling')
    parser.add_argument('--output', type=str, default='busstop_xgboost.json',
                       help='Output model filename')
    
    args = parser.parse_args()
    
    # 1. Load data
    print("Loading data...")
    df = pd.read_csv(args.data_file)
    
    # If first column is datetime, set it as index but keep it as a column too
    if df.columns[0] in ['_time', 'time', 'timestamp']:
        df.index = pd.to_datetime(df.iloc[:, 0])
        # Don't drop the _time column, let prepare_data handle it
    
    print(f"Loaded {len(df)} samples from {args.data_file}")
    print(f"Columns: {list(df.columns)}")
    print()
    
    # 2. Initialize model
    xgb_model = BusStopXGBoostPredictor(
        n_estimators=args.n_estimators,
        max_depth=args.max_depth,
        learning_rate=args.learning_rate
    )
    
    if args.no_scaling:
        xgb_model.use_scaling = False
        print("Feature scaling disabled")
        print()
    
    # 3. Prepare data
    X_train, X_test, y_train, y_test = xgb_model.prepare_data(
        df, target_col=args.target, test_size=args.test_size
    )
    
    # 4. Train model directly on full training set
    xgb_model.train(X_train, y_train)
    
    # 5. Evaluate on test set
    metrics, y_pred = xgb_model.evaluate(X_test, y_test)
    
    # 6. Feature importance
    importance_df = xgb_model.get_feature_importance(plot=True)
    
    # 7. Plot predictions
    xgb_model.plot_predictions(y_test, y_pred)
    
    # 8. Save model
    model_filename = args.output
    scaler_filename = model_filename.replace('.json', '_scaler.pkl')
    xgb_model.save_model(model_filename, scaler_filename)
    
    print("\n Output files:")
    print(f"   - {model_filename} (trained XGBoost model)")
    print(f"   - {scaler_filename} (scaler & metadata)")
    print("   - predictions.png (prediction plots)")
    print("   - feature_importance.png (feature importance)")
    print("\n Model Performance:")
    for metric, value in metrics.items():
        if metric == 'MAPE':
            print(f"   {metric}: {value:.2f}%")
        else:
            print(f"   {metric}: {value:.4f}")
    print("=" * 70)


if __name__ == "__main__":
    main()
