# e-Shock LED — Fly Lab Experiment Controller

An integrated system for running timed electric-shock + LED-light sequences synchronized with high-speed Basler camera recording. Built for behavioral neuroscience experiments (e.g., Y-maze fly assays).

---

## Overview

| Component | Description |
|-----------|-------------|
| **Camera Interface** (Python) | Controls a Basler camera for live preview and recording, while commanding the Arduino over serial |
| **Arduino Firmware** (C++ / PlatformIO) | Drives a WS2812B LED strip and a shock-output pin on an Arduino Nano, responding to serial commands from the Python host |

### System Diagram

```
┌──────────────┐    USB / Serial     ┌─────────────────┐
│  Python Host │ ◄─────────────────► │  Arduino Nano   │
│  (Camera     │   115200 baud       │                 │
│   Interface) │                     │  Pin 3 → Shock  │
│              │                     │  Pin 8 → LEDs   │
└──────┬───────┘                     └─────────────────┘
       │
       │  Basler Pylon SDK
       ▼
 ┌────────────┐
 │  Basler    │
 │  Camera    │
 └────────────┘
```

---

## Hardware Requirements

- **Arduino Nano** (ATmega328P)
- **WS2812B** individually-addressable LED strip (data line on **pin 8**)
- **Shock circuit** triggered by a rising edge on **pin 3** (active HIGH)
- **Basler area-scan camera** (GigE or USB3, compatible with Pylon SDK)
- USB cable (Nano ↔ PC)

---

## Software Requirements

### Python (host PC)

```bash
pip install opencv-python pypylon pyserial
```

| Package | Purpose |
|---------|---------|
| `opencv-python` | Video display & recording |
| `pypylon` | Basler Pylon camera SDK bindings |
| `pyserial` | Serial communication with Arduino |

### Arduino (PlatformIO)

The project uses [PlatformIO](https://platformio.org/). Dependencies are declared in `platformio.ini` and installed automatically on build:

| Library | Version |
|---------|---------|
| FastLED | ^3.6.0 |

---

## Project Structure

```
VS Code Repo For e-Shock-LED/
├── Camera Interface              # Python script — camera + Arduino control
├── README.md                     # This file
└── Arduino Files for e-Shock/    # PlatformIO project
    ├── platformio.ini
    └── src/
        └── main.cpp              # Arduino Nano firmware
```

---

## Usage

### 1. Flash the Arduino

Open the `Arduino Files for e-Shock` folder in VS Code with PlatformIO, then:

```bash
pio run --target upload
```

Or use the PlatformIO sidebar **Upload** button.

### 2. Run the Python Interface

```bash
python "Camera Interface"
```

The script walks you through the following steps interactively:

1. **Select COM port** — pick the Arduino's serial port from a list.
2. **Configure settings** — enter values for:

   | Category | Parameters |
   |----------|------------|
   | **Camera** | Exposure time (µs), Frame rate (fps) |
   | **LEDs** | Number of LEDs, Brightness (0-255), Color (R, G, B), ON time (ms), OFF time (ms) |
   | **Shock** | ON time (ms), OFF time (ms), Number of cycles |

3. **Preview** — camera live-streams while LEDs blink on the strip. Shock events are **printed to the terminal only** (pin 3 stays LOW). Press **ESC** to exit preview.
4. **Confirm** — choose whether to keep settings or adjust.
5. **Record** — camera records to `~/Downloads/YMazeMP4/`. Arduino drives **real shocks on pin 3** and LEDs simultaneously. Stop with **Enter** or **ESC**, or let it run for the configured duration.

---

## Serial Protocol (115200 baud)

### Commands → Arduino

| Command | Description |
|---------|-------------|
| `CONFIG_LED:<n>,<bright>,<r>,<g>,<b>,<on_ms>,<off_ms>` | Configure LED strip parameters |
| `CONFIG_SHOCK:<on_ms>,<off_ms>,<cycles>` | Configure shock timing |
| `PREVIEW` | Start LEDs + simulated shocks (no pin 3 output) |
| `RECORD` | Start LEDs + real shocks (pin 3 driven HIGH/LOW) |
| `STOP` | Halt all outputs immediately |

### Responses ← Arduino

| Message | Meaning |
|---------|---------|
| `READY` | Firmware booted and ready |
| `OK` | Command accepted |
| `SHOCK_ON` | Shock pulse started (or simulated) |
| `SHOCK_OFF` | Shock pulse ended |
| `LED_ON` | LED pattern turned on |
| `LED_OFF` | LED pattern turned off |
| `SEQUENCE_DONE` | All shock cycles completed |
| `ERR:<msg>` | Error description |

---

## Defaults

| Parameter | Default |
|-----------|---------|
| Exposure time | 10 000 µs |
| Frame rate | 200 fps |
| Number of LEDs | 8 |
| LED brightness | 128 |
| LED color | Red (255, 0, 0) |
| LED ON / OFF | 500 ms / 500 ms |
| Shock ON / OFF | 200 ms / 800 ms |
| Shock cycles | 5 |
| Recording duration | 1 minute |

---

## Output

Recorded videos are saved to:

```
~/Downloads/YMazeMP4/basler_capture_YYYYMMDD_HHMMSS.mp4
```

---

## License

This project is part of internal lab tooling. See repository for licensing details.
