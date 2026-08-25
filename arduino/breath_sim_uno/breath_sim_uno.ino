/* ==========================================================================
   breath_flute.ino  —  Chamki Firefly Forest, breath sensor

   WIRING — four wires. Exactly the four you already have.
       HX710B VCC  ->  Arduino 5V
       HX710B GND  ->  Arduino GND
       HX710B OUT  ->  Arduino digital 2
       HX710B SCK  ->  Arduino digital 3

   Using the digital pins for the sensor keeps all six analog pins free,
   which is what lets the flute have six finger holes later instead of
   four. Nothing else needs to move, ever.

   IF OUT AND SCK ARE THE OTHER WAY ROUND: change SWAP_PINS to 1 below
   and re-upload. Do NOT move the wires — it is the same circuit either
   way, and swapping in code is quicker and cannot be got wrong.

   WHAT THIS SENDS
       One line, about 40 times a second, at 115200 baud:

           0.734,523.3
           ^     ^
           |     the note being fingered, in Hz (0 if not fitted)
           how hard the air is, 0.000 .. 1.000

       Lines beginning with '#', 'READY' or 'ERR' are diagnostics.
       The game ignores them, so they are safe to leave switched on.

   WHAT IT LISTENS FOR
       'z'  ->  re-zero. Take a fresh "nobody is blowing" reading.
       'd'  ->  toggle raw debug output.

   ------------------------------------------------------------------------
   IMPORTANT — HOW THIS DIFFERS FROM THE OLD SKETCH

   The old sketch needed FULL_SCALE hand-tuned to each child, because the
   game treated the number as final. It no longer does: the game measures
   the player's own normal breath at the start of every session and scales
   everything to it.

   So FULL_SCALE is no longer a per-person setting. It has ONE job now —
   keep the number inside 0..1 without ever hitting the ceiling. Set it so
   that the hardest blow anyone will manage reads about 0.8, and then leave
   it alone forever. Headroom is the point: if a hard blow pins at 1.000,
   everything above "normal" is flattened into the same value and the game
   can no longer tell a firm breath from a huge one.
   ========================================================================== */

/* Set to 1 if the numbers come out frozen or as nonsense — that means
   OUT and SCK are the other way round. It is the single most common
   snag in this build, and it damages nothing. */
#define SWAP_PINS 0

#if SWAP_PINS
  const uint8_t PIN_DOUT = 3;
  const uint8_t PIN_SCK  = 2;
#else
  const uint8_t PIN_DOUT = 2;
  const uint8_t PIN_SCK  = 3;
#endif

/* Raw counts that should read as 1.000. Bigger number = gentler curve.
   Check it with DEBUG_RAW (or send 'd'): blow as hard as you can and aim
   for roughly 0.8 on screen. Anything in the right ballpark is fine —
   the game's own calibration does the rest. */
long FULL_SCALE = 500000L;

bool DEBUG_RAW = false;         // send 'd' to flip this at runtime

/* ---- Finger holes (optional) --------------------------------------------
   Only needed if you fitted the copper tape pads. The note is a READOUT
   in this game, not something you score on, so leaving this off costs you
   nothing. Set to 1 only after installing the "ADCTouch" library.        */
#define USE_TOUCH 0

#if USE_TOUCH
  #include <ADCTouch.h>
  /* The sensor is on the digital pins, so every analog pin is free:
     six finger holes, C down to D. */
  const uint8_t TOUCH_PINS[] = { A0, A1, A2, A3, A4, A5 };
  const uint8_t TOUCH_COUNT  = 6;
  int  touchRef[TOUCH_COUNT];
  int  TOUCH_DELTA = 40;        // raise to 60 if pads read as always-touched
#endif

/* German fingering: each lower note simply adds one more finger.
   Index = how many holes are covered. */
const float NOTE_HZ[] = { 1046.50, 987.77, 880.00, 783.99, 698.46, 659.25, 587.33 };

long zeroOffset = 0;
unsigned long lastSend = 0;
const unsigned long SEND_EVERY_MS = 20;   // ~50 lines a second, capped by the ADC

/* -------------------------------------------------------------------------
   Talking to the HX710B. Data comes back on one wire, timing on the other:
   the Arduino pulses SCK twenty-four times and the sensor puts out one bit
   of its reading on each pulse.
   ------------------------------------------------------------------------- */
bool sensorReady() { return digitalRead(PIN_DOUT) == LOW; }

long readSensor() {
  long v = 0;
  for (uint8_t i = 0; i < 24; i++) {
    digitalWrite(PIN_SCK, HIGH);
    delayMicroseconds(1);
    v = (v << 1) | digitalRead(PIN_DOUT);
    digitalWrite(PIN_SCK, LOW);
    delayMicroseconds(1);
  }
  /* Two extra pulses (26 total) selects the differential input at 40
     samples a second, which is the fastest this chip offers. */
  for (uint8_t i = 0; i < 2; i++) {
    digitalWrite(PIN_SCK, HIGH);
    delayMicroseconds(1);
    digitalWrite(PIN_SCK, LOW);
    delayMicroseconds(1);
  }
  if (v & 0x800000L) v |= ~0xFFFFFFL;    // 24-bit two's complement -> signed
  return v;
}

/* Average a stack of readings taken while nobody is blowing. This is the
   "still air" mark that every later reading is measured against. */
void reZero() {
  const uint8_t N = 24;
  long sum = 0;
  uint8_t got = 0;
  unsigned long guard = millis();
  while (got < N && millis() - guard < 3000) {
    if (sensorReady()) { sum += readSensor(); got++; }
  }
  if (got == 0) { Serial.println("ERR no sensor - check OUT and SCK"); return; }
  zeroOffset = sum / got;
  Serial.println("READY");
}

/* A healthy sensor gives readings that wander a little, even in still
   air. Readings that are perfectly identical, or jammed at either end of
   the range, mean the two data wires are the wrong way round — so say so
   in plain words rather than letting the game show a dead bar. */
void checkWiring() {
  long first = 0;
  bool got = false, moved = false, sane = false;
  unsigned long guard = millis();
  uint8_t n = 0;
  while (n < 20 && millis() - guard < 2000) {
    if (!sensorReady()) continue;
    long v = readSensor();
    if (!got) { first = v; got = true; }
    if (v != first) moved = true;
    if (v != 0 && v != -1 && v != 0x7FFFFFL && v != -0x800000L) sane = true;
    n++;
  }
  if (!got)            Serial.println("ERR nothing on OUT - check the pin 2 and pin 3 wires");
  else if (!sane)      Serial.println("ERR readings are stuck - set SWAP_PINS to 1 and re-upload");
  else if (!moved)     Serial.println("ERR readings never change - set SWAP_PINS to 1 and re-upload");
}

void setup() {
  Serial.begin(115200);
  pinMode(PIN_SCK, OUTPUT);
  pinMode(PIN_DOUT, INPUT);
  digitalWrite(PIN_SCK, LOW);
  delay(400);                            // let the sensor settle after power-up
  reZero();
  checkWiring();

#if USE_TOUCH
  /* Reference reading for each pad, taken with hands off the flute. */
  for (uint8_t i = 0; i < TOUCH_COUNT; i++) touchRef[i] = ADCTouch.read(TOUCH_PINS[i], 500);
#endif
}

/* How many holes are covered right now -> which note that is. */
float currentNoteHz() {
#if USE_TOUCH
  uint8_t holes = 0;
  for (uint8_t i = 0; i < TOUCH_COUNT; i++) {
    if (ADCTouch.read(TOUCH_PINS[i], 20) - touchRef[i] > TOUCH_DELTA) holes++;
  }
  if (holes > TOUCH_COUNT) holes = TOUCH_COUNT;
  return NOTE_HZ[holes];
#else
  return 0.0;                            // no pads fitted; the game copes
#endif
}

void loop() {
  /* Commands from the game or the Serial Monitor. */
  while (Serial.available()) {
    char c = Serial.read();
    if (c == 'z' || c == 'Z') reZero();
    else if (c == 'd' || c == 'D') DEBUG_RAW = !DEBUG_RAW;
  }

  if (!sensorReady()) return;            // never block; the chip sets the pace
  long raw = readSensor();
  long delta = raw - zeroOffset;
  if (delta < 0) delta = 0;              // below "still air" just means still air

  if (millis() - lastSend < SEND_EVERY_MS) return;
  lastSend = millis();

  if (DEBUG_RAW) {
    /* '#' keeps this invisible to the game while you watch it in the
       Serial Monitor. Blow your hardest and note the biggest number. */
    Serial.print('#');
    Serial.println(delta);
    return;
  }

  float p = (float)delta / (float)FULL_SCALE;
  if (p > 1.0) p = 1.0;

  Serial.print(p, 3);
  Serial.print(',');
  Serial.println(currentNoteHz(), 1);
}