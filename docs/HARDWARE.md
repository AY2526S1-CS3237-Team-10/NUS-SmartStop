# ESP32 Hardware Configuration

## ESP32 DOIT DevKit V1 Pinout Reference

```
                     ESP32 DOIT DevKit V1
                    ┌──────────────────┐
                    │                  │
         EN  ┌──────┤ EN            D23├──────┐
      GPIO36 ├──────┤ VP            D22├──────┤ GPIO22
      GPIO39 ├──────┤ VN            TX0├──────┤ GPIO1
      GPIO34 ├──────┤ D34           RX0├──────┤ GPIO3
      GPIO35 ├──────┤ D35           D21├──────┤ GPIO21
      GPIO32 ├──────┤ D32           D19├──────┤ GPIO19
      GPIO33 ├──────┤ D33           D18├──────┤ GPIO18
      GPIO25 ├──────┤ D25            D5├──────┤ GPIO5
      GPIO26 ├──────┤ D26           D17├──────┤ GPIO17
      GPIO27 ├──────┤ D27           D16├──────┤ GPIO16
      GPIO14 ├──────┤ D14            D4├──────┤ GPIO4
      GPIO12 ├──────┤ D12            D0├──────┤ GPIO0
            GND ────┤ GND            D2├──────┤ GPIO2
      GPIO13 ├──────┤ D13           D15├──────┤ GPIO15
         9V  ├──────┤ 9V           CMD├──────┤
         EN  ├──────┤ CMD           3V3├──────┤ 3.3V
         3V3 ├──────┤ 3V3           GND├──────┤ GND
                    └──────────────────┘
```

## Default Pin Assignments for Smart Bus Stop

### Ultrasonic Sensor (HC-SR04)
```
HC-SR04         ESP32 Pin
VCC      ──────> 5V (or 3.3V)
GND      ──────> GND
TRIG     ──────> GPIO 12 (D12)
ECHO     ──────> GPIO 14 (D14)
```

### Speaker/Buzzer
```
Component       ESP32 Pin
Positive ──────> GPIO 13 (D13) [with 220Ω resistor]
Negative ──────> GND
```

### Camera Module Pins (ESP32-CAM Standard)
```
Camera Pin      ESP32 Pin       Note
PWDN     ──────> GPIO 32       Power down
RESET    ──────> -1            Not used
XCLK     ──────> GPIO 0        Clock
SIOD     ──────> GPIO 26       I2C Data
SIOC     ──────> GPIO 27       I2C Clock
Y9       ──────> GPIO 35       Data bit 9
Y8       ──────> GPIO 34       Data bit 8
Y7       ──────> GPIO 39       Data bit 7
Y6       ──────> GPIO 36       Data bit 6
Y5       ──────> GPIO 21       Data bit 5
Y4       ──────> GPIO 19       Data bit 4
Y3       ──────> GPIO 18       Data bit 3
Y2       ──────> GPIO 5        Data bit 2
VSYNC    ──────> GPIO 25       Vertical sync
HREF     ──────> GPIO 23       Horizontal reference
PCLK     ──────> GPIO 22       Pixel clock
```

## Additional Sensors (Optional Extensions)

### DHT22 Temperature/Humidity Sensor
```
DHT22           ESP32 Pin
VCC      ──────> 3.3V
GND      ──────> GND
DATA     ──────> GPIO 15 (D15)
```

### PIR Motion Sensor
```
PIR Sensor      ESP32 Pin
VCC      ──────> 5V
GND      ──────> GND
OUT      ──────> GPIO 17 (D17)
```

### LED Status Indicators
```
Component       ESP32 Pin
Red LED  ──────> GPIO 2 (D2) + 220Ω resistor
Green LED ─────> GPIO 4 (D4) + 220Ω resistor
Blue LED ──────> GPIO 16 (D16) + 220Ω resistor
```

## Power Considerations

### Power Supply Options
1. **USB Power**: 5V via micro-USB (500mA minimum) - Note: ESP32 DOIT DevKit V1 uses micro-USB; newer variants may use USB-C
2. **External Power**: 5V via VIN pin (1A recommended for camera)
3. **Battery**: 3.7V LiPo with voltage regulator

### Power Consumption (Approximate)
- ESP32 (WiFi active): 160-260mA
- Camera module: 100-200mA
- Ultrasonic sensor: 15mA (during measurement)
- Speaker/buzzer: 20-100mA (when active)

**Total recommended**: 1A power supply

## Important Notes

### GPIO Restrictions

**Input Only Pins** (cannot be used as OUTPUT):
- GPIO 34, 35, 36, 39

**Strapping Pins** (have special boot modes):
- GPIO 0: Boot mode selection (avoid external loads)
- GPIO 2: Boot mode selection
- GPIO 12: Flash voltage selection
- GPIO 15: Boot mode selection

**Built-in Features**:
- GPIO 2: Built-in LED (ESP32 DevKit)
- GPIO 1, 3: Used for Serial/USB (avoid for other purposes)

### Safe Pins for General Use
Recommended for sensors and peripherals:
- GPIO 4, 5, 13, 14, 16, 17, 18, 19, 21, 22, 23, 25, 26, 27, 32, 33

### Camera Pin Conflicts
When using camera, these pins are occupied:
- GPIO 0, 5, 18, 19, 21, 22, 23, 25, 26, 27, 32, 34, 35, 36, 39

## Wiring Tips

1. **Use appropriate wire gauges**: 22-24 AWG for sensors
2. **Keep wires short**: Especially for I2C and clock signals
3. **Use pull-up resistors**: For I2C (4.7kΩ typical)
4. **Add decoupling capacitors**: 100nF near power pins
5. **Protect GPIOs**: Use resistors for LEDs and sensitive components
6. **Common ground**: Ensure all components share common GND

## Testing Individual Components

### Test Ultrasonic Sensor
```cpp
// Simple test code
void loop() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);
  
  long duration = pulseIn(ECHO_PIN, HIGH);
  float distance = duration * 0.034 / 2;
  
  Serial.print("Distance: ");
  Serial.print(distance);
  Serial.println(" cm");
  
  delay(1000);
}
```

### Test Speaker
```cpp
// Simple beep test
void loop() {
  tone(SPEAKER_PIN, 1000); // 1kHz tone
  delay(500);
  noTone(SPEAKER_PIN);
  delay(500);
}
```

### Test Camera
```cpp
// Camera initialization test
void setup() {
  camera_config_t config;
  // ... (configuration)
  
  esp_err_t err = esp_camera_init(&config);
  if (err == ESP_OK) {
    Serial.println("Camera initialized successfully");
  } else {
    Serial.printf("Camera init failed: 0x%x\n", err);
  }
}
```

## Schematic Diagram

For detailed schematic diagrams:
1. Refer to ESP32 DevKit V1 official documentation
2. See camera module documentation (OV2640/OV5640)
3. Check sensor datasheets for specific requirements

## Safety Considerations

⚠️ **Important Safety Notes**:
- Never exceed 3.3V on GPIO pins
- Use level shifters for 5V sensors if needed
- Don't draw more than 40mA from any GPIO
- Total GPIO current limit: ~200mA
- Use external power for high-current devices
- Add flyback diodes for inductive loads (relays, motors)

## Troubleshooting Hardware Issues

**Camera not detected:**
- Check all pin connections
- Verify camera module power (needs stable 3.3V)
- Try reducing clock frequency

**Ultrasonic sensor unreliable:**
- Check 5V power supply
- Keep sensor away from metal surfaces
- Ensure proper timing in code

**ESP32 won't boot:**
- Check GPIO 0, 2, 12, 15 connections
- Remove all peripherals and test
- Verify power supply is adequate

**Random resets:**
- Increase power supply capacity
- Add bulk capacitor (100μF-470μF) near ESP32
- Check for brownout conditions

## Resources

- [ESP32 Datasheet](https://www.espressif.com/sites/default/files/documentation/esp32_datasheet_en.pdf)
- [ESP32 DevKit V1 Pinout](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/hw-reference/esp32/get-started-devkitc.html)
- [Camera Module Documentation](https://github.com/espressif/esp32-camera)
