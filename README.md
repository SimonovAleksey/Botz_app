# Robot Controller App

A macOS desktop application for real-time control of an Arduino ESP32-based robot over a local Wi-Fi network.

---

## What is this?

**Robot Controller App** is a native macOS app built with **Qt6 (C++)** that lets you wirelessly control a differential-drive robot powered by an **ESP32** microcontroller. The app connects to the robot over TCP and sends control commands in real time — no cables, no delays.

<img width="792" height="647" alt="Снимок экрана 2026-06-11 в 12 46 34" src="https://github.com/user-attachments/assets/210da359-41e1-4dae-8c6c-cf7b9b76aee2" />


## Features

- **Two control modes:**
  - **Tank mode** — control left and right tracks independently using sliders or keyboard (`W/S` and `↑/↓`)
  - **Steering mode** — throttle + angle control (`W/S` and `A/D`)
- **Real-time input** — sliders return to zero automatically on release, 50Hz command loop
- **Keyboard support** — full keyboard control with smooth acceleration
- **Light control** — toggle NeoPixel LED ring (`L`)
- **Extra / Relay** — toggle an onboard relay for any auxiliary device (`E`)
- **Connection panel** — app acts as TCP server, ESP32 connects to it automatically
- **Visual feedback** — green flash on connect, red flash on error, button glow animations

<img width="792" height="647" alt="Снимок экрана 2026-06-11 в 12 46 01" src="https://github.com/user-attachments/assets/1c3273bc-8768-4ca3-a767-a7037d500500" />
<img width="792" height="647" alt="Снимок экрана 2026-06-11 в 12 45 31" src="https://github.com/user-attachments/assets/bab0c9f9-6009-4dfd-97a5-d299011e1f3f" />





