# Solilux

## Introduction
Solilux is an Arduino Bluetooth automation system that controls blinds, lights, and other connected devices. Built from scratch, it combines **hardware and software** for real-time control, automation, and sensor-based adjustments. The system is programmed in C++ (Arduino) and can be controlled via a mobile app.

## ⚙️ Key Features
- **Bluetooth Control** – Operate blinds, lights, and more via a **mobile app**.
- **Physical Controls** – Manual operation using the button panel.
- **Sound & Light Control** – Adjust devices using **sound** input and **light sensors**.
- **Preset Configurations** – Save and load custom settings.
- **Automation & Scheduling** – Set **timers** to trigger actions at specific times.
- **Calibration** – Adjust servo speed, range of motion, and sensor thresholds via the app.

## ⚡ How It Works
The system allows multiple control methods:
- **Bluetooth Commands** – Mobile app sends formatted commands for specific actions.
- **Button Panel** – Manual operation for toggling lights, blinds, presets etc.
- **Microphone Input** – Sound-based control of components.
- **Light Sensor** – Adjusts lights and blinds automatically based on ambient brightness.
- **Timers & Scheduling** – Automates actions based on set time intervals.
- **Stepper Motor** – Lifts and lowers blinds within configurable range.
- **Servos** – Adjusts the tilt of blinds and controls lights with configurable speed and range of motion.

## ▶️ Demonstration
| Mobile App Interface | Blinds and light control |
|----------------------------------|-----------------------------------|
| ![UI Mobile App](assets/ui_demo.gif) | ![Blinds in Action](assets/demo.gif) |

## 🧩 Technical Challenges
- **Power Management** – Ensuring stable power delivery to stepper motors, servos, and sensors took some fine-tuning —and a few exploding capacitors— to get it right.
- **Hardware irregularities** - Mechanical buttons sometimes register multiple clicks instead of one, despite software debouncing. The light sensor overreacts to sudden changes (like reflections), and the microphone sometimes picks up the wrong sounds. Filtering and compensating for these problems was more challenging than expected.
- **Mechanical Design** – Custom stepper motor spool mechanism for lifting blinds, plus a worm-wheel locking system to hold blinds when unpowered.
- **Data Formatting** – Bluetooth commands had to be structured for accurate hardware interpretation.
- **Calibration Complexity** – Ensuring smooth servo and stepper motor operation required a lot of fine-tuning.
- **Code Complexity** – Managing multiple inputs (Bluetooth, buttons, sensors, timers) without conflicts took way more debugging than I expected.


## 🔩 Hardware
- **Microcontroller**: Arduino Mega 2560.
- **Stepper Motor**: Nema 17 for blinds lift, with worm-wheel lock.
- **Servo Motor**: 25kg motor controls blinds tilt and lights.
- **RTC**: DS3231 real-time clock for scheduling.
- **Microphone**: Simple microphone triggers actions based on sound.
- **LDR Sensor**: LDR (light-dependent resistor) detects ambient/outisde light levels.
- **Button Panel**: Physical toggles for manual control.
- **Buzzer & LEDs**: Provide user feedback on actions.
