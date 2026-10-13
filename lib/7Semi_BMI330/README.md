# 7Semi BMI330 Arduino Library

Arduino library for the Bosch BMI330 6-axis IMU
(3-axis accelerometer + 3-axis gyroscope)

This library provides a clean and easy-to-use interface for:

- Acceleration (X, Y, Z)

- Gyroscope (X, Y, Z)

- Temperature

- I²C and SPI communication

---

##  Features

- Supports **I2C and SPI**
- Configurable:
  - accelerometer and gyroscope ranges
  - Output Data Rate (ODR)
  - Raw and scaled data access
  - FIFO
  - Interrupts (DRDY, FIFO, MOVING)
  - Temperature reading
- Optional **custom I2C / SPI pins on ESP32**

---

##  Connections / Wiring

###  I²C Connection (Most Common)

| BMI330 Pin | MCU Pin | Notes |
|-----------|--------|------|
| VDD | 3.3V | **3.3V only** (do NOT use 5V) |
| GND | GND | Common ground |
| SDI | SDA | I²C data |
| SCK | SCL | I²C clock |
| INT | GPIO (optional) | Required only for interrupts |

**I@C Notes**
- I2C address 0x68 and 0x69
- Dummy 2 byte before every read data (handled by library)  

---

###  SPI Connection

| BMI330 Pin | MCU Pin | Notes |
|-----------|--------|------|
| VDD | 3.3V | **3.3V only** |
| GND | GND | Common ground |
| SCK | SPI SCK | Clock |
| SDI | SPI MOSI | Master → sensor |
| SDO | SPI MISO | Sensor → master |
| CS | GPIO | Chip Select (active-low) |
| INT | GPIO (optional) | Interrupt output |

**SPI Notes**
- SPI mode **MODE0** required  
- Dummy 1 byte before every read data (handled by library)  
- CS must stay LOW during the entire transaction  

Notes

SPI mode: MODE0

Start with 1 MHz SPI clock

Keep SPI wires short for reliability

##  Installation

### Arduino Library Manager
1. Open **Arduino IDE**
2. Go to **Library Manager**
3. Search for **7Semi BMI330**
4. Install

### Manual
1. Download this repository as ZIP
2. Arduino IDE → **Sketch → Include Library → Add .ZIP Library**

---

### Initialization

- begin() – Initializes BMI330 over default I²C (0x69) and verifies the chip ID.

- beginI2C() – Initializes BMI330 over I²C with custom address and verifies the chip ID.

- beginSPI() – Initializes BMI330 over SPI (MODE0) and verifies the chip ID.

- reset() – Performs a soft reset and restores default register values.

- chipID() – Reads and returns the BMI330 chip ID.

### Configuration

- setAccelRange() – Sets accelerometer measurement range (±2g / ±4g / ±8g / ±16g).

- setGyroRange() – Sets gyroscope measurement range (±125 / ±250 / ±500 / ±1000 / ±2000 dps).

- setAccelODR() – Sets accelerometer Output Data Rate.

- setGyroODR() – Sets gyroscope Output Data Rate.

### Data Reading

- readAcceleration() – Reads acceleration values in g (X, Y, Z).

- readGyroscope() – Reads angular rate values in dps (X, Y, Z).

- readTemperature() – Reads internal temperature in °C.

- readRawAccel() – Reads raw accelerometer register values.

- readRawGyro() – Reads raw gyroscope register values.

### FIFO

- configureFIFO() – Configures FIFO frame type and mode.

- getFIFOCount() – Returns the number of frames currently stored in FIFO.

- readFIFOFrame() – Reads one FIFO frame (Accel / Gyro / Both).

- clearFIFO() – Flushes all FIFO data.

### Interrupts

- configureIntPin() – Configures interrupt pin behavior (latched, polarity, push-pull/open-drain).

- setIntSources() – Enables specific interrupt sources (DRDY, FIFO, motion).

- readIntStatus() – Reads and clears interrupt status flags.

#### Motion / Detection Features

- enableStepCounter() – Enables internal step counter.

- readStepCount() – Reads current step count value.

- enableMotionInterrupt() – Enables motion detection interrupt.

- enableNoMotionInterrupt() – Enables no-motion detection interrupt.

---

### Accelerometer Ranges

- ±2g

- ±4g

- ±8g

- ±16g

---

### Gyroscope Ranges

- ±125 dps

- ±250 dps

- ±500 dps

- ±1000 dps

- ±2000 dps

---

### Example Use Cases

- Motion tracking

- Robotics

- Drone stabilization

- Gesture detection

- Tilt sensing

- Vibration monitoring

---

## ⚡ Quick Start (I2C)

```cpp
#include <Wire.h>
#include <7Semi_BMI330.h>

BMI330_7Semi imu;

void setup() {
  Serial.begin(115200);
  Wire.begin();

  if (!imu.begin()) {
    Serial.println("BMI330 not detected!");
    while (1);
  }

  Serial.println("BMI330 initialized successfully");
}

void loop() {
  float ax, ay, az;
  float gx, gy, gz;

  imu.readAcceleration(ax, ay, az);
  imu.readGyroscope(gx, gy, gz);

  Serial.print("Accel (g): ");
  Serial.print(ax); Serial.print(", ");
  Serial.print(ay); Serial.print(", ");
  Serial.println(az);

  Serial.print("Gyro (dps): ");
  Serial.print(gx); Serial.print(", ");
  Serial.print(gy); Serial.print(", ");
  Serial.println(gz);

  delay(500);
}
```
---

## ⚡ Quick Start (SPI)

```cpp
#include <SPI.h>
#include <7Semi_BMI330.h>

#define BMI330_CS 5

BMI330_7Semi imu;

void setup() {
  Serial.begin(115200);
  SPI.begin();

  if (!imu.beginSPI(BMI330_CS)) {
    Serial.println("BMI330 not detected!");
    while (1);
  }

  Serial.println("BMI330 initialized successfully");
}

void loop() {
  float ax, ay, az;
  float gx, gy, gz;

  imu.readAcceleration(ax, ay, az);
  imu.readGyroscope(gx, gy, gz);

  Serial.print("Accel (g): ");
  Serial.print(ax); Serial.print(", ");
  Serial.print(ay); Serial.print(", ");
  Serial.println(az);

  delay(500);
}
```
