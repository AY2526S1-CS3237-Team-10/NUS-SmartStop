# Quick Start: Integrated Sensors vs Individual Sketches

## Which Option Should I Use?

### Use `integrated_sensors.ino` if:
✅ You want ALL sensors on one ESP32  
✅ You need speaker + microphone + IR + ultrasonic together  
✅ You want a complete smart bus stop system  
✅ You have enough GPIO pins available  
✅ You want unified WiFi/MQTT management  

### Use individual sketches if:
✅ You're testing one sensor at a time  
✅ You have multiple ESP32 boards (one per sensor type)  
✅ You want to debug a specific sensor  
✅ You need different WiFi settings per sensor  
✅ You want simpler, focused code  

## Comparison Table

| Feature | Integrated Sensors | Individual Sketches |
|---------|-------------------|---------------------|
| **Boards Needed** | 1 ESP32 | 4 separate ESP32s |
| **Wiring Complexity** | Complex (many connections) | Simple (few per board) |
| **Code Complexity** | Medium (modular design) | Simple (focused) |
| **Power Usage** | Higher (all sensors active) | Lower per board |
| **Cost** | Lower (1 board) | Higher (4 boards) |
| **Debugging** | More complex | Easier |
| **Scalability** | Limited by pins | Unlimited |
| **MQTT Topics** | All from one device | Separate devices |

## File Sizes

```
integrated_sensors.ino:         ~627 lines (19 KB)
cs3237speaker.ino:              ~69 lines (1.6 KB)
cs3237_mic.ino:                 ~239 lines (6.8 KB)
esp32_ir_people_counter.ino:    ~122 lines (2.9 KB)
ultrasonic_sensors.ino:         ~146 lines (4.1 KB)
----------------------------------------
Total individual:               ~576 lines (15.4 KB)
```

## Memory Usage Comparison

### Integrated Sensors
```
Sketch uses ~500-600 KB Flash
RAM usage: ~40-50 KB (FFT buffers + audio)
PSRAM recommended for audio
```

### Individual Sketches (each)
```
Speaker:      ~200-250 KB Flash, ~20 KB RAM
Microphone:   ~250-300 KB Flash, ~30 KB RAM  
IR Sensors:   ~150-200 KB Flash, ~5 KB RAM
Ultrasonic:   ~150-200 KB Flash, ~5 KB RAM
```

## Setup Time Comparison

| Task | Integrated | Individual Total |
|------|-----------|-----------------|
| **Hardware Wiring** | 2-3 hours | 1 hour × 4 = 4 hours |
| **Software Config** | 30 minutes | 15 min × 4 = 1 hour |
| **Testing** | 1 hour | 30 min × 4 = 2 hours |
| **Debugging** | 1-2 hours | 30 min × 4 = 2 hours |
| **Total** | **4-6.5 hours** | **9 hours** |

## Cost Comparison

### Integrated System
- 1× ESP32 DevKit: $8-12
- 1× MAX98357A Speaker: $5-8
- 1× INMP441 Mic: $3-5
- 2× IR Sensors: $2-4
- 3× HC-SR04: $6-9
- 1× SD Card Module: $2-3
- 1× microSD Card: $3-5
- **Total: $29-46**

### Individual Systems (4 ESP32s)
- 4× ESP32 DevKit: $32-48
- Same sensors as above: $21-34
- **Total: $53-82**

**Savings with integrated: $24-36 (45%)**

## Feature Matrix

| Feature | Integrated | Speaker | Mic | IR | Ultrasonic |
|---------|-----------|---------|-----|-----|-----------|
| MP3 Playback | ✅ | ✅ | ❌ | ❌ | ❌ |
| Voice Detection | ✅ | ❌ | ✅ | ❌ | ❌ |
| FFT Analysis | ✅ | ❌ | ✅ | ❌ | ❌ |
| People Counting | ✅ | ❌ | ❌ | ✅ | ❌ |
| Occupancy Detection | ✅ | ❌ | ❌ | ❌ | ✅ |
| WiFi | ✅ | ❌ | ✅ | ✅ | ✅ |
| MQTT | ✅ | ❌ | ✅ | ✅ | ✅ |
| SD Card | ✅ | ✅ | ❌ | ❌ | ❌ |

## Decision Tree

```
Do you have only ONE ESP32?
│
├─ YES ──> Use integrated_sensors.ino
│          (no other choice)
│
└─ NO ──> Do you need ALL sensors working together?
           │
           ├─ YES ──> Use integrated_sensors.ino
           │          (better coordination)
           │
           └─ NO ──> Are you testing/debugging?
                      │
                      ├─ YES ──> Use individual sketches
                      │          (easier to debug)
                      │
                      └─ NO ──> Use individual sketches
                                 (more scalable, distributed)
```

## Recommended Workflow

### For Development/Testing
1. Start with individual sketches
2. Test each sensor separately
3. Once working, migrate to integrated

### For Production
- Use integrated if cost/space is priority
- Use individual if reliability/scalability is priority

## Common Questions

### Q: Can I run integrated sketch on multiple ESP32s?
**A:** Yes, but change the `DEVICE_ID` for each board so they're unique on MQTT.

### Q: What if I don't need all sensors?
**A:** You can comment out the setup/loop calls for sensors you don't need. The code is modular.

### Q: Can I add more sensors to integrated sketch?
**A:** Yes, but check available GPIO pins first. See `pin_config.h` for available pins.

### Q: Which is more reliable?
**A:** Individual sketches are more reliable (one failure doesn't affect others). Integrated is more complex but more cost-effective.

### Q: Can I use different ESP32 boards?
**A:** Yes, but pin numbers might differ. Check your board's pinout diagram.

## Getting Started

### Integrated Sensors
```bash
# 1. Read documentation
cat esp32/INTEGRATED_SENSORS_README.md
cat esp32/PIN_MIGRATION_GUIDE.md

# 2. Wire hardware following pin_config.h

# 3. Configure WiFi in integrated_sensors.ino

# 4. Upload and test
```

### Individual Sketches
```bash
# 1. Start with one sensor (e.g., speaker)
# Open esp32/cs3237speaker.ino

# 2. Configure WiFi (if applicable)

# 3. Upload and test

# 4. Repeat for other sensors

# 5. Deploy each to separate ESP32 boards
```

## Summary

**Choose Integrated If:**
- Budget constraint (1 board cheaper than 4)
- Space constraint (compact deployment)
- Single location deployment
- Learning/educational purposes

**Choose Individual If:**
- Reliability is critical
- Need to scale to many locations
- Easier maintenance/debugging needed
- Different sensors in different locations

## Need Help?

See the detailed documentation:
- `esp32/INTEGRATED_SENSORS_README.md` - Full guide
- `esp32/PIN_MIGRATION_GUIDE.md` - Pin changes
- `esp32/pin_config.h` - Quick pin reference
- Main `README.md` - Project overview
