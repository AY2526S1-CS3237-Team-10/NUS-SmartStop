// ======== PIN DEFINITIONS ========
const int IR_TX_A = 26;   // Transmitter A control pin
const int IR_TX_B = 27;   // Transmitter B control pin
const int IR_RX_A = 34;   // Receiver A signal pin
const int IR_RX_B = 35;   // Receiver B signal pin

// ======== VARIABLES ========
volatile int peopleCount = 0;
int lastA = HIGH, lastB = HIGH;
unsigned long lastTriggerTime = 0;

// time window to detect sequence (ms)
const unsigned long sequenceWindow = 1000;

// ======== SETUP ========
void setup() {
  Serial.begin(115200);

  pinMode(IR_TX_A, OUTPUT);
  pinMode(IR_TX_B, OUTPUT);
  pinMode(IR_RX_A, INPUT);
  pinMode(IR_RX_B, INPUT);

  // turn both IR transmitters ON
  digitalWrite(IR_TX_A, HIGH);
  digitalWrite(IR_TX_B, HIGH);
}

// ======== LOOP ========
void loop() {
  int currA = digitalRead(IR_RX_A);
  int currB = digitalRead(IR_RX_B);

  unsigned long now = millis();

  // If A beam is broken (LOW), then B shortly after → Entry
  if (lastA == HIGH && currA == LOW) {
    lastTriggerTime = now;
    while (millis() - lastTriggerTime < sequenceWindow) {
      if (digitalRead(IR_RX_B) == LOW) {
        peopleCount++;
        Serial.println("Count: " + String(peopleCount));
        delay(500);  // prevent double count
        break;
      }
    }
  }

  // If B beam is broken first, then A → Exit
  if (lastB == HIGH && currB == LOW) {
    lastTriggerTime = now;
    while (millis() - lastTriggerTime < sequenceWindow) {
      if (digitalRead(IR_RX_A) == LOW) {
        peopleCount--;
        if (peopleCount < 0) peopleCount = 0;
        Serial.println(peopleCount);
        delay(500);
        break;
      }
    }
  }

  lastA = currA;
  lastB = currB;
}
