#include <Arduino.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// Initialize LCD with I2C address 0x27 (common address, might also be 0x3F)
// 16 columns and 2 rows (adjust if your LCD is different, e.g., 20x4)
LiquidCrystal_I2C lcd(0x27, 16, 2);

void setup() {
  // Initialize Serial for debugging
  Serial.begin(115200);
  Serial.println("LCD I2C Test Starting...");
  
  // Initialize I2C with default ESP32 pins (SDA=21, SCL=22)
  Wire.begin();
  
  // Initialize LCD
  lcd.init();
  
  // Turn on the backlight
  lcd.backlight();
  
  // Clear the display
  lcd.clear();
  
  // Display initial message
  lcd.setCursor(0, 0);  // Column 0, Row 0
  lcd.print("Hello ESP32!");
  
  lcd.setCursor(0, 1);  // Column 0, Row 1
  lcd.print("LCD Test OK!");
  
  Serial.println("LCD initialized successfully!");
}

void loop() {
  // Update the display every 2 seconds with a counter
  static unsigned long lastUpdate = 0;
  static int counter = 0;
  
  if (millis() - lastUpdate >= 2000) {
    lastUpdate = millis();

    lcd.setContrast(50); // Optional: adjust contrast if needed
    
    // Clear second line and display counter
    lcd.setCursor(0, 1);
    lcd.print("Count: ");
    lcd.print(counter);
    lcd.print("    ");  // Clear any remaining characters
    
    counter++;
    
    Serial.print("Counter: ");
    Serial.println(counter);
  }
}