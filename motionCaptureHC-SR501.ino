// XIAO ESP32S3 + HC-SR501 with smoothing and min hold times
#define PIR_PIN D1
#define BAUD 115200

// Tuning knobs (ms)
const uint32_t STABLE_HIGH_MS = 120;   // require this much continuous HIGH to call it motion
const uint32_t STABLE_LOW_MS  = 250;   // require this much continuous LOW to call it no motion
const uint32_t MIN_ON_MS      = 2000;  // keep state ON at least this long once triggered
const uint32_t MIN_OFF_MS     = 1000;  // keep state OFF at least this long once cleared
const uint32_t WARMUP_MS      = 60000; // PIR warm-up

bool motion_state = false;             // our debounced state
uint32_t state_changed_at = 0;         // when we last flipped states
uint32_t last_edge_time = 0;           // tracks stability window timing
int last_raw = -1;

void setup() {
  delay(2000);
  Serial.begin(BAUD);
  pinMode(PIR_PIN, INPUT);         // HC-SR501 drives the line; no pullups needed
  pinMode(LED_BUILTIN, OUTPUT);

  Serial.println("Booting...");
  Serial.println("Warm-up 60s: sensor may flicker.");
  state_changed_at = millis();
}

void loop() {
  uint32_t now = millis();
  int raw = digitalRead(PIR_PIN);

  // show a heartbeat + raw level every second (useful for tuning)
  static uint32_t last_tick = 0;
  if (now - last_tick >= 1000) {
    last_tick = now;
    Serial.print("Tick | raw=");
    Serial.print(raw);
    Serial.print(" | state=");
    Serial.println(motion_state ? "MOTION" : "NO MOTION");
  }

  // During warm-up, just mirror raw to LED but don't change state permanently
  if (now < WARMUP_MS) {
    digitalWrite(LED_BUILTIN, raw ? LOW : HIGH); // active-low LED on some XIAOs
    delay(50);
    return;
  }

  // Detect stability windows (raw must stay the same for a while)
  if (raw != last_raw) {
    last_raw = raw;
    last_edge_time = now;   // reset stability timer on every transition
  }

  bool stable_high = (raw == HIGH) && (now - last_edge_time >= STABLE_HIGH_MS);
  bool stable_low  = (raw == LOW)  && (now - last_edge_time >= STABLE_LOW_MS);

  if (!motion_state) {
    // Currently OFF; allow turning ON only if:
    //  - input has been stably HIGH, and
    //  - we've respected minimum OFF time
    if (stable_high && (now - state_changed_at >= MIN_OFF_MS)) {
      motion_state = true;
      state_changed_at = now;
      Serial.println("Motion detected (debounced).");
    }
  } else {
    // Currently ON; allow turning OFF only if:
    //  - input has been stably LOW, and
    //  - we've respected minimum ON time
    if (stable_low && (now - state_changed_at >= MIN_ON_MS)) {
      motion_state = false;
      state_changed_at = now;
      Serial.println("No motion (debounced).");
    }
  }

  // Drive the user LED from the debounced state
  digitalWrite(LED_BUILTIN, motion_state ? LOW : HIGH);

  delay(20); // small loop delay
}
