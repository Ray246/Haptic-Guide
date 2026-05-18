​# The Haptic Guide

> An STM32-based assistive navigation device that detects terrain drop-offs and alerts low-vision users through haptic feedback.

---

## What It Does

Traditional white canes are great for finding objects in front of you, but they often miss sudden drop-offs like curbs, stairs, or dips in the ground. The Haptic Guide solves that by pointing an ultrasonic sensor at the ground and constantly measuring the distance to the surface. If that distance suddenly jumps up — meaning the ground dropped away — the device immediately alerts the user through an LED and buzzer (with a vibration motor planned for the final version).

The user can also press a button at any time to recalibrate the device for a new surface or angle, making it adaptable to different environments.

---

## Demo

> 📷 *Add a photo or GIF of the prototype here*

---

## Hardware Components

| Component | Purpose |
|---|---|
| STM32F303K8 (Nucleo-32) | Main microcontroller |
| HC-SR04 Ultrasonic Sensor | Measures distance to ground |
| 1kΩ + 2kΩ Resistors | Voltage divider (5V → 3.3V on ECHO pin) |
| Pushbutton | Recalibrates floor baseline via EXTI interrupt |
| LED | Visual hazard indicator |
| Piezo Buzzer | Audible hazard alert (stand-in for vibration motor) |

---

## How It Works

1. **Sense** — The HC-SR04 fires an ultrasonic pulse and the STM32 measures how long the echo takes to return, converting that to a distance in centimeters.

2. **Calibrate** — On startup (or when the button is pressed), the system takes several readings and averages them to save a baseline floor distance. Outlier readings are automatically rejected.

3. **Detect** — Every new reading is compared to the baseline. If the difference (delta) exceeds 15 cm, the system classifies it as a potential drop-off.

4. **Alert** — The LED and buzzer fire immediately. All readings, the baseline, delta value, and status are printed over UART to a serial terminal for monitoring.

---

## Block Diagram

```
┌─────────────────┐        GPIO         ┌──────────────────────┐
│   HC-SR04       │ ──────────────────► │                      │
│ Ultrasonic      │ ◄────────────────── │   STM32F303K8        │──── LED (PA4)
│ Sensor          │  (voltage divider)  │                      │──── Buzzer (PA3)
└─────────────────┘                     │                      │──── UART Terminal
                                        │                      │
┌─────────────────┐    EXTI Interrupt   │                      │
│   Pushbutton    │ ──────────────────► │                      │
└─────────────────┘                     └──────────────────────┘
```

---

## UART Output Example

When running, the device prints live data to the serial terminal (115200 baud via USART2):

```
Calibrating baseline...
Calibration sample 1: 47 cm accepted
Calibration sample 2: 47 cm accepted
New Baseline Distance: 47 cm

Distance: 50 cm
Distance: 70 cm | Baseline: 47 cm | Delta: 23 cm
Status: NEGATIVE OBSTACLE / DROP DETECTED
ALERT: Significant delta-y drop detected
```

---

## Test Results

| Test | Goal | Result |
|---|---|---|
| Distance Accuracy | Read 50 cm ± 2 cm via UART | ✅ Pass |
| Negative Obstacle Detection | Detect 10 cm drop and trigger alert | ✅ Pass |
| Haptic Intensity (PWM) | Vary vibration motor with proximity | ⚠️ Partial — planned for v2 |
| User Calibration | Button press saves new baseline via EXTI | ✅ Pass |

---

## Planned Improvements

- [ ] Replace buzzer with a PWM-controlled coin vibration motor (via 2N2222 transistor)
- [ ] Use hardware timer input capture for more accurate distance measurement
- [ ] Add DMA + circular buffer for real-time noise filtering
- [ ] Add potentiometer to let the user adjust detection sensitivity
- [ ] Add a second HC-SR04 for forward-facing obstacle detection
- [ ] 3D-printed enclosure with cane attachment and battery power

---

## Built With

- STM32CubeIDE
- Embedded C (HAL Library)
- USART2 for serial output
- Mac terminal for monitoring

---

## Author

**Raymond Cortez** 

---

## License

This project is licensed under the MIT License. See [LICENSE](LICENSE) for details.
