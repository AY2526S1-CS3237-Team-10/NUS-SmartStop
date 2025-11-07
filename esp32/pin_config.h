/*
 * Pin Configuration Summary for ESP32 Integrated Sensors
 * 
 * This header file provides a quick reference for all GPIO pin assignments
 * Use this when wiring your hardware to ensure correct connections
 */

#ifndef PIN_CONFIG_H
#define PIN_CONFIG_H

// ==================== Speaker & Audio System (MAX98357A + SD Card) ====================
// I2S Audio Output (MAX98357A)
#define SPEAKER_I2S_BCLK    26    // I2S Bit Clock
#define SPEAKER_I2S_LRCK    25    // I2S Left/Right Clock
#define SPEAKER_I2S_DATA    22    // I2S Data

// SD Card (SPI - HSPI)
#define SD_CS               5     // SD Card Chip Select
#define SD_SCK              18    // SPI Clock
#define SD_MISO             19    // SPI Master In Slave Out
#define SD_MOSI             23    // SPI Master Out Slave In

// ==================== Microphone System (INMP441) ====================
// I2S Audio Input (INMP441)
#define MIC_I2S_WS          15    // I2S Word Select
#define MIC_I2S_SD          32    // I2S Serial Data
#define MIC_I2S_SCK         14    // I2S Serial Clock

// ==================== IR Sensors (People Counting) ====================
#define IR_TX_A             33    // IR Transmitter A (Entry side) - CHANGED from 26
#define IR_TX_B             27    // IR Transmitter B (Exit side)
#define IR_RX_A             34    // IR Receiver A (Input only pin)
#define IR_RX_B             35    // IR Receiver B (Input only pin)

// ==================== Ultrasonic Sensors (HC-SR04) ====================
// LEFT Section
#define ULTRASONIC_TRIG_0   2     // CHANGED from 5
#define ULTRASONIC_ECHO_0   4     // CHANGED from 18

// CENTER Section
#define ULTRASONIC_TRIG_1   16    // CHANGED from 22
#define ULTRASONIC_ECHO_1   17    // CHANGED from 23

// RIGHT Section
#define ULTRASONIC_TRIG_2   12    // CHANGED from 13
#define ULTRASONIC_ECHO_2   13    // CHANGED from 14

// ==================== Available GPIO Pins ====================
// The following GPIO pins are still available for future use:
// GPIO 0  - Boot button (use with caution)
// GPIO 1  - UART TX (Serial)
// GPIO 3  - UART RX (Serial)
// GPIO 21 - Available
// GPIO 36 - Input only (ADC)
// GPIO 39 - Input only (ADC)

// ==================== Reserved/Unavailable Pins ====================
// GPIO 6-11  - Connected to integrated SPI flash (DO NOT USE)
// GPIO 34-39 - Input only (no internal pull-up/pull-down)

// ==================== Pin Conflict Resolution Notes ====================
/*
 * Original pin conflicts that were resolved:
 * 
 * 1. IR_TX_A: 26 -> 33
 *    Reason: GPIO 26 was needed for Speaker I2S_BCLK
 * 
 * 2. Ultrasonic TRIG_0: 5 -> 2
 *    Reason: GPIO 5 was needed for SD Card CS
 * 
 * 3. Ultrasonic ECHO_0: 18 -> 4
 *    Reason: GPIO 18 was needed for SD Card SCK
 * 
 * 4. Ultrasonic TRIG_1: 22 -> 16
 *    Reason: GPIO 22 was needed for Speaker I2S_DATA
 * 
 * 5. Ultrasonic ECHO_1: 23 -> 17
 *    Reason: GPIO 23 was needed for SD Card MOSI
 * 
 * 6. Ultrasonic TRIG_2: 13 -> 12
 *    Reason: Minor reorganization for clarity
 * 
 * 7. Ultrasonic ECHO_2: 14 -> 13
 *    Reason: GPIO 14 was needed for Microphone I2S_SCK
 */

// ==================== Power Requirements ====================
/*
 * 3.3V Components:
 * - INMP441 Microphone
 * - SD Card Module (some modules support both 3.3V and 5V)
 * - MAX98357A (logic level, but can be powered by 5V)
 * 
 * 5V Components:
 * - HC-SR04 Ultrasonic Sensors
 * - IR Sensors (check your specific module)
 * - MAX98357A amplifier power (for better audio output)
 * 
 * Note: ESP32 GPIO pins are 3.3V tolerant. Some 5V sensors may need
 * level shifters, but HC-SR04 typically works with ESP32's 3.3V logic.
 */

// ==================== I2S Configuration Notes ====================
/*
 * The ESP32 has 2 I2S peripherals (I2S0 and I2S1)
 * 
 * This configuration uses:
 * - I2S0 (I2S_NUM_0): Microphone input
 * - I2S1 (via AudioOutputI2S): Speaker output
 * 
 * The ESP32-audioI2S library automatically manages I2S1 for audio output.
 */

// ==================== Wiring Checklist ====================
/*
 * Before powering on, verify:
 * 
 * [ ] Speaker MAX98357A connected to GPIOs 26, 25, 22
 * [ ] SD Card connected to GPIOs 5, 18, 19, 23
 * [ ] Microphone INMP441 connected to GPIOs 15, 32, 14
 * [ ] IR TX_A connected to GPIO 33 (not 26!)
 * [ ] IR TX_B connected to GPIO 27
 * [ ] IR RX_A connected to GPIO 34
 * [ ] IR RX_B connected to GPIO 35
 * [ ] Ultrasonic LEFT: TRIG=2, ECHO=4
 * [ ] Ultrasonic CENTER: TRIG=16, ECHO=17
 * [ ] Ultrasonic RIGHT: TRIG=12, ECHO=13
 * [ ] All GND connections made
 * [ ] 3.3V and 5V power supplies connected
 * [ ] USB cable connected for programming and serial monitoring
 */

#endif // PIN_CONFIG_H
