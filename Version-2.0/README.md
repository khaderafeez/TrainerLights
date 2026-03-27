# TrainerLights v2.0

> Wireless athletic reaction training system — ESP32 server + NodeMCU sensor nodes

**Authors:** Khader Afeez · Abin Abraham · James Kattoor

---

## What it does

TrainerLights is a hardware-software system for athletic reaction time training. A central ESP32 board hosts a WiFi access point and web dashboard. Up to 12 NodeMCU sensor nodes connect wirelessly, each with an ultrasonic distance sensor and LEDs. The server randomly or sequentially activates a node (blue LED lights up), the athlete reacts by touching/reaching the sensor, and response time + distance are recorded in real time.

---

## Hardware

| Component | Board | Role |
|-----------|-------|------|
| Server | ESP32 WROOM-32D | WiFi AP, WebSocket server, web dashboard |
| Client (×N) | NodeMCU ESP-12E | Sensor node — HC-SR04 + 4 LEDs |

### NodeMCU pin map

| Pin | Function | Notes |
|-----|----------|-------|
| D1 | HC-SR04 TRIG | Trigger output |
| D2 | HC-SR04 ECHO | Interrupt-driven input |
| D4 | LED_OK (Green) | Hit confirmation |
| D5 | LED_ERR (Red) | Miss / timeout |
| D6 | LED_STIM (Blue) | Active stimulus — react now |
| D7 | LED_STATUS (Yellow) | WiFi/WS connection status |

> **Note:** HC-SR04 requires 5V VCC. Use a voltage divider (1kΩ + 2kΩ) on the ECHO pin to bring the 5V signal down to 3.3V safe for NodeMCU.

---

## Network

```
WiFi SSID:    TrainerLights
Password:     1234567890
AP IP:        192.168.4.1
Dashboard:    http://192.168.4.1
WebSocket:    ws://192.168.4.1:81/
mDNS:         trainerlights.local
Max nodes:    12
```

---

## Getting started

### 1. Flash the server

- Board: `ESP32 Dev Module`
- Upload speed: `921600`
- CPU: `240MHz`
- Flash `server/TrainerLights_Server_v2.ino`

### 2. Flash each client node

- Board: `NodeMCU 1.0 (ESP-12E Module)`
- Upload speed: `115200`
- CPU: `80MHz`
- Flash `client/TrainerLights_Client_v2.ino`

### 3. Required libraries

Install via Arduino Library Manager:

```
arduinoWebSockets   by Markus Sattler
ArduinoJson         by Benoit Blanchon    (v6.x or v7.x)
TaskScheduler       by Anatoli Arkhipenko
LinkedList          by Ivan Seidel        (server only)
```

### 4. First run

1. Power on ESP32 — AP starts immediately
2. Power on NodeMCU nodes — yellow LED blinks while connecting
3. Open `http://192.168.4.1` in browser
4. Nodes appear in **Hardware Nodes** list when connected
5. Tap **Test Lights** to verify all LEDs
6. Configure training parameters in **Settings**
7. Tap **Start** in the Session Timer to begin

---

## Training settings

| Setting | Default | Description |
|---------|---------|-------------|
| Mode | `random` | `random` or `sequence` |
| Delay min/max | 4000ms | Time between stimulus rounds |
| Timeout min/max | 1000–2000ms | Reaction window per stimulus |
| Detection min/max | 0–50cm | Valid distance range for hit |

---

## WebSocket protocol

All messages are JSON with a `type` field.

### Server → Node
| Type | Description |
|------|-------------|
| `stimulus` | Activate node — includes `timeout`, `delay`, `min/max_detection_range` |
| `assign_id` | Confirm node ID after registration |
| `blink` | All LEDs on 500ms — test/identify |

### Node → Server
| Type | Description |
|------|-------------|
| `sensor` | Registration — `mac`, `ip`, `node_id` |
| `response` | Result — `time` (ms), `distance` (cm), `error` (0/1) |

### Dashboard → Server
| Type | Description |
|------|-------------|
| `app_connected` | Register dashboard socket |
| `start_test` | Begin stimulus loop |
| `stop_test` | Halt all stimulus |
| `config` | Update training parameters |
| `clear_stats` | Reset session statistics |
| `remove_node` | Remove node by MAC |
| `blink_all` | Test all node LEDs |

---

## v2.0 fixes

| Bug | Fix |
|-----|-----|
| Memory leak on node disconnect | `removeSensorAt()` calls `delete s` before list removal |
| Stimulus stuck after node drop | Dropped node clears `stimulating` + `stimulatingNum` |
| Late response after stop_test | Server ignores response if `!isTesting`; client checks `isStopped` |
| `pulseIn()` blocking CPU | Replaced with echo ISR + `tTrigger` task (30ms non-blocking) |
| Null dereference in `sendSensorList` | Early return if `toSocket == -1` |
| Testing badge not clearing in UI | `sendStats()` now includes `is_testing` field |
| `tResultBlink` not stopped on cancel | `cancelStimulus()` disables `tResultBlink` |

---

## File structure

```
TrainerLights/
├── server/
│   └── TrainerLights_Server_v2.ino
├── client/
│   └── TrainerLights_Client_v2.ino
└── README.md
```

---

## Troubleshooting

**Node connects then immediately drops**
Check power supply stability. NodeMCU needs clean 3.3V — weak USB cables cause voltage sag during WiFi TX.

**Distance readings erratic**
Verify HC-SR04 VCC is 5V. Check voltage divider on ECHO pin.

**Dashboard stats frozen**
Refresh browser. WebSocket auto-reconnects in 3 seconds.

**Two nodes with same ID**
Remove one via the dashboard X button, power-cycle it to re-register with a fresh auto-assigned ID.

**Stimulus stops after a few rounds**
On v2.0 this should not occur. If it does, check Serial monitor — `stimulating` should clear on each response received.

---.
