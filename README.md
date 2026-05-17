
Smart-Lock-System-STM32-AI-Gesture-Recognition

A Two-Factor Authentication (2FA) access control system combining hardware security with computer vision. Built with an STM32F446RE microcontroller and a Python-based AI model.

## How It Works
1. **Motion Detection:** A PIR sensor wakes the system up.
2. **PIN Code:** The user enters a 4-digit PIN via a matrix keypad (feedback shown on an I2C OLED screen).
3. **AI Verification:** A Python script uses a Teachable Machine (Keras) model to scan the camera feed. The user must hold a specific hand gesture (open palm) for **3 seconds**.
4. **Access Granted:** The script sends a UART signal to the STM32, which generates a PWM signal to open the servo-driven lock for 10 seconds.

## Tech Stack
- **Hardware:** NUCLEO-F446RE, 0.96" OLED (SSD1306), 4x4 Keypad, SG90 Servo, PIR Sensor.
- **Firmware (C):** STM32CubeIDE, HAL Library, Finite State Machine (FSM), Hardware Interrupts (EXTI), I2C, PWM, UART.
- **Software (Python):** `OpenCV` (vision), `Keras` / `TensorFlow` (ML model), `pyserial` (communication).

## Installation
Clone this repository to your local machine:

```bash
git clone https://github.com/igokraj/Smart-Lock-System-STM32-Gesture-Recognition.git
cd Smart-Lock-System-STM32-AI-Gesture-Recognition
```

## Quick Start
1. **STM32:** Flash the C code from the `STM32_Code` folder onto your Nucleo-F446RE board.
2. **Python Setup:** Install dependencies:
   ```bash
   pip install tensorflow opencv-python pyserial numpy
   ```
3. **Run AI Script:** Navigate to the `Python_AI` folder, update the `PORT_COM` variable in `instructions.py`, and run:
   ```bash
   python instructions.py
   ```
