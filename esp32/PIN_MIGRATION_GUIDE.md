# Pin Migration Guide

This document shows the pin changes made when integrating all sensors into a single file.

## Pin Comparison Table

| Module | Signal | Original Pin | New Pin | Status | Reason for Change |
|--------|--------|--------------|---------|--------|-------------------|
| **Speaker** | I2S_BCLK | GPIO 26 | GPIO 26 | ✓ Unchanged | Base configuration |
| **Speaker** | I2S_LRCK | GPIO 25 | GPIO 25 | ✓ Unchanged | Base configuration |
| **Speaker** | I2S_DATA | GPIO 22 | GPIO 22 | ✓ Unchanged | Base configuration |
| **Speaker** | SD_CS | GPIO 5 | GPIO 5 | ✓ Unchanged | Base configuration |
| **Speaker** | SD_SCK | GPIO 18 | GPIO 18 | ✓ Unchanged | Base configuration |
| **Speaker** | SD_MISO | GPIO 19 | GPIO 19 | ✓ Unchanged | Base configuration |
| **Speaker** | SD_MOSI | GPIO 23 | GPIO 23 | ✓ Unchanged | Base configuration |
| **Microphone** | I2S_WS | GPIO 15 | GPIO 15 | ✓ Unchanged | No conflict |
| **Microphone** | I2S_SD | GPIO 32 | GPIO 32 | ✓ Unchanged | No conflict |
| **Microphone** | I2S_SCK | GPIO 14 | GPIO 14 | ✓ Unchanged | No conflict |
| **IR Sensor** | IR_TX_A | GPIO 26 | **GPIO 33** | ⚠ **CHANGED** | Conflicted with Speaker I2S_BCLK |
| **IR Sensor** | IR_TX_B | GPIO 27 | GPIO 27 | ✓ Unchanged | No conflict |
| **IR Sensor** | IR_RX_A | GPIO 34 | GPIO 34 | ✓ Unchanged | Input-only pin |
| **IR Sensor** | IR_RX_B | GPIO 35 | GPIO 35 | ✓ Unchanged | Input-only pin |
| **Ultrasonic** | TRIG_LEFT | GPIO 5 | **GPIO 2** | ⚠ **CHANGED** | Conflicted with SD_CS |
| **Ultrasonic** | ECHO_LEFT | GPIO 18 | **GPIO 4** | ⚠ **CHANGED** | Conflicted with SD_SCK |
| **Ultrasonic** | TRIG_CENTER | GPIO 22 | **GPIO 16** | ⚠ **CHANGED** | Conflicted with Speaker I2S_DATA |
| **Ultrasonic** | ECHO_CENTER | GPIO 23 | **GPIO 17** | ⚠ **CHANGED** | Conflicted with SD_MOSI |
| **Ultrasonic** | TRIG_RIGHT | GPIO 13 | **GPIO 12** | ⚠ **CHANGED** | Better organization |
| **Ultrasonic** | ECHO_RIGHT | GPIO 14 | **GPIO 13** | ⚠ **CHANGED** | Conflicted with Mic I2S_SCK |

## Visual Pin Map

```
ESP32 GPIO Pin Allocation (Integrated Sensors)
================================================

INPUT ONLY PINS (34-39):
├─ GPIO 34 ──> IR_RX_A
├─ GPIO 35 ──> IR_RX_B
├─ GPIO 36 ──> [Available]
└─ GPIO 39 ──> [Available]

I2S AUDIO (Speaker - MAX98357A):
├─ GPIO 26 ──> I2S_BCLK (Bit Clock)
├─ GPIO 25 ──> I2S_LRCK (L/R Clock)
└─ GPIO 22 ──> I2S_DATA (Audio Data)

SPI (SD Card):
├─ GPIO 5  ──> SD_CS (Chip Select)
├─ GPIO 18 ──> SD_SCK (Clock)
├─ GPIO 19 ──> SD_MISO (Data In)
└─ GPIO 23 ──> SD_MOSI (Data Out)

I2S AUDIO (Microphone - INMP441):
├─ GPIO 15 ──> I2S_WS (Word Select)
├─ GPIO 32 ──> I2S_SD (Serial Data)
└─ GPIO 14 ──> I2S_SCK (Serial Clock)

IR SENSORS:
├─ GPIO 33 ──> IR_TX_A (Transmitter A) ⚠ CHANGED!
├─ GPIO 27 ──> IR_TX_B (Transmitter B)
├─ GPIO 34 ──> IR_RX_A (Receiver A)
└─ GPIO 35 ──> IR_RX_B (Receiver B)

ULTRASONIC SENSORS:
LEFT Section:
├─ GPIO 2  ──> TRIG_LEFT ⚠ CHANGED!
└─ GPIO 4  ──> ECHO_LEFT ⚠ CHANGED!

CENTER Section:
├─ GPIO 16 ──> TRIG_CENTER ⚠ CHANGED!
└─ GPIO 17 ──> ECHO_CENTER ⚠ CHANGED!

RIGHT Section:
├─ GPIO 12 ──> TRIG_RIGHT ⚠ CHANGED!
└─ GPIO 13 ──> ECHO_RIGHT ⚠ CHANGED!

AVAILABLE PINS:
├─ GPIO 0  ──> Boot button (use with caution)
├─ GPIO 21 ──> General purpose
├─ GPIO 36 ──> Input only (ADC)
└─ GPIO 39 ──> Input only (ADC)

RESERVED (DO NOT USE):
└─ GPIO 6-11 ──> Flash memory
```

## Conflict Resolution Details

### Conflict 1: IR_TX_A (GPIO 26)
- **Problem**: GPIO 26 needed by both IR sensor and speaker I2S_BCLK
- **Solution**: Moved IR_TX_A to GPIO 33
- **Impact**: Need to rewire IR transmitter A
- **Priority**: Speaker I2S took priority as it's harder to change

### Conflict 2: Ultrasonic TRIG_LEFT (GPIO 5)
- **Problem**: GPIO 5 needed by both ultrasonic and SD card CS
- **Solution**: Moved ultrasonic trigger to GPIO 2
- **Impact**: Minimal - GPIO 2 is available and suitable
- **Note**: GPIO 2 has built-in LED on some boards

### Conflict 3: Ultrasonic ECHO_LEFT (GPIO 18)
- **Problem**: GPIO 18 needed by both ultrasonic and SD card SCK
- **Solution**: Moved ultrasonic echo to GPIO 4
- **Impact**: None - GPIO 4 is general purpose

### Conflict 4: Ultrasonic TRIG_CENTER (GPIO 22)
- **Problem**: GPIO 22 needed by both ultrasonic and speaker I2S_DATA
- **Solution**: Moved ultrasonic trigger to GPIO 16
- **Impact**: None - GPIO 16 is general purpose (U2_RXD)

### Conflict 5: Ultrasonic ECHO_CENTER (GPIO 23)
- **Problem**: GPIO 23 needed by both ultrasonic and SD card MOSI
- **Solution**: Moved ultrasonic echo to GPIO 17
- **Impact**: None - GPIO 17 is general purpose (U2_TXD)

### Conflict 6: Ultrasonic TRIG_RIGHT (GPIO 13)
- **Problem**: No direct conflict, but reorganized for consistency
- **Solution**: Moved from GPIO 13 to GPIO 12
- **Impact**: Minor reorganization

### Conflict 7: Ultrasonic ECHO_RIGHT (GPIO 14)
- **Problem**: GPIO 14 needed by both ultrasonic and microphone I2S_SCK
- **Solution**: Moved ultrasonic echo to GPIO 13
- **Impact**: None - GPIO 13 is general purpose

## Migration Checklist

If you're upgrading from individual sketches to the integrated version:

### Hardware Changes Required

- [ ] **IR Sensor**: Move IR_TX_A wire from GPIO 26 to GPIO 33
- [ ] **Ultrasonic LEFT**: Move TRIG from GPIO 5 to GPIO 2
- [ ] **Ultrasonic LEFT**: Move ECHO from GPIO 18 to GPIO 4
- [ ] **Ultrasonic CENTER**: Move TRIG from GPIO 22 to GPIO 16
- [ ] **Ultrasonic CENTER**: Move ECHO from GPIO 23 to GPIO 17
- [ ] **Ultrasonic RIGHT**: Move TRIG from GPIO 13 to GPIO 12
- [ ] **Ultrasonic RIGHT**: Move ECHO from GPIO 14 to GPIO 13
- [ ] Verify all other connections remain the same
- [ ] Test each sensor individually after rewiring

### Software Changes Required

- [ ] Update WiFi credentials in `integrated_sensors.ino`
- [ ] Update MQTT server address if needed
- [ ] Change `DEVICE_ID` to unique identifier
- [ ] Ensure SD card has MP3 files in `/MP3/` folder
- [ ] Upload integrated sketch to ESP32
- [ ] Subscribe to MQTT topics to verify data publishing

### Testing Procedure

1. **Power On Test**
   - Upload sketch
   - Open Serial Monitor (115200 baud)
   - Verify WiFi connection
   - Verify MQTT connection

2. **Speaker Test**
   - Publish MQTT command: `PLAY_AUDIO_1`
   - Verify audio plays from speaker
   - Test all 4 audio files

3. **Microphone Test**
   - Speak near microphone
   - Check serial output for voice detection
   - Verify MQTT publishes voice status

4. **IR Sensor Test**
   - Pass hand through IR beams in sequence
   - Verify people count increments/decrements
   - Check MQTT publishes count changes

5. **Ultrasonic Test**
   - Place objects in detection range
   - Verify distance readings on serial
   - Check MQTT publishes occupancy data

## Additional Notes

### GPIO Restrictions

- **Input-only pins** (GPIO 34-39): Can only be used for input, no internal pull-up/pull-down
- **Strapping pins** (GPIO 0, 2, 5, 12, 15): Used during boot, may need attention
- **Flash pins** (GPIO 6-11): Connected to integrated flash, must not be used
- **UART pins** (GPIO 1, 3): Used for Serial, avoid if using Serial Monitor

### Safe to Change

If you encounter issues with the new pin assignments, you can safely change:
- Ultrasonic sensor pins (TRIG_LEFT, ECHO_LEFT, etc.) to any available GPIO
- IR_TX_A to any available output pin (except input-only pins)

### Do Not Change

These pins are hardcoded by libraries or hardware interfaces:
- I2S pins (both speaker and microphone)
- SPI pins (SD card)
- Input-only pins (GPIO 34-39) - cannot be used for outputs

## Quick Reference

### To wire from scratch:
```bash
# View pin configuration
cat esp32/pin_config.h

# View detailed documentation
cat esp32/INTEGRATED_SENSORS_README.md
```

### To migrate from old code:
1. Note your current pin wiring
2. Compare with the table above
3. Change only the pins marked "CHANGED"
4. Keep unchanged pins as-is
