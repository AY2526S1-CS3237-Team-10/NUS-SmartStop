#include <Arduino.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// LCD: I2C address 0x27, 16x2 display
LiquidCrystal_I2C lcd(0x27, 16, 2);

// Example data placeholders (replace with real sensor data later)
int peopleCount = 0;
float distance = 0.0;
bool voiceDetected = false;

unsigned long lastUpdate = 0;
int updateInterval = 2000; // ms

void setup() {
  Serial.begin(115200);
  Wire.begin();
  lcd.init();
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("SmartStop Init");
  delay(1000);
  lcd.clear();
}

void loop() {
  // Simulate data updates (replace these with real readings)
  peopleCount = random(0, 10);    // mock people count
  distance = random(20, 200) / 10.0; // mock distance (cm)
  voiceDetected = random(0, 2);   // mock voice detection

  // Update every 2 seconds
  if (millis() - lastUpdate >= updateInterval) {
    lastUpdate = millis();

    lcd.clear();
    lcd.setCursor(0, 0);

    // First line: show crowd info
    lcd.print("Ppl:");
    lcd.print(peopleCount);
    lcd.print("  Dist:");
    lcd.print(distance, 1); // 1 decimal
    lcd.print("m");

    // Second line: show noise / crowd level
    lcd.setCursor(0, 1);
    if (voiceDetected) {
      lcd.print("Noise: YES ");
    } else {
      lcd.print("Noise: NO  ");
    }

    // Simple crowd level estimate
    if (peopleCount <= 3) {
      lcd.print(" L"); // Low
    } else if (peopleCount <= 6) {
      lcd.print(" M"); // Medium
    } else {
      lcd.print(" H"); // High
    }

    // Debug serial output
    Serial.print("People: ");
    Serial.print(peopleCount);
    Serial.print(" | Dist: ");
    Serial.print(distance);
    Serial.print("m | Voice: ");
    Serial.println(voiceDetected ? "YES" : "NO");
  }
}
