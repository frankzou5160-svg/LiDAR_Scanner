# STM32 LiDAR Scanner

A 360° LiDAR room scanner built with an STM32 NUCLEO-L476RG, TFMini-S ToF sensor, and 28BYJ-48 stepper motor. The scanner rotates the sensor via a belt drive system and transmits distance data over UART to a Python visualizer that renders a live 2D map.

---

## Hardware

| Component | Details |
|---|---|
| Microcontroller | STM32 NUCLEO-L476RG |
| Distance Sensor | TFMini-S (UART, ToF) |
| Stepper Motor | 28BYJ-48 with ULN2003 driver |
| Display | SSD1306 0.96" OLED (I2C) |
| Rotation Platform | Lazy Susan bearing (72mm main cylinder) |
| Drive System | Belt drive — 25mm hub on motor drives 72mm main cylinder |

**Wiring:**

| Peripheral | Pin |
|---|---|
| Stepper IN1 | PA0 |
| Stepper IN2 | PA1 |
| Stepper IN3 | PA4 |
| Stepper IN4 | PB0 |
| TFMini-S TX | USART1 RX |
| TFMini-S RX | USART1 TX |
| OLED SDA | I2C1 SDA |
| OLED SCL | I2C1 SCL |
| PC UART | USART2 (via ST-Link) |

---

## How It Works

- The STM32 drives the 28BYJ-48 stepper motor in half-step mode (4096 steps/rev)
- The motor hub (25mm) drives the main platform (72mm) via a rubber band belt — giving a ~2.88:1 ratio
- `STEPS_PER_REV` is tuned to 14155 to match the actual full rotation time of the main platform
- Every 8th step the TFMini-S is read via UART at 115200 baud, giving ~512 readings per sweep
- Each reading is validated (signal strength, range check) and filtered with a 5-sample rolling average
- Data is transmitted over USART2 in the format `A<angle>,<distance>` with `A360` marking sweep end
- The SSD1306 OLED displays the current angle and last valid distance reading

---

## Tools & Dependencies

**Firmware:**
- STM32CubeIDE
- HAL drivers (auto-generated via CubeMX — use the `.ioc` file to regenerate)

**Python visualizer:**
```
pip install pyserial matplotlib numpy
```

---

## How To Run

**1. Flash the firmware**
- Open STM32CubeIDE
- Import the project
- Build and flash to the NUCLEO board

**2. Run the Python visualizer**
- Find your COM port in Device Manager
- Edit `PyStuff/Plot.py` and set your COM port:
```python
PORT = 'COM3'  # change to your port
```
- Run the script:
```
python PyStuff/Plot.py
```

**3. Power on the scanner**
- The OLED will display `SCANNER READY...` then begin scanning
- The Python window will show a live polar sweep and a completed sweep map

---

## Demo

- Left panel — live polar plot of the current sweep building in real time
- Right panel — completed 2D Cartesian map of the last full sweep with distance rings
- Invalid readings (out of range, weak signal) are transmitted as distance `0` and skipped by the visualizer
[![Watch Test_Box]([https://youtube.com](https://drive.google.com/file/d/1pvvdbFVxaCZRf6Azi6ZIid-OlKYKUEdy/view?usp=sharing))]([https://youtube.com](https://drive.google.com/file/d/1pvvdbFVxaCZRf6Azi6ZIid-OlKYKUEdy/view?usp=sharing))
[![Watch Test_Can_Board](https://drive.google.com/file/d/1UlSf4U0Xcj4iSGjaJmbnd8ghX0Z0wEgw/view?usp=sharing)](https://drive.google.com/file/d/1UlSf4U0Xcj4iSGjaJmbnd8ghX0Z0wEgw/view?usp=sharing)


---

## Project Structure

```
├── Core/
│   ├── Inc/          # Header files
│   ├── Src/          # Source files (main.c, ssd1306 driver)
│   └── Startup/      # Startup assembly
├── PyStuff/
│   └── Plot.py       # Python visualizer
├── STM32L476RGTX_FLASH.ld
├── STM32L476RGTX_RAM.ld
├── NUCLEO_L476RG_LiDAR_Scanner.ioc
└── README.md
```
