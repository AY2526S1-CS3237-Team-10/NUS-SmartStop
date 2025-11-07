# NUS SmartStop - Integrated Sensors System

## Overview

This file (`integrated_sensors.ino`) combines all sensor functionalities into a single Arduino sketch for the ESP32:

- **Speaker (MAX98357A)**: MP3 audio playback via I2S interface
- **Microphone (INMP441)**: Real-time voice activity detection with FFT analysis
- **IR Sensors**: Bidirectional people counting for entry/exit tracking
- **Ultrasonic Sensors**: Multi-zone occupancy detection (LEFT/CENTER/RIGHT)
- **WiFi & MQTT**: Real-time data publishing and command reception

## Pin Configuration

All pin assignments have been carefully selected to avoid conflicts between modules.

### Speaker (MAX98357A I2S Audio)
| Signal | GPIO Pin | Description |
|--------|----------|-------------|
| I2S_BCLK | GPIO 26 | Bit clock |
| I2S_LRCK | GPIO 25 | Left/Right clock |
| I2S_DATA | GPIO 22 | Audio data |

### SD Card (HSPI for MP3 storage)
| Signal | GPIO Pin | Description |
|--------|----------|-------------|
| SD_CS | GPIO 5 | Chip select |
| SD_SCK | GPIO 18 | Clock |
| SD_MISO | GPIO 19 | Master In Slave Out |
| SD_MOSI | GPIO 23 | Master Out Slave In |

### Microphone (INMP441 I2S Input)
| Signal | GPIO Pin | Description |
|--------|----------|-------------|
| I2S_WS | GPIO 15 | Word select |
| I2S_SD | GPIO 32 | Serial data |
| I2S_SCK | GPIO 14 | Serial clock |

### IR Sensors (People Counting)
| Signal | GPIO Pin | Description |
|--------|----------|-------------|
| IR_TX_A | GPIO 33 | Transmitter A (entry side) |
| IR_TX_B | GPIO 27 | Transmitter B (exit side) |
| IR_RX_A | GPIO 34 | Receiver A (input only) |
| IR_RX_B | GPIO 35 | Receiver B (input only) |

### Ultrasonic Sensors (HC-SR04 - Occupancy Detection)
| Sensor | TRIG Pin | ECHO Pin | Description |
|--------|----------|----------|-------------|
| LEFT | GPIO 2 | GPIO 4 | Left section |
| CENTER | GPIO 16 | GPIO 17 | Center section |
| RIGHT | GPIO 12 | GPIO 13 | Right section |

## Pin Changes from Original Sketches

The following pins were reassigned to avoid conflicts:

| Original Sketch | Original Pin | New Pin | Reason |
|----------------|--------------|---------|---------|
| IR Sensors | IR_TX_A: GPIO 26 | GPIO 33 | Conflicted with Speaker I2S_BCLK |
| Ultrasonic | TRIG_0: GPIO 5 | GPIO 2 | Conflicted with SD_CS |
| Ultrasonic | ECHO_0: GPIO 18 | GPIO 4 | Conflicted with SD_SCK |
| Ultrasonic | TRIG_1: GPIO 22 | GPIO 16 | Conflicted with Speaker I2S_DATA |
| Ultrasonic | ECHO_1: GPIO 23 | GPIO 17 | Conflicted with SD_MOSI |
| Ultrasonic | TRIG_2: GPIO 13 | GPIO 12 | Minor reorganization |
| Ultrasonic | ECHO_2: GPIO 14 | GPIO 13 | Conflicted with Mic I2S_SCK |

## Features

### 1. Speaker System
- MP3 playback from SD card
- Four predefined audio files:
  - Audio 1: `bus3min.mp3` - 3 minutes until bus
  - Audio 2: `bus1min.mp3` - 1 minute until bus
  - Audio 3: `fullcapacity.mp3` - Bus at full capacity
  - Audio 4: `busbreakdown.mp3` - Bus breakdown announcement
- I2S audio output via MAX98357A amplifier
- Controlled via MQTT commands

### 2. Microphone System
- Real-time voice activity detection
- FFT-based audio analysis (1024 samples @ 16kHz)
- Multi-feature detection:
  - Voice frequency ratio analysis
  - Voicing strength calculation (low/mid frequency ratio)
  - Spectral flatness measure (SFM)
  - RMS energy threshold
- Debounced state transitions (prevents false triggers)
- High-pass filter to remove DC offset and low-frequency noise
- Publishes voice activity state to MQTT every second

### 3. IR Sensor System
- Bidirectional people counting
- Detects entry and exit events
- Sequence detection within 1000ms window
- Prevents count from going negative
- Real-time MQTT updates on count changes

### 4. Ultrasonic Sensor System
- Three-zone occupancy detection (LEFT, CENTER, RIGHT)
- Distance measurements in centimeters
- Configurable detection range (20-400cm)
- Density calculation (percentage of occupied zones)
- 2-second reading interval
- MQTT publishing with detailed sensor data

## Required Libraries

Install these libraries via Arduino IDE Library Manager:

```
- WiFi (built-in with ESP32)
- PubSubClient (MQTT client)
- arduinoFFT (FFT for microphone)
- ESP32-audioI2S (for speaker)
  - AudioFileSourceSD
  - AudioOutputI2S
  - AudioGeneratorMP3
- ESP32MQTTClient (alternative MQTT client)
```

## Configuration

### WiFi Settings
```cpp
const char* WIFI_SSID = "YOUR_WIFI_SSID";
const char* WIFI_PASS = "YOUR_WIFI_PASSWORD";
```

### MQTT Settings
```cpp
const char* MQTT_HOST = "157.230.250.226";
const uint16_t MQTT_PORT = 1883;
const char* DEVICE_ID = "esp32-integrated-01";
```

Change the `DEVICE_ID` to uniquely identify each ESP32 device.

## MQTT Topics

### Published Topics
- `nus-smartstop/voice` - Voice activity detection
  ```json
  {"deviceId":"esp32-integrated-01","voice":true}
  ```

- `nus-smartstop/ir-sensor/data` - People count
  ```json
  {"people_count": 5.0, "location": "busstop"}
  ```

- `nus-smartstop/ultrasonic/data` - Occupancy data
  ```json
  {
    "sensors": {
      "LEFT": {"distance": 150, "occupied": 1},
      "CENTER": {"distance": 0, "occupied": 0},
      "RIGHT": {"distance": 230, "occupied": 1}
    },
    "density": 0.67
  }
  ```

### Subscribed Topics
- `nus-smartstop/command/esp32-integrated-01` - Command reception

### Supported Commands
- `PLAY_AUDIO_1` - Play bus3min.mp3
- `PLAY_AUDIO_2` - Play bus1min.mp3
- `PLAY_AUDIO_3` - Play fullcapacity.mp3
- `PLAY_AUDIO_4` - Play busbreakdown.mp3
- `RESET_COUNT` - Reset people counter to 0

## SD Card Setup

For speaker functionality, prepare a microSD card:

1. Format as FAT32
2. Create a folder named `MP3`
3. Place the following MP3 files:
   - `bus3min.mp3`
   - `bus1min.mp3`
   - `fullcapacity.mp3`
   - `busbreakdown.mp3`

## Hardware Connections

### MAX98357A Speaker Amplifier
```
MAX98357A -> ESP32
VIN       -> 5V
GND       -> GND
DIN       -> GPIO 22 (I2S_DATA)
BCLK      -> GPIO 26 (I2S_BCLK)
LRC       -> GPIO 25 (I2S_LRCK)
```

### SD Card Module (SPI)
```
SD Module -> ESP32
VCC       -> 3.3V
GND       -> GND
CS        -> GPIO 5
MOSI      -> GPIO 23
MISO      -> GPIO 19
SCK       -> GPIO 18
```

### INMP441 Microphone
```
INMP441   -> ESP32
VDD       -> 3.3V
GND       -> GND
SD        -> GPIO 32 (I2S_SD)
WS        -> GPIO 15 (I2S_WS)
SCK       -> GPIO 14 (I2S_SCK)
L/R       -> GND (for left channel)
```

### IR Sensors
```
IR Module -> ESP32
TX_A VCC  -> 3.3V
TX_A OUT  -> GPIO 33
TX_B OUT  -> GPIO 27
RX_A IN   -> GPIO 34
RX_B IN   -> GPIO 35
GND       -> GND
```

### HC-SR04 Ultrasonic Sensors
```
Sensor LEFT:
VCC  -> 5V
TRIG -> GPIO 2
ECHO -> GPIO 4
GND  -> GND

Sensor CENTER:
VCC  -> 5V
TRIG -> GPIO 16
ECHO -> GPIO 17
GND  -> GND

Sensor RIGHT:
VCC  -> 5V
TRIG -> GPIO 12
ECHO -> GPIO 13
GND  -> GND
```

## Usage

1. **Upload the sketch** to your ESP32 using Arduino IDE
2. **Open Serial Monitor** at 115200 baud to see status messages
3. **Connect to WiFi** - Device will automatically connect on boot
4. **MQTT connection** - Will automatically connect and subscribe to commands
5. **Monitor data** - All sensor data is published to respective MQTT topics

## Monitoring & Debugging

### Serial Output
The system provides detailed serial output:
- WiFi connection status
- MQTT connection status
- Microphone voice detection status
- IR sensor people counting events
- Ultrasonic sensor readings every 2 seconds
- Network health checks every 5 seconds

### LED Indicators
Consider connecting LEDs to unused GPIO pins for visual status:
- WiFi connected
- MQTT connected
- Voice detected
- Person counted

## Performance Considerations

### Memory Usage
- FFT requires significant RAM (1024 samples × 16 bytes = ~16KB)
- Audio buffers consume additional memory
- Ensure your ESP32 has sufficient free heap

### CPU Usage
- FFT calculations are CPU intensive
- Voice detection runs continuously
- Other sensors operate on intervals to balance load

### Timing
- Microphone processing: Continuous (driven by I2S DMA)
- Ultrasonic reading: Every 2 seconds
- MQTT publish: Voice every 1 second, others on events
- Network check: Every 5 seconds

## Troubleshooting

### SD Card Not Detected
- Check SPI wiring
- Verify SD card is FAT32 formatted
- Try reducing SPI speed (lower the 25000000 parameter)

### No Audio Output
- Verify speaker connections
- Check SD card has MP3 files in /MP3/ folder
- Ensure speaker is powered (5V for MAX98357A)

### Microphone Not Working
- Check I2S wiring
- Verify INMP441 is powered with 3.3V
- Ensure L/R pin is connected to GND

### IR Sensors Not Counting
- Check TX LEDs are illuminated
- Verify receivers are aligned with transmitters
- Test with `digitalRead()` to confirm signal levels

### Ultrasonic Sensors Return 0
- Check power supply (requires 5V)
- Verify trigger/echo connections
- Reduce timeout or add pull-down resistors

### WiFi Won't Connect
- Verify SSID and password
- Check 2.4GHz WiFi is enabled (ESP32 doesn't support 5GHz)
- Move closer to WiFi router

### MQTT Connection Failed
- Verify MQTT broker is running and accessible
- Check firewall settings
- Confirm MQTT_HOST and MQTT_PORT are correct

## Future Enhancements

Potential improvements:
1. Add temperature/humidity sensors (DHT22)
2. Implement OTA (Over-The-Air) firmware updates
3. Add local web server for configuration
4. Store sensor data on SD card for offline mode
5. Add light sensor for automatic night mode
6. Implement power-saving deep sleep modes
7. Add LCD/OLED display for local status

## License

Part of NUS-SmartStop project for CS3237 coursework.

## Support

For issues or questions, refer to the main project README or open an issue on GitHub.
