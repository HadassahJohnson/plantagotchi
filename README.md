# Plantagotchi

Plantagotchi is an interactive, automated plant-watering system that uses simple game mechanics to encourage the user to take care of a real plant. It combines a soil moisture sensor, an automatic pump, an OLED display, and a memory game inspired by the original Tamagotchi. When the user plays and succeeds, the plant grows. When they ignore it, the plant declines.
This project uses an Arduino Uno and is fully self-contained.

---

# Features
## Core behavior
- Monitors soil moisture and shows a happy or sad plant depending on the reading.
- When the plant is sad, the user must play a short memory game to water it.
- Winning the game waters the plant and levels it up.
- Losing the game decreases the level by one.
- If the plant is sad for 24 hours without being watered, it resets to level 1.


## Plant growth and sprites
- Levels are represented by a growing plant animation on the OLED screen.
- Each level corresponds to a leaf.
- Level gains and losses have their own animations.
- The sad plant sprite also triggers a single beep when it first appears.

## User setup
- The user chooses a plant type (which sets the moisture threshold).
- The user then chooses a plant name from a list.
- All selections and progress are saved in EEPROM, so everything persists after power loss. 
## Game interaction
- A joystick is used for navigation and gameplay.
- The game shows an arrow pattern paired with tones; the user must repeat the sequence.
- Sequence length scales with the current level.


---
# Hardware
- Arduino Uno 
- SH1106 128x64 OLED display (I2C, uses U8g2 library)
- Soil moisture sensor (analog)
- Peristaltic pump (3.3–5V)
- PN2222 NPN transistor 
- Flyback diode (1N4007 or similar)
- Base resistor for the transistor (1k–10kΩ)
- Passive buzzer 
- Joystick (5-way)
- Breadboard and jumper wires 
- Arduino 5V power rail for low-current components 
- Pump powered through the power module

---
## Wiring Overview 

### Display (OLED, I2C)
- VCC → 5V
- GND → GND
- SDA → A4
- SCL → A5

### Joystick
- X-axis → A1
- Y-axis → A0
- Button → digital pin (configurable)
- VCC → 5V
- GND → GND
### Soil moisture sensor
- AOUT → A2
- VCC → 5V 
- GND → GND
### Pump driver (PN2222 + diode)
- Pump + → 5V 
- Pump – → Collector 
- Emitter → GND 
- Base → Arduino digital pin through resistor 
- Diode across pump terminals (stripe toward +)
### Buzzer 
- Signal → digital pin 
- GND → GND
---
## Software Overview
### Display and graphics
- Uses U8g2 for rendering sprites and animations.
- Happy and sad plant sprites stored in separate header files.
- Plant growth animation driven by indexed frames.
### Game logic
- Generates arrow sequences using random directions and tones.
- Input read from joystick directions.
- Sequence comparison determines success or failure.
### State persistence
- EEPROM stores:
  - Level
  - Leaf count
  - Plant type
  - Plant name index
  - Timestamp of when the plant became sad
---
## How It Works
1. The loop constantly checks soil moisture.
2. If moisture is above the threshold, the plant is happy.
3. If moisture falls below the threshold, the plant becomes sad.
4. The system records the time when sadness starts.
5. If the user plays and wins the game, the pump runs and the plant levels up.
6. If the user loses, their level decreases by one with a corresponding animation.
7. If 24 hours pass without watering while sad, the level resets to 1.
8. All values persist after reboot.

