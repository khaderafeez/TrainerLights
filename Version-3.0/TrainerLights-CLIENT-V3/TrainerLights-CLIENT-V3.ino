// ============================================================
// TrainerLights CLIENT - NodeMCU ESP-12E (V3.0 — WS2812 RGB)
// Authors: Khader Afeez <khaderafeez16@gmail.com>
//          Abin Abraham <abinabraham176@gmail.com>
//          James Kattoor <jameskattoor004@gmail.com>
//
// V2.0 fixes carried forward:
//   - Late response guarded by isStopped flag — discard any
//     result that arrives after the server sends stop_test
//   - pulseIn() blocking replaced with ISR-based non-blocking
//     echo timing on the HC-SR04
//   - Result blink task properly disabled on cancelStimulus
//
// V3.0 changes (RGB upgrade):
//   - All 4 discrete LEDs (status, stim, hit, miss) replaced
//     by a 4-pixel WS2812B strip driven on a single data pin
//   - FastLED library handles the timing-sensitive protocol
//   - Pixel layout: 0=Status, 1=Stimulus, 2=Hit, 3=Miss
//   - Colors: Status=yellow/green, Stimulus=blue,
//             Hit=green, Miss=red, Blink-all=white
//   - "resultPixel / resultColor" replaces "resultPin" so
//     the blink task knows which pixel + color to flash
//   - DATA_PIN changed to D5 (avoids D4/GPIO2 boot glitch)
//   - Brightness capped at LED_BRIGHT (80/255) for 3.3V rail
//
// Hardware notes:
//   - WS2812B VCC can be 3.3V at low brightness or 5V from
//     a separate rail (share GND). Data signal at 3.3V is
//     enough for WS2812B; add a 300-470Ω series resistor on
//     the data line if you see glitching.
//   - HC-SR04 on D1 (TRIG) and D2 (ECHO) — unchanged.
//
// Required libraries (install via Arduino Library Manager):
//   FastLED, WebSocketsClient, ArduinoJson, TaskScheduler
// ============================================================

#include <ESP8266WiFi.h>
#include <TaskScheduler.h>
#include <ArduinoJson.h>
#include <WebSocketsClient.h>
#include <EEPROM.h>
#include <FastLED.h>      // WS2812 driver

extern "C" {
#include "user_interface.h"  // ESP8266 SDK — needed for wifi_set_sleep_type
}

// ============================================================
// Pin definitions
// ============================================================
#define PIN_TRIG  D1   // HC-SR04 trigger (output)
#define PIN_ECHO  D2   // HC-SR04 echo (input, interrupt-capable)
#define DATA_PIN  D5   // WS2812B data line
                       // D5 = GPIO14 — safe at boot (no pull-up/down conflict)
                       // Avoid D4/GPIO2: pulled HIGH at boot, glitches strip

// ============================================================
// WS2812 strip configuration
// ============================================================
#define LED_COUNT  4    // 4 pixels = 4 roles
#define LED_BRIGHT 80   // Global brightness 0-255.
                        // Keep ≤150 when powering strip from 3.3V pin.
                        // At 80 each pixel draws ~10-15mA, total ~60mA max —
                        // safely within NodeMCU 3.3V regulator limits.

// Pixel index assignments — physical order on the strip
#define PX_STATUS  0    // Yellow blink = WiFi only; green solid = WS connected
#define PX_STIM    1    // Blue  — light up on stimulus (react now!)
#define PX_HIT     2    // Green — flash on successful hit
#define PX_MISS    3    // Red   — flash on miss / timeout

CRGB leds[LED_COUNT];   // FastLED pixel array

// ============================================================
// Color palette
// Tweak these to match your strip's viewing angle and ambient
// light. WS2812B color order is GRB — FastLED handles this
// automatically with the GRB template parameter.
// ============================================================
#define COL_OFF          CRGB::Black
#define COL_STATUS_WIFI  CRGB(60,  60,   0)    // Dim yellow: WiFi up, WS not yet
#define COL_STATUS_OK    CRGB(0,   80,  20)    // Solid green: fully connected
#define COL_STIM         CRGB(0,   60, 200)    // Blue: stimulus active
#define COL_HIT          CRGB(0,  200,  30)    // Green: hit confirmed
#define COL_MISS         CRGB(220,  20,   0)   // Red: miss / timeout
#define COL_BLINK_ALL    CRGB(200, 200, 200)   // White: test-lights command

// ============================================================
// WiFi / WebSocket credentials
// Must match the server's AP settings.
// ============================================================
const char* SSID      = "TrainerLights";
const char* PASSWORD  = "1234567890";
const char* SERVER_IP = "192.168.4.1";
const int   WS_PORT   = 81;

// ============================================================
// EEPROM layout
// We store the node's assigned ID so it survives power cycles.
// ============================================================
#define EEPROM_SIZE 8
#define ADDR_ID     0   // Byte: stored node ID
#define ADDR_MAGIC  1   // Byte: magic number confirms data is valid
#define MAGIC_VALUE 0xAB

// ============================================================
// Connection state
// ============================================================
WebSocketsClient webSocket;
bool wsConnected = false;   // True after WS handshake completes
bool wsStarted   = false;   // True after webSocket.begin() called
int  nodeID      = 0;       // Assigned by server; 0 = unassigned

// ============================================================
// Stimulus state
// ============================================================
bool          active        = false;   // True while waiting for athlete reaction
bool          isStopped     = false;   // True after server sends stop_test —
                                       // any in-flight sendResult is discarded
int           stimTimeoutMs = 1000;    // Max reaction window (ms)
int           stimDelayMs   = 0;       // Delay before blue LED lights up (ms)
int           detMin        = 0;       // Minimum valid detection distance (cm)
int           detMax        = 50;      // Maximum valid detection distance (cm)
unsigned long stimOnTime    = 0;       // millis() when blue LED turned on
bool          resultSent    = false;   // Guards against double-sending

// V3: replaces resultPin (int) and resultColor is new.
// The blink task flashes whichever pixel + color we set here.
int  resultPixel = PX_HIT;
CRGB resultColor = COL_HIT;

// ============================================================
// Non-blocking HC-SR04 via echo interrupt
//
// Why non-blocking? pulseIn() freezes the CPU for up to 23ms
// per measurement. That starves the WebSocket keep-alive and
// causes connection drops under heavy polling. Instead, we
// fire the trigger on a schedule and use an ISR to timestamp
// the echo rising + falling edges.
// ============================================================
volatile unsigned long echoStart = 0;   // micros() at echo HIGH
volatile unsigned long echoDur   = 0;   // pulse duration (micros)
volatile bool          echoReady = false; // Set by ISR, cleared by consumer

// ISR: fires on both edges of the ECHO pin.
// ICACHE_RAM_ATTR ensures it runs from RAM, not flash cache —
// required for reliable ISR timing on ESP8266.
void ICACHE_RAM_ATTR echoISR() {
  if (digitalRead(PIN_ECHO) == HIGH) {
    // Rising edge: start timing
    echoStart = micros();
  } else {
    // Falling edge: compute duration
    unsigned long dur = micros() - echoStart;
    // Valid HC-SR04 window: 150µs (2cm) to ~14700µs (252cm).
    // We cap at 15000µs to match a ~257cm hard ceiling — anything
    // longer is either a timeout or a spurious reflection.
    if (dur > 0 && dur < 15000) {
      echoDur   = dur;
      echoReady = true;
    }
  }
}

// Fires a 10µs trigger pulse to start a new measurement.
// Call this on the tTrigger schedule (every 30ms).
void triggerPulse() {
  echoReady = false;        // Clear previous reading
  digitalWrite(PIN_TRIG, LOW);  delayMicroseconds(2);
  digitalWrite(PIN_TRIG, HIGH); delayMicroseconds(10);
  digitalWrite(PIN_TRIG, LOW);
}

// Read the latest ISR-computed distance.
// Returns 999 if no fresh reading is available.
long getDistance() {
  if (!echoReady) return 999;
  echoReady = false;
  // Distance (cm) = (duration in µs × speed of sound) / 2
  // Speed of sound ≈ 0.0343 cm/µs at 20°C
  return (long)((echoDur * 0.0343f) / 2.0f);
}

// ============================================================
// LED helpers (WS2812 / FastLED)
// ============================================================

// Set a single pixel and push to the strip immediately.
// Only call when the change needs to be visible right away;
// for batched updates, set leds[] directly then call FastLED.show().
void setPixel(int idx, CRGB color) {
  leds[idx] = color;
  FastLED.show();
}

// Turn off only the game-state pixels (stim, hit, miss).
// Status pixel is managed separately by updateStatusLED().
void clearGameLEDs() {
  leds[PX_STIM] = COL_OFF;
  leds[PX_HIT]  = COL_OFF;
  leds[PX_MISS] = COL_OFF;
  FastLED.show();
}

// ============================================================
// Task Scheduler
// Cooperative multitasking — keeps the main loop non-blocking.
// ============================================================
Scheduler runner;

// Forward declarations for task callbacks
void taskOnDelay();
void taskMeasure();
void taskOnTimeout();
void taskLedsOff();
void taskRegister();
void taskResultBlink();
void taskTrigger();

// tOnDelay: armed when a stimulus arrives; fires after stimDelayMs
// to turn on the blue pixel and begin the reaction window.
Task tOnDelay    (0,   TASK_ONCE,    &taskOnDelay,     &runner, false);

// tMeasure: polls ultrasonic distance every 25ms during the
// active reaction window.
Task tMeasure    (25,  TASK_FOREVER, &taskMeasure,     &runner, false);

// tOnTimeout: fires if the athlete doesn't react within the
// reaction window. Counts as an error.
Task tOnTimeout  (10000, TASK_ONCE,  &taskOnTimeout,   &runner, false);

// tLedsOff: turns off game LEDs after the hit/miss blink period.
Task tLedsOff    (300, TASK_ONCE,    &taskLedsOff,     &runner, false);

// tRegister: fires once on connect to send our identity to server.
Task tRegister   (600, TASK_ONCE,    &taskRegister,    &runner, false);

// tResultBlink: rapidly toggles the result pixel (green or red)
// to give a visible hit/miss flash. Runs until tLedsOff stops it.
Task tResultBlink(100, TASK_FOREVER, &taskResultBlink, &runner, false);

// tTrigger: fires a new HC-SR04 pulse every 30ms while a
// stimulus is active. Non-blocking — ISR handles the echo.
Task tTrigger    (30,  TASK_FOREVER, &taskTrigger,     &runner, false);

// ============================================================
// EEPROM helpers
// ============================================================

// Read the previously stored node ID.
// Returns 0 if EEPROM is uninitialized (first boot or erased).
int readID() {
  EEPROM.begin(EEPROM_SIZE);
  byte magic = EEPROM.read(ADDR_MAGIC);
  int  id    = (magic == MAGIC_VALUE) ? (int)EEPROM.read(ADDR_ID) : 0;
  EEPROM.end();
  return id;
}

// Persist a node ID to EEPROM so it survives power cycles.
// The magic byte confirms the write was intentional.
void saveID(int id) {
  EEPROM.begin(EEPROM_SIZE);
  EEPROM.write(ADDR_ID,    (byte)id);
  EEPROM.write(ADDR_MAGIC, MAGIC_VALUE);
  EEPROM.commit();
  EEPROM.end();
}

// ============================================================
// cancelStimulus
// Stops all active tasks and turns off game LEDs.
// Called on stop_test, disconnect, or new stimulus (to
// cleanly reset before starting the next one).
// ============================================================
void cancelStimulus() {
  active = false;
  tOnDelay.disable();
  tMeasure.disable();
  tOnTimeout.disable();
  tLedsOff.disable();
  tResultBlink.disable();  // Must stop blink task or pixel stays on
  tTrigger.disable();      // Stop ultrasonic polling
  clearGameLEDs();
}

// ============================================================
// sendResult
// Sends the response payload to the server.
// Guards: won't send if already sent this stimulus, or if
// isStopped is true (server sent stop_test — result is stale).
// ============================================================
void sendResult(unsigned long reactionMs, int distCm, int error) {
  if (!wsConnected || resultSent || isStopped) return;
  resultSent = true;

  StaticJsonDocument<200> doc;
  doc["type"]     = "response";
  doc["node_id"]  = nodeID;
  doc["time"]     = (int)reactionMs;
  doc["distance"] = distCm;
  doc["error"]    = error;
  doc["ip"]       = WiFi.localIP().toString();
  doc["mac"]      = WiFi.macAddress();

  String msg;
  serializeJson(doc, msg);
  webSocket.sendTXT(msg);
}

// ============================================================
// Task: Register with server
// Fires 600ms after WS connects (enough time for the server
// to be ready to receive). Sends MAC, IP, and last known ID.
// ============================================================
void taskRegister() {
  if (!wsConnected) return;

  StaticJsonDocument<150> doc;
  doc["type"]    = "sensor";
  doc["ip"]      = WiFi.localIP().toString();
  doc["mac"]     = WiFi.macAddress();
  doc["node_id"] = nodeID;

  String msg;
  serializeJson(doc, msg);
  webSocket.sendTXT(msg);

  // Status pixel solid green once registered
  setPixel(PX_STATUS, COL_STATUS_OK);
}

// ============================================================
// Task: Delay expired — go live
// Turns on the blue stimulus pixel and opens the reaction window.
// ============================================================
void taskOnDelay() {
  resultSent = false;         // Fresh stimulus — allow a new result
  stimOnTime = millis();      // Start the reaction clock
  active     = true;

  // Blue = react now!
  setPixel(PX_STIM, COL_STIM);

  // Arm the timeout watchdog
  tOnTimeout.setInterval(stimTimeoutMs > 100 ? stimTimeoutMs : 100);
  tOnTimeout.restartDelayed();

  // Start non-blocking ultrasonic polling
  tTrigger.enable();
  tMeasure.enable();
}

// ============================================================
// Task: Fire a trigger pulse (called every 30ms by tTrigger)
// ============================================================
void taskTrigger() {
  triggerPulse();
}

// ============================================================
// Task: Blink the result pixel
// Alternates the pixel between its result color and off,
// giving a visible flash without blocking.
// Uses a local toggle instead of digitalRead (no GPIO involved).
// ============================================================
bool blinkPhase = false;  // Local toggle for blink phase

void taskResultBlink() {
  blinkPhase = !blinkPhase;
  leds[resultPixel] = blinkPhase ? resultColor : COL_OFF;
  FastLED.show();
}

// ============================================================
// Task: Measure distance — check for a valid hit
// Runs every 25ms while a stimulus is active.
// ============================================================
void taskMeasure() {
  if (!active) {
    // Stimulus was cancelled externally — stop polling
    tMeasure.disable();
    tTrigger.disable();
    return;
  }

  long dist = getDistance();

  // Ignore readings outside the configured detection window
  // or no-fresh-reading sentinel (999).
  if (dist <= 0 || dist >= 999 || dist > detMax || dist < detMin) return;

  unsigned long reactionMs = millis() - stimOnTime;

  // Anti-cheat: ignore detections in the first 10ms
  // (the athlete can't physically react that fast — it's noise
  // from the trigger pulse bouncing back off nearby surfaces).
  if (reactionMs < 10) return;

  // Valid hit — stop everything
  tOnTimeout.disable();
  tMeasure.disable();
  tTrigger.disable();
  active = false;

  // Flash green on PX_HIT
  resultPixel = PX_HIT;
  resultColor = COL_HIT;
  leds[PX_STIM] = COL_OFF;   // Turn off blue stimulus
  setPixel(PX_HIT, COL_HIT); // Green on immediately
  blinkPhase = true;
  tResultBlink.enable();
  tLedsOff.setInterval(2500);
  tLedsOff.restartDelayed();

  sendResult(reactionMs, (int)dist, 0);
}

// ============================================================
// Task: Stimulus timeout — reaction window expired
// Counts as a miss. Flashes red on PX_MISS.
// ============================================================
void taskOnTimeout() {
  if (!active) return;  // Might fire just after a valid hit — guard

  tMeasure.disable();
  tTrigger.disable();
  active = false;

  // Flash red on PX_MISS
  resultPixel = PX_MISS;
  resultColor = COL_MISS;
  leds[PX_STIM] = COL_OFF;
  setPixel(PX_MISS, COL_MISS);
  blinkPhase = true;
  tResultBlink.enable();
  tLedsOff.setInterval(2500);
  tLedsOff.restartDelayed();

  // Report total elapsed time as response time for the error record
  sendResult((unsigned long)(stimDelayMs + stimTimeoutMs), 0, 1);
}

// ============================================================
// Task: Turn off game LEDs after the blink period
// ============================================================
void taskLedsOff() {
  tResultBlink.disable();
  clearGameLEDs();
}

// ============================================================
// Status LED management
// Called every loop iteration. Blinks PX_STATUS to signal
// connection state when the WS is not fully connected.
//   Fast blink (200ms): no WiFi at all
//   Slow blink (700ms): WiFi connected, WS not yet
//   Solid green:        fully connected and registered
// ============================================================
unsigned long blinkLast  = 0;
bool          blinkState = false;

void updateStatusLED() {
  if (wsConnected) {
    // Solid green — set once and return (no blinking needed)
    if (leds[PX_STATUS] != COL_STATUS_OK) {
      setPixel(PX_STATUS, COL_STATUS_OK);
    }
    return;
  }

  // Not connected — blink yellow at rate based on WiFi state
  unsigned long interval = (WiFi.status() == WL_CONNECTED) ? 700 : 200;
  if (millis() - blinkLast >= interval) {
    blinkLast  = millis();
    blinkState = !blinkState;
    setPixel(PX_STATUS, blinkState ? COL_STATUS_WIFI : COL_OFF);
  }
}

// ============================================================
// WebSocket event handler
// ============================================================
void onWSEvent(WStype_t type, uint8_t* payload, size_t length) {
  switch (type) {

    // ---- Disconnected from server ----
    case WStype_DISCONNECTED:
      wsConnected = false;
      setPixel(PX_STATUS, COL_OFF);  // Status off — will blink in updateStatusLED
      cancelStimulus();
      tRegister.disable();
      break;

    // ---- Connected to server ----
    case WStype_CONNECTED:
      wsConnected = true;
      isStopped   = false;    // Clear stop flag on fresh connection
      tRegister.restartDelayed();  // Schedule registration message
      break;

    // ---- Text message from server ----
    case WStype_TEXT: {
      StaticJsonDocument<300> doc;
      if (deserializeJson(doc, payload)) break;  // Ignore malformed JSON
      const char* t = doc["type"]; if (!t) break;

      // --- Server assigns or confirms our node ID ---
      if (strcmp(t, "assign_id") == 0) {
        int newID = doc["node_id"] | 0;
        if (newID > 0 && newID != nodeID) {
          nodeID = newID;
          saveID(nodeID);
          // Re-register with the new ID so server roster is current
          tRegister.restartDelayed();
        }
      }

      // --- Stimulus command — light up and wait for reaction ---
      if (strcmp(t, "stimulus") == 0) {
        isStopped     = false;   // New stimulus means session is live

        stimTimeoutMs = doc["timeout"] | 1000;
        stimDelayMs   = doc["delay"]   | 0;
        detMin        = doc["min_detection_range"] | 0;
        detMax        = doc["max_detection_range"] | 50;

        // Clamp to sane values
        if (stimTimeoutMs < 100) stimTimeoutMs = 100;
        if (stimDelayMs   < 0)   stimDelayMs   = 0;

        // Cancel any previous in-flight stimulus cleanly
        cancelStimulus();

        // Arm the delay task; minimum 10ms to avoid zero-interval glitch
        tOnDelay.setInterval(stimDelayMs > 0 ? stimDelayMs : 10);
        tOnDelay.restartDelayed();
      }

      // --- Stop test — discard any in-flight result ---
      if (strcmp(t, "stop_test") == 0) {
        isStopped = true;   // Gate sendResult to discard late responses
        cancelStimulus();
      }

      // --- Blink-all test command (from "Test Lights" button) ---
      // V3: flash all 4 pixels white for 500ms
      if (strcmp(t, "blink") == 0) {
        fill_solid(leds, LED_COUNT, COL_BLINK_ALL);
        FastLED.show();
        tLedsOff.setInterval(500);
        tLedsOff.restartDelayed();
      }

      // --- Remote restart (debug / OTA prep) ---
      if (strcmp(t, "restart") == 0) {
        cancelStimulus();
        delay(100);
        ESP.restart();
      }

      // --- Reset stored ID (force server to assign a fresh one) ---
      if (strcmp(t, "reset_id") == 0) {
        saveID(0);
        nodeID = 0;
        delay(100);
        ESP.restart();
      }

      break;
    }

    default: break;
  }
}

// ============================================================
// WiFi init
// ============================================================
void connectWiFi() {
  WiFi.persistent(false);       // Don't save credentials to flash
  WiFi.setAutoReconnect(true);  // Reconnect automatically on drop
  WiFi.setSleepMode(WIFI_NONE_SLEEP); // No power-save — reduces latency
  WiFi.mode(WIFI_STA);
  WiFi.begin(SSID, PASSWORD);
}

// ============================================================
// WebSocket init
// ============================================================
void connectWS() {
  webSocket.begin(SERVER_IP, WS_PORT, "/");
  webSocket.onEvent(onWSEvent);
  webSocket.setReconnectInterval(3000);  // Retry every 3s on drop
  // Heartbeat: ping every 10s, pong within 3s, drop after 2 failures
  webSocket.enableHeartbeat(10000, 3000, 2);
  wsStarted = true;
}

// ============================================================
// SETUP
// ============================================================
void setup() {
  Serial.begin(115200);

  // ---- Pin setup ----
  pinMode(PIN_TRIG, OUTPUT);
  pinMode(PIN_ECHO, INPUT);
  digitalWrite(PIN_TRIG, LOW);

  // ---- FastLED init ----
  // WS2812B uses GRB color order (not RGB).
  // FastLED handles this with the GRB template parameter.
  FastLED.addLeds<WS2812B, DATA_PIN, GRB>(leds, LED_COUNT);
  FastLED.setBrightness(LED_BRIGHT);
  FastLED.clear();
  FastLED.show();

  // ---- ISR: non-blocking HC-SR04 echo timing ----
  // CHANGE triggers the ISR on both rising and falling edges,
  // which lets us measure the full pulse width.
  attachInterrupt(digitalPinToInterrupt(PIN_ECHO), echoISR, CHANGE);

  // ---- ESP8266 WiFi sleep mode (SDK level) ----
  wifi_set_sleep_type(NONE_SLEEP_T);

  // ---- Load stored node ID ----
  nodeID = readID();

  // ---- Boot blink: all pixels white for 300ms ----
  // Gives visual confirmation that the strip is wired and working.
  fill_solid(leds, LED_COUNT, COL_BLINK_ALL);
  FastLED.show();
  delay(300);
  FastLED.clear();
  FastLED.show();

  // ---- WiFi + WebSocket ----
  connectWiFi();
  connectWS();

  Serial.println("TrainerLights Client v3.0 RGB started.");
}

// ============================================================
// LOOP
// Drives WebSocket keep-alive, task scheduler, status LED,
// and a watchdog that catches WiFi drops without a WS event.
// ============================================================
void loop() {
  // Must call every iteration — handles incoming frames,
  // heartbeat pings, and reconnection logic.
  if (wsStarted) webSocket.loop();

  // Run any due scheduled tasks
  runner.execute();

  // Update status pixel blink / solid based on connection state
  updateStatusLED();

  // Watchdog: detect WiFi drop that didn't trigger a WS event.
  // Checks every 2s — cheap enough to not affect timing.
  static unsigned long lastCheck = 0;
  if (millis() - lastCheck > 2000) {
    lastCheck = millis();
    if (WiFi.status() != WL_CONNECTED && wsConnected) {
      wsConnected = false;
      setPixel(PX_STATUS, COL_OFF);
      cancelStimulus();
    }
  }
}
