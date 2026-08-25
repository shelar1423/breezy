/*
 * breath_flute_hx710b.ino
 * --------------------------------------------------------------
 * Streams:  <0 to 1>,<note hz>\n   at ~40 Hz, 115200 baud
 *
 * CALIBRATION IS AUTOMATIC. You never have to read numbers as they
 * scroll past. In the Serial Monitor, type a single letter and press
 * Enter:
 *
 *      c   calibrate. It counts you in, you blow one comfortable
 *          breath, and it works out FULL_SCALE for you and saves it.
 *          Saved in EEPROM, so it survives unplugging. Do it once.
 *
 *          A comfortable breath should land around 0.45 afterwards, NOT
 *          1.000. That is deliberate - the game needs room above your
 *          normal breath to tell a firm one from a huge one. If you see
 *          1.000 while breathing normally, this is the old sketch.
 *
 *      z   re-zero. You should not need this any more - the sketch now
 *          tracks the zero continuously while you play - but it forces
 *          a fresh measurement of the zero AND of the sensor's noise.
 *
 *      p   print the current settings, and PAUSE the stream so you can
 *          read them. Type s to start it again.
 *
 *      s   pause / resume the stream by hand. It resumes on its own
 *          after 30 seconds either way, so you cannot leave it stuck.
 *
 * Wiring (module is 3.3-5V tolerant, so 5V from the Uno is fine)
 *   VCC -> 5V      OUT -> pin 2
 *   GND -> GND     SCK -> pin 3
 *   Holes 1..6 -> A0, A1, A2, A3, A4, A5   (thin wire to copper tape)
 *   Air tube -> onto the sensor's port, with a ring of glue
 *
 * Library (Library Manager): "ADCTouch"   — only for the finger pads
 * --------------------------------------------------------------
 */

#include <ADCTouch.h>
#include <EEPROM.h>

// ---------- pins ----------
const uint8_t HX_DOUT = 2;   // renamed: plain DOUT/SCK clash with the Arduino core
const uint8_t HX_SCK  = 3;   // (SCK is already taken by the SPI pins)
const uint8_t HOLE_PIN[6] = {A0, A1, A2, A3, A4, A5};

// ---------- tuning ----------
// 27 clock pulses = pressure bridge at 40 Hz.
// 25 clock pulses = pressure bridge at 10 Hz. Try 25 if 27 gives nothing.
const uint8_t PULSES = 27;

const float   ALPHA       = 0.35;   // smoothing, 0..1
const int     TOUCH_DELTA = 40;     // above reference = hole covered
const uint8_t TOUCH_EVERY = 3;      // read finger pads every Nth sample
const long    DEFAULT_FULL_SCALE = 20000;
// ----------------------------

const uint16_t EE_MAGIC = 0xB1F7;   // marks "there is a saved value here"
const int      EE_ADDR  = 0;

long  fullScale = DEFAULT_FULL_SCALE;
long  baseline  = 0;

/* THE DRIFT FIX.
   The HX710B does not sit still. Its "nobody is blowing" reading wanders
   with temperature and with time, and this sketch used to measure that
   reading exactly once, at boot. Ten minutes later the board was
   reporting a steady 0.3 with the flute on the table — the game saw a
   breath that never ended, and a real breath started already a third of
   the way to the ceiling.

   So the zero is now chased continuously, and asymmetrically:
     - a reading BELOW the zero is simply the truth about where zero is
       (nothing reads lower than no air), so follow it down briskly;
     - a reading a little ABOVE it is drift, so creep up, slowly enough
       that a gentle breath is never eaten;
     - a reading well above it is AIR. Freeze, or a long breath drags the
       floor up underneath itself and cancels itself out.

   noiseBand is where "a little above" ends. It is measured from this
   sensor's own jitter rather than guessed, so a twitchy board gets a
   wider deadband than a calm one. */
long  noiseBand = 200;

/* SELF-WIDENING SCALE.

   If FULL_SCALE is too small for how hard this child actually blows, the
   output pins at 1.000 and stays there. From the game's side that is a
   disaster: one ordinary breath then uses the whole 0..1 range, so a
   normal breath and a huge one are the same number, and every target
   above "normal" becomes physically unreachable. The bar looks frozen
   however hard you blow.

   Rather than wait for somebody to notice and re-run 'c', the sketch
   watches for air running off the top of its own scale and widens the
   scale to fit. EEPROM has a limited number of writes, so the new value
   lives in RAM immediately and is only written down occasionally, and
   only when it has really changed. */
unsigned long lastScaleSave = 0;
long          savedScale     = 0;
const unsigned long SCALE_SAVE_EVERY = 15000;

/* The stream is 50 lines a second, which buries anything you ask it to
   print. 'p' and 's' pause it so you can actually read the answer. It
   resumes by itself after half a minute, so a forgotten pause can never
   be mistaken for dead hardware. */
bool          streaming   = true;
unsigned long pausedAt    = 0;
const unsigned long AUTO_RESUME_MS = 30000;
float smoothed  = 0;
uint8_t touchTick = 0;
int   touchRef[6];
bool  covered[6] = {false,false,false,false,false,false};

const float NOTE_HZ[7] = {523.25, 493.88, 440.00, 392.00, 349.23, 329.63, 261.63};

bool ready() { return digitalRead(HX_DOUT) == LOW; }

long readRaw() {
  long v = 0;
  for (uint8_t i = 0; i < 24; i++) {
    digitalWrite(HX_SCK, HIGH);
    delayMicroseconds(1);
    v = (v << 1) | digitalRead(HX_DOUT);
    digitalWrite(HX_SCK, LOW);
    delayMicroseconds(1);
  }
  for (uint8_t i = 24; i < PULSES; i++) {      // sets mode for the next read
    digitalWrite(HX_SCK, HIGH);
    delayMicroseconds(1);
    digitalWrite(HX_SCK, LOW);
    delayMicroseconds(1);
  }
  if (v & 0x800000L) v |= 0xFF000000L;         // it's a signed 24-bit number
  return v;
}

long waitRead() {                              // blocks until a fresh sample
  unsigned long t0 = millis();
  while (!ready()) { if (millis() - t0 > 500) return baseline; }
  return readRaw();
}

void tareBaseline() {
  long v[32];
  long sum = 0;
  for (uint8_t i = 0; i < 32; i++) { v[i] = waitRead(); sum += v[i]; }
  baseline = sum / 32;

  long lo = v[0], hi = v[0];
  for (uint8_t i = 1; i < 32; i++) { if (v[i] < lo) lo = v[i]; if (v[i] > hi) hi = v[i]; }
  noiseBand = (hi - lo) * 2;                   // headroom over the jitter we just saw
  if (noiseBand < 150) noiseBand = 150;        // a suspiciously quiet sample is not proof of a perfect sensor
  if (noiseBand > fullScale / 6) noiseBand = fullScale / 6;   // never squelch a real breath

  Serial.print(F("# zeroed, baseline="));
  Serial.print(baseline);
  Serial.print(F(" noiseBand="));
  Serial.println(noiseBand);
}

void saveFullScale(long v) {
  fullScale = v;
  EEPROM.put(EE_ADDR, EE_MAGIC);
  EEPROM.put(EE_ADDR + 2, v);
}

void loadFullScale() {
  uint16_t magic = 0;
  EEPROM.get(EE_ADDR, magic);
  if (magic == EE_MAGIC) {
    long v = 0;
    EEPROM.get(EE_ADDR + 2, v);
    if (v > 200 && v < 4000000L) { fullScale = v; return; }
  }
  fullScale = DEFAULT_FULL_SCALE;
}

void pauseStream() { streaming = false; pausedAt = millis(); }
void resumeStream() { streaming = true; Serial.println(F("# streaming again")); }

void printSettings() {
  pauseStream();                       // so the answer stays on screen
  Serial.println(F("#"));
  Serial.print(F("# FULL_SCALE=")); Serial.print(fullScale);
  if (fullScale == DEFAULT_FULL_SCALE) Serial.print(F("   <-- DEFAULT, 'c' has never been run"));
  Serial.println();
  Serial.print(F("# baseline="));   Serial.println(baseline);
  Serial.print(F("# noiseBand=")); Serial.println(noiseBand);
  Serial.print(F("# PULSES="));     Serial.println(PULSES);
  Serial.println(F("# paused so you can read this. type s + Enter to resume."));
  Serial.println(F("#"));
}

// ---- the whole point: the Arduino watches for the peak, not you ----
void calibrate() {
  Serial.println(F("#"));
  Serial.println(F("# ---- CALIBRATION ----"));
  Serial.println(F("# Do not blow yet. Sitting quietly..."));
  tareBaseline();

  for (int8_t i = 3; i >= 1; i--) {
    Serial.print(F("# blow in ")); Serial.println(i);
    delay(1000);
  }
  Serial.println(F("# BLOW NOW - one comfortable breath - 6 seconds"));

  long peak = 0;
  unsigned long t0 = millis();
  uint8_t dots = 0;
  while (millis() - t0 < 6000) {
    if (!ready()) continue;
    long v = readRaw() - baseline;
    if (v > peak) peak = v;
    if ((millis() - t0) / 1000 > dots) { dots++; Serial.println(F("# ...")); }
  }

  Serial.print(F("# your peak was ")); Serial.println(peak);

  if (peak < 200) {
    Serial.println(F("# TOO SMALL - nothing reached the sensor."));
    Serial.println(F("# Check the 4 wires, then try 'c' again."));
    return;
  }

  /* HEADROOM. This used to be peak * 0.85, which made a comfortable
     breath read 1.18 and get clipped to 1.000 - and a HARDER breath read
     1.000 as well, so the two were indistinguishable.

     That was fine when the sketch was the last word on the number. It no
     longer is: the game now measures each child's own normal breath at
     the start of every session and puts it at the MIDDLE of the bar, so
     that gentler and firmer both have somewhere to go. For that to work
     the sketch must hand over a number with room left above it.

     At 2.2, a comfortable breath arrives as about 0.45 and a breath twice
     as hard as about 0.91 - still inside range, still distinguishable. */
  long fs = (long)(peak * 2.2);
  saveFullScale(fs);
  savedScale = fs;
  if (noiseBand > fs / 6) noiseBand = fs / 6;
  Serial.print(F("# SAVED. FULL_SCALE = ")); Serial.println(fs);
  Serial.println(F("# It is stored on the board and survives unplugging."));
  Serial.println(F("# ---- DONE ---- blow again and watch the first number."));
  Serial.println(F("#"));
  smoothed = 0;
}

void setup() {
  Serial.begin(115200);
  pinMode(HX_DOUT, INPUT);
  pinMode(HX_SCK, OUTPUT);
  digitalWrite(HX_SCK, LOW);

  delay(300);
  waitRead();                                  // first read is junk
  loadFullScale();                             // before the tare: it clamps noiseBand
  savedScale = fullScale;
  tareBaseline();

  for (uint8_t i = 0; i < 6; i++) touchRef[i] = ADCTouch.read(HOLE_PIN[i], 100);

  Serial.println(F("READY"));
  printSettings();
  Serial.println(F("# type c + Enter to calibrate, z to re-zero, p to print settings"));
}

void readFingering() {
  for (uint8_t i = 0; i < 6; i++) {
    int v = ADCTouch.read(HOLE_PIN[i], 8) - touchRef[i];
    covered[i] = (v > TOUCH_DELTA);
  }
}

uint8_t coveredCount() {
  uint8_t n = 0;
  for (uint8_t i = 0; i < 6; i++) if (covered[i]) n++;
  return n;
}

void handleSerial() {
  while (Serial.available()) {
    char c = Serial.read();
    if (c == 'c' || c == 'C') { streaming = true; calibrate(); }
    else if (c == 'z' || c == 'Z') { streaming = true; tareBaseline(); }
    else if (c == 'p' || c == 'P') printSettings();
    else if (c == 's' || c == 'S') { if (streaming) pauseStream(); else resumeStream(); }
  }
}

void loop() {
  handleSerial();

  if (!ready()) return;                        // no new sample yet

  long v = readRaw();
  long d = v - baseline;

  // ---- chase the zero (see the note by noiseBand) ----
  if (d < 0) {
    long down = d / 8;
    if (down < -noiseBand) down = -noiseBand;  // bounded, so one odd sample cannot yank it
    baseline += down;
  } else if (d < noiseBand) {
    // ~5 second time constant, with a floor of 1 count so it always
    // makes progress even when the division rounds away to nothing.
    baseline += (d / 200) + 1;
  }
  // d >= noiseBand: that is air. Leave the zero exactly where it is.

  /* Air off the top of the scale: widen to fit, with room to spare, so
     the child immediately has somewhere above "normal" to go. */
  if (d > fullScale) {
    fullScale = (long)(d * 1.7);
    if (fullScale > 4000000L) fullScale = 4000000L;
    if (noiseBand > fullScale / 6) noiseBand = fullScale / 6;
    Serial.print(F("# scale was too small - widened to "));
    Serial.println(fullScale);
  }
  // Write it down now and then, not every sample.
  if (fullScale != savedScale && millis() - lastScaleSave > SCALE_SAVE_EVERY) {
    lastScaleSave = millis();
    savedScale = fullScale;
    saveFullScale(fullScale);
  }

  long raw = d - noiseBand;                    // squelch: silence is silent
  if (raw < 0) raw = 0;

  // Divided by the range ABOVE the squelch, so subtracting the band does
  // not quietly shrink every reading.
  long fsSpan = (fullScale > noiseBand) ? (fullScale - noiseBand) : fullScale;
  float norm = (float)raw / (float)fsSpan;
  if (norm > 1) norm = 1;

  smoothed = (ALPHA * norm) + ((1.0 - ALPHA) * smoothed);

  if (++touchTick >= TOUCH_EVERY) { touchTick = 0; readFingering(); }

  if (!streaming) {
    if (millis() - pausedAt > AUTO_RESUME_MS) resumeStream();
    return;
  }

  Serial.print(smoothed, 3);
  Serial.print(',');
  Serial.println(NOTE_HZ[coveredCount()], 1);
}