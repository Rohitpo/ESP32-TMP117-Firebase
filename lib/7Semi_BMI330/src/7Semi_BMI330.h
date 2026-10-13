#pragma once
#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include "7Semi_BMI330_defs.h"

/**
 * 7Semi BMI330 driver (no official Bosch library)
 *
 * - Works with I²C (and supports SPI if you wire CS + provide SPI bus)
 * - Uses BMI330 16-bit register words and required dummy bytes on read (datasheet)
 * - Provides: accel, gyro, temperature, sensortime, FIFO, interrupts, and feature engine control
 *
 * Notes
 * - The advanced features (step, tap, any/no motion, orientation, flat, tilt, sig-motion, etc.)
 *   live in the "feature engine". You must enable it first, then enable individual features via FEATURE_IO0.
 * - Feature tuning parameters are in the "EXT" configuration space accessed by FEATURE_DATA_ADDR/TX.
 *   This library exposes generic read/write helpers so you can implement/tune any feature without vendor code.
 */

#include <stdint.h>

class BMI330_7Semi
{
public:
  /** Communication type selection
   *  - NONE: bus not selected yet (begin() will pick default)
   *  - I2C : use TwoWire transport
   *  - SPI : use SPIClass transport
   *
   * Notes:
   *  - Call beginI2C() or beginSPI() to select a bus explicitly.
   *  - If Bus::NONE, begin() should default to I2C with default address.
   */
  enum class Bus : uint8_t
  {
    NONE = 0,
    I2C = 1,
    SPI = 2
  };

  /** Output Data Rate codes (ACC_CONF.acc_odr / GYR_CONF.gyr_odr)
   *  - These values are written into the 4-bit ODR field in ACC_CONF / GYR_CONF.
   *  - Choose ODR based on your required sample rate and power budget.
   *
   * Notes:
   *  - Higher ODR increases bandwidth and power consumption.
   *  - Make sure your host polling/interrupt rate can keep up with the chosen ODR.
   */
  enum ODR : uint8_t
  {
    ODR_0_78125HZ = 0x01,
    ODR_1_5625HZ = 0x02,
    ODR_3_125HZ = 0x03,
    ODR_6_25HZ = 0x04,
    ODR_12_5HZ = 0x05,
    ODR_25HZ = 0x06,
    ODR_50HZ = 0x07,
    ODR_100HZ = 0x08,
    ODR_200HZ = 0x09,
    ODR_400HZ = 0x0A,
    ODR_800HZ = 0x0B,
    ODR_1_6KHZ = 0x0C,
    ODR_3_2KHZ = 0x0D,
    ODR_6_4KHZ = 0x0E
  };

  /** Accelerometer full-scale range (ACC_CONF.acc_range)
   *  - These values are written into the 3-bit range field in ACC_CONF.
   *
   * Practical guidance:
   *  - Use 2G for best resolution when motion is small.
   *  - Use 16G if you expect high shock/impacts.
   *
   * Scaling:
   *  - The comments show the typical LSB/mg values used for conversion helpers.
   *  - Your readAccel() uses these to convert raw to g.
   */
  enum AccRange : uint8_t
  {
    ACC_RANGE_2G = 0x0, /* 16.38 LSB/mg */
    ACC_RANGE_4G = 0x1, /* 8.19  LSB/mg */
    ACC_RANGE_8G = 0x2, /* 4.10  LSB/mg */
    ACC_RANGE_16G = 0x3 /* 2.05  LSB/mg */
  };

  /** Gyroscope full-scale range (GYR_CONF.gyr_range)
   *  - These values are written into the 3-bit range field in GYR_CONF.
   *
   * Practical guidance:
   *  - Use 125/250 dps for slow motion and best resolution.
   *  - Use 2000 dps for fast rotation (sports, drones, robotics).
   *
   * Scaling:
   *  - The comments show the typical LSB/°/s values used for conversion helpers.
   *  - Your readGyro() uses these to convert raw to dps.
   */
  enum GyrRange : uint8_t
  {
    GYR_RANGE_125DPS = 0x0,  /* 262.144 LSB/°/s */
    GYR_RANGE_250DPS = 0x1,  /* 131.072 LSB/°/s */
    GYR_RANGE_500DPS = 0x2,  /* 65.536  LSB/°/s */
    GYR_RANGE_1000DPS = 0x3, /* 32.768  LSB/°/s */
    GYR_RANGE_2000DPS = 0x4  /* 16.4    LSB/°/s */
  };

  /** Bandwidth selection (ACC_CONF.acc_bw / GYR_CONF.gyr_bw)
   *  - This selects the internal filter bandwidth relative to ODR.
   *
   * Practical guidance:
   *  - DIV2: higher bandwidth (less filtering, more noise, more responsive)
   *  - DIV4: lower bandwidth  (more filtering, less noise, more latency)
   */
  enum BW : uint8_t
  {
    BW_ODR_DIV2 = 0x0,
    BW_ODR_DIV4 = 0x1
  };

  /** Averaging selection (ACC_CONF.acc_avg_num / GYR_CONF.gyr_avg_num)
   *  - Controls internal sample averaging.
   *
   * Practical guidance:
   *  - AVG_OFF: lowest latency, highest noise
   *  - Higher AVG reduces noise but increases latency and may reduce effective bandwidth.
   */
  enum AVG : uint8_t
  {
    AVG_OFF = 0x0,
    AVG_2 = 0x1,
    AVG_4 = 0x2,
    AVG_8 = 0x3,
    AVG_16 = 0x4,
    AVG_32 = 0x5,
    AVG_64 = 0x6
  };

  /** Sensor operation mode (ACC_CONF.acc_mode / GYR_CONF.gyr_mode)
   *  - Controls power/performance behavior for the sensor path.
   *
   * Practical guidance:
   *  - MODE_DISABLE           : disables the sensor output path
   *  - MODE_DUTY_CYCLE        : reduced power, intermittent sampling
   *  - MODE_CONTINUOUS_LP     : continuous low power
   *  - MODE_HIGH_PERFORMANCE  : best noise / best stability (highest current)
   *
   * Notes:
   *  - MODE_GYR_DRIVE_ONLY is typically used in special gyro-driven configurations.
   *  - Not all combinations of ODR/BW/AVG/mode are valid; choose conservative combos first.
   */
  enum Mode : uint8_t
  {
    MODE_DISABLE = 0x0,
    MODE_GYR_DRIVE_ONLY = 0x1, /* gyro only */
    MODE_DUTY_CYCLE = 0x3,
    MODE_CONTINUOUS_LP = 0x4,
    MODE_HIGH_PERFORMANCE = 0x7
  };

  /** Interrupt route selection for INT_MAP1 / INT_MAP2 (2-bit fields)
   *  - These are the 2-bit codes written into each mapping field.
   *
   * Values:
   *  - INT_DISABLED : interrupt output disabled
   *  - INT_TO_INT1  : route event to INT1 pin
   *  - INT_TO_INT2  : route event to INT2 pin
   *  - INT_TO_IBI   : route event to I3C In-Band Interrupt (I3C only)
   *
   * Notes:
   *  - For pure I2C/SPI designs, INT_TO_IBI is not usable.
   */
  enum IntRoute : uint8_t
  {
    INT_DISABLED = 0x0,
    INT_TO_INT1 = 0x1,
    INT_TO_INT2 = 0x2,
    INT_TO_IBI = 0x3
  };

  /** FIFO configuration container
   *  - Used by setFifoConfig() to enable/disable FIFO sources.
   *
   * Fields:
   *  - enable_accel : include accelerometer XYZ frames
   *  - enable_gyro  : include gyroscope XYZ frames
   *  - enable_temp  : include temperature field
   *  - enable_time  : include sensor time field
   *  - stop_on_full : true = stop writing when full, false = overwrite oldest
   *
   * Practical guidance:
   *  - For streaming applications, keep stop_on_full=false and read FIFO regularly.
   *  - For triggered capture, stop_on_full=true can freeze data when full.
   */
  struct FifoConfig
  {
    bool enable_accel = false;
    bool enable_gyro = false;
    bool enable_temp = false;
    bool enable_time = false;
    bool stop_on_full = false;
  };

  /** Decoded sensor sample
   *  - Filled by decodeFifoFrame() or read*() helper functions.
   *
   * Raw fields:
   *  - ax/ay/az and gx/gy/gz are raw signed output codes.
   *
   * Converted fields:
   *  - ax_g/ay_g/az_g are in g
   *  - gx_dps/gy_dps/gz_dps are in degrees per second
   *
   * Flags:
   *  - has_* tells you which parts were present in the decoded frame.
   */
  struct Sample
  {
    int16_t ax = 0, ay = 0, az = 0;
    float ax_g = 0, ay_g = 0, az_g = 0;

    int16_t gx = 0, gy = 0, gz = 0;
    float gx_dps = 0, gy_dps = 0, gz_dps = 0;

    int16_t t = 0;
    uint32_t sensor_time = 0;

    bool has_accel = false;
    bool has_gyro = false;
    bool has_temp = false;
    bool has_time = false;
  };

  /** Feature bit-index values for FEATURE_IO0
   *  - Each value is the BIT POSITION (0..15), not a mask.
   *
   * How to use:
   *  - setFeature(BMI330Feature feature, bool enable) expects a bit index.
   *  - Example:
   *    - setFeature(STEP_COUNTER, true)  -> sets FEATURE_IO0 bit 9
   *
   * Notes:
   *  - Prefer this enum for low-memory targets, because you store only the index.
   */
  enum BMI330Feature : uint16_t
  {
    NO_MOTION_X = 0,
    NO_MOTION_Y = 1,
    NO_MOTION_Z = 2,

    ANY_MOTION_X = 3,
    ANY_MOTION_Y = 4,
    ANY_MOTION_Z = 5,

    FLAT = 6,
    ORIENTATION = 7,

    STEP_DETECTOR = 8,
    STEP_COUNTER = 9,

    SIG_MOTION = 10,
    TILT = 11,

    TAP_S = 12,
    TAP_D = 13,
    TAP_T = 14,

    I3C_SYNC = 15
  };

  /** Feature interrupt output mapping (2-bit field values)
   *  - Used with INT_MAP1 feature outputs (no-motion, any-motion, flat, etc.).
   *
   * Values:
   *  - FEAT_INT_DISABLE : 0b00 -> interrupt disabled
   *  - FEAT_INT_INT1    : 0b01 -> map event to INT1 pin
   *  - FEAT_INT_INT2    : 0b10 -> map event to INT2 pin
   *  - FEAT_INT_IBI     : 0b11 -> map event to I3C IBI (I3C only)
   *
   * Notes:
   *  - For I2C/SPI systems, use INT1/INT2 mapping only.
   */
  enum FeatureIntOut : uint8_t
  {
    FEAT_INT_DISABLE = 0x0,
    FEAT_INT_INT1 = 0x1,
    FEAT_INT_INT2 = 0x2,
    FEAT_INT_IBI = 0x3
  };

public:
  BMI330_7Semi();

  /**
   * - Initialize BMI330 using the selected bus (I2C or SPI).
   * - If no bus is selected, it defaults to I2C with default parameters.
   * - Performs CHIP_ID check (lower byte), soft reset, and applies a safe default configuration.
   *
   * Notes:
   * - Default config should be stable for most use:
   *   - Accel: 100 Hz, ±2 g, BW=ODR/2, AVG=64, High Performance
   *   - Gyro : 100 Hz, ±2000 dps, BW=ODR/2, AVG=64, High Performance
   *
   * Returns:
   * - true  : device detected and configured successfully
   * - false : bus init failed, CHIP_ID mismatch, reset failed, or configuration failed
   */
  bool begin();

  /**
   * - Select and initialize BMI330 on I2C, then run full bring-up (begin()).
   * - Sets internal driver state (bus type, I2C address, Wire port, I2C clock).
   * - Starts the I2C peripheral and verifies the device ACKs the given address.
   *
   * Parameters:
   * - address : 7-bit I2C address
   *   - Common BMI330 addresses are 0x68 (SDO=GND) or 0x69 (SDO=VDDIO).
   * - wire    : TwoWire instance (Wire, Wire1, etc.)
   * - clock   : I2C clock in Hz (typical: 100000 or 400000)
   * - sda/scl : ESP32/ESP8266 pin selection
   *   - Use 255 to keep the platform default pins (ignored on many non-ESP boards).
   *
   * Notes:
   * - This function is expected to call begin() internally after confirming I2C ACK.
   * - If you want “only bus init without configuring the sensor”, do not call begin().
   *
   * Returns:
   * - true  : I2C started, device ACKed, and begin() completed successfully
   * - false : device did not ACK, bus init failed, or begin() failed
   */
  bool beginI2C(uint8_t address = 0x69,
                TwoWire &wire = Wire,
                uint32_t clock = 400000,
                uint8_t sda = 255,
                uint8_t scl = 255);

  /**
   * - Select and initialize BMI330 on SPI, then run full bring-up (begin()).
   * - Sets internal driver state (bus type, CS pin, SPI port, SPI clock).
   * - Configures CS pin and starts the SPI peripheral.
   *
   * Parameters:
   * - csPin   : chip select pin (must be a valid GPIO)
   * - spiPort : SPI instance (SPI, SPI1, etc.)
   * - clock   : SPI clock in Hz
   *   - Keep conservative for long wires or noisy environments.
   * - sck/miso/mosi : ESP32 pin selection
   *   - Use 255 to keep the platform default pins (ignored on many non-ESP boards).
   *
   * Notes:
   * - If you see occasional SPI bring-up failures, do one “dummy” read after power-up
   *   (throw-away readChipID) before validating CHIP_ID in begin().
   * - Keep CS HIGH when idle. Always toggle CS LOW/HIGH around every transfer.
   *
   * Returns:
   * - true  : SPI started and begin() completed successfully
   * - false : begin() failed (CHIP_ID mismatch, reset/config failure, etc.)
   */
  bool beginSPI(uint8_t csPin,
                SPIClass &spiPort = SPI,
                uint32_t clock = 10000000,
                uint8_t sck = 255,
                uint8_t miso = 255,
                uint8_t mosi = 255);

  /**
   * - Low-level helpers for register access.
   * - These functions do not interpret data; they only move bytes over I2C/SPI.
   * - All higher-level configuration should be built on top of these primitives.
   *
   * Notes:
   * - BMI330 register pairs are usually little-endian (LSB then MSB).
   * - For I2C reads, BMI330 returns dummy bytes before valid payload (handled internally).
   * - For SPI reads, a dummy byte is inserted after the address phase.
   */
  /**
   * - Write a 16-bit value to consecutive registers (LSB then MSB).
   * - Uses the active bus (I2C/SPI).
   */
  bool writeReg(uint8_t reg, uint16_t value);

  /**
   * - Read a 16-bit value from consecutive registers (LSB then MSB).
   * - Uses the active bus (I2C/SPI).
   */
  bool readReg(uint8_t reg, uint16_t &value);

  /**
   * - Read 'length' bytes starting from 'startReg' into 'buffer'.
   * - Uses the active bus (I2C/SPI).
   */
  bool readBurst(uint8_t startReg, uint8_t *buffer, size_t length);

  /**
   * - Send a soft reset command to BMI330.
   * - Only reports whether the command write completed on the bus.
   *
   * Notes:
   * - Always delay after reset before configuration or reads.
   * - Recommended: delay(10) and then confirm CHIP_ID.
   */
  bool softReset();

  /**
   * - Read CHIP_ID register word.
   * - Used to validate sensor presence during begin().
   *
   * Notes:
   * - Compare only the lower byte for ID check: (chipId & 0xFF).
   */
  bool readChipID(uint16_t &chipId);

  /**
   * - Read STATUS register word.
   * - Used to check internal flags / device state.
   */
  bool readStatus(uint16_t &status);

  /**
   * - Read ERR_REG register word.
   * - Used to detect errors in configuration or internal operations.
   */
  bool readError(uint16_t &error);

  /**
   * - Read one bit from a 16-bit register.
   * - Use this for status/error flags and “simple boolean” config bits.
   *
   * Notes:
   * - bitPos must be 0..15.
   */
  bool readBit(uint8_t reg, uint8_t bitPos, bool &bitValue);

  /**
   * - Write one bit inside a 16-bit register (read-modify-write).
   * - Use this for enable/disable features that are controlled by a single bit.
   *
   * Notes:
   * - Not interrupt-safe by default (read-modify-write can collide with other writers).
   * - bitPos must be 0..15.
   */
  bool writeBit(uint8_t reg, uint8_t bitPos, bool bitVal);

  /**
   * - Configure accelerometer ODR, range, bandwidth, averaging and mode.
   * - Updates internal accel_range for correct scaling in engineering units.
   *
   * Notes:
   * - Uses read-modify-write to preserve unrelated bits in the config register.
   * - Enums should match hardware field encodings.
   */
  bool setAccelConfig(ODR odr, AccRange range, BW bw, AVG avg, Mode mode);

  /**
   * - Configure gyroscope ODR, range, bandwidth, averaging and mode.
   * - Updates internal gyro_range for correct scaling in engineering units.
   *
   * Notes:
   * - Uses read-modify-write to preserve unrelated bits in the config register.
   */
  bool setGyroConfig(ODR odr, GyrRange range, BW bw, AVG avg, Mode mode);

  /**
   * - Read accelerometer samples converted to g.
   *
   * Notes:
   * - Uses accel_range to convert raw values to g.
   * - For best accuracy, ensure setAccelConfig() was called first.
   */
  bool readAccel(float &ax_g, float &ay_g, float &az_g);

  /**
   * - Read gyroscope samples converted to degrees/second.
   *
   * Notes:
   * - Uses gyro_range to convert raw values to dps.
   * - For best accuracy, ensure setGyroConfig() was called first.
   */
  bool readGyro(float &gx_dps, float &gy_dps, float &gz_dps);

  /**
   * - Read temperature converted to °C.
   *
   * Notes:
   * - Uses the standard BMI330 temperature scaling:
   *   - tempC = (raw / 256) + 23
   * - If you need custom scaling, use readTemperatureRaw().
   */
  bool readTemperatureC(float &tempC);

  /**
   * - Convert sensor time ticks to microseconds using 39.0625 us per tick.
   * - Uses integer math to avoid float:
   *   - us ≈ (ticks * 390625) / 10000
   */
  uint32_t sensorTimeToUs(uint32_t ticks);

  /**
   * - Read raw accelerometer output as signed 16-bit values.
   * - Values are in sensor LSB units and must be scaled using accel_range.
   */
  bool readAccelRaw(int16_t &ax, int16_t &ay, int16_t &az);

  /**
   * - Read raw gyroscope output as signed 16-bit values.
   * - Values are in sensor LSB units and must be scaled using gyro_range.
   */
  bool readGyroRaw(int16_t &gx, int16_t &gy, int16_t &gz);

  /**
   * - Read raw temperature output.
   * - Convert to °C using: (raw / 256) + 23
   */
  bool readTemperatureRaw(int16_t &t);

  /**
   * - Read the internal sensor time counter (32-bit tick counter).
   *
   * Notes:
   * - 1 tick = 39.0625 us.
   * - Value is returned as raw ticks.
   */
  bool readSensorTime(uint32_t &time32);

  /**
   * - Configure FIFO enable flags and behavior (stop-on-full, time/accel/gyro/temp).
   */
  bool setFifoConfig(const FifoConfig &cfg);

  /**
   * - Set FIFO watermark level.
   */
  bool setFifoWatermark(uint16_t watermark);

  /**
   * - Read FIFO fill level.
   */
  bool getFifoFillLevel(uint16_t &data);

  /**
   * - Flush FIFO contents (must use documented BMI330 flush mechanism).
   */
  bool flushFifo();

  /**
   * - Read FIFO data into a uint16_t buffer.
   *
   * Notes:
   * - Reads bytes and assembles data explicitly for portability.
   */
  bool readFifo(uint16_t *dst, size_t fifoCount);

  /**
   * Decode one FIFO frame based on current FIFO_CONF enable bits.
   *
   * - BMI330 FIFO frames have no header and use a fixed order:
   *   Accelerometer(3) -> Gyroscope(3) -> Temperature(1) -> Sensortime(1). (datasheet table: FIFO order)
   * - You must know which sources are enabled to parse correctly.
   */
  bool decodeFifoFrame(const uint16_t *dataFrame, size_t dataFrameCount, Sample &out);

  /**
   * - Read SAT_FLAGS raw word.
   */
  bool readSaturationFlagReg(uint16_t &flags);

  /**
   * - Check if accel axis is saturated.
   * - axis: 0=X, 1=Y, 2=Z
   */
  bool isAccelSaturated(uint8_t axis, bool &saturated);

  /**
   * - Check if gyro axis is saturated.
   * - axis: 0=X, 1=Y, 2=Z
   */
  bool isGyroSaturated(uint8_t axis, bool &saturated);

  /**
   * - Configure INT1 pin electrical behavior and enable/disable the output driver.
   *
   * Parameters:
   * - enable:
   *   - true  = INT1 driver enabled
   *   - false = INT1 output disabled
   *
   * - activeHigh:
   *   - true  = interrupt is active HIGH
   *   - false = interrupt is active LOW
   *
   * - openDrain:
   *   - true  = open-drain output (requires external pull-up)
   *   - false = push-pull output
   *
   * Notes:
   * - This only configures the physical pin behavior.
   * - Actual interrupt sources must be mapped using INT_MAP registers.
   *
   * Returns:
   * - true  : configuration applied successfully
   * - false : register access failed
   */
  bool configInt1Pin(bool enable, bool activeHigh, bool openDrain);

  /**
   * - Configure INT2 pin electrical behavior and enable/disable the output driver.
   *
   * Parameters:
   * - enable:
   *   - true  = INT2 driver enabled
   *   - false = INT2 output disabled
   *
   * - activeHigh:
   *   - true  = interrupt is active HIGH
   *   - false = interrupt is active LOW
   *
   * - openDrain:
   *   - true  = open-drain output (requires external pull-up)
   *   - false = push-pull output
   *
   * Notes:
   * - This only configures the physical pin behavior.
   * - Interrupt routing must be configured separately.
   *
   * Returns:
   * - true  : configuration applied successfully
   * - false : register access failed
   */
  bool configInt2Pin(bool enable, bool activeHigh, bool openDrain);

  /**
   * - Configure interrupt latching behavior.
   *
   * Parameters:
   * - latched:
   *   - true  = interrupt remains asserted until status register is read/cleared
   *   - false = interrupt behaves as level/pulse according to device configuration
   *
   * Notes:
   * - Latching applies to mapped interrupt outputs.
   * - Use readIntStatusInt1()/readIntStatusInt2() to clear latched interrupts.
   *
   * Returns:
   * - true  : latch configuration updated
   * - false : register access failed
   */
  bool configIntLatch(bool latched);

  /**
   * - Map basic interrupt sources to interrupt outputs using INT_MAP2.
   *
   * Notes:
   * - Uses a single read-modify-write for speed (no repeated writeBit calls).
   * - IntRoute is treated as a 2-bit field (0..3).
   */
  bool mapBasicInterrupts(IntRoute fifoFull,
                          IntRoute fifoWm,
                          IntRoute accelDrdy,
                          IntRoute gyroDrdy,
                          IntRoute tempDrdy,
                          IntRoute errStatus,
                          IntRoute tapOut);

  /**
   * - Write the full INT_MAP1 register (0x3A) in one call.
   * - INT_MAP1 controls routing of feature-engine interrupts to INT1 / INT2 / I3C IBI.
   *
   * Route encoding (for every 2-bit field):
   * - 0b00 = interrupt disabled
   * - 0b01 = mapped to INT1
   * - 0b10 = mapped to INT2
   * - 0b11 = mapped to I3C IBI
   *
   * INT_MAP1 layout (each field is 2 bits):
   * - bits [1:0]   no_motion_out
   *   - Routes "no motion" feature output interrupt.
   *
   * - bits [3:2]   any_motion_out
   *   - Routes "any motion" feature output interrupt.
   *
   * - bits [5:4]   flat_out
   *   - Routes "flat" feature output interrupt.
   *
   * - bits [7:6]   orientation_out
   *   - Routes "orientation" feature output interrupt.
   *
   * - bits [9:8]   step_detector_out
   *   - Routes "step detector" feature output interrupt.
   *
   * - bits [11:10] step_counter_out
   *   - Routes "step counter watermark" feature output interrupt.
   *
   * - bits [13:12] sig_motion_out
   *   - Routes "significant motion" feature output interrupt.
   *
   * - bits [15:14] tilt_out
   *   - Routes "tilt" feature output interrupt.
   *
   * Notes:
   * - This function overwrites ALL feature routing fields in INT_MAP1.
   * - Use setNoMotionInt()/setAnyMotionInt()/... if you want to update only one field.
   * - Recommended pattern:
   *   - build 'flags' using shifts:
   *     - flags |= (route & 0x3) << BMI330_INT_MAP1_NO_MOTION_POS;
   *     - flags |= (route & 0x3) << BMI330_INT_MAP1_ANY_MOTION_POS;
   *     - ...
   *
   * Returns:
   * - true  : register written successfully
   * - false : bus write failed
   */
  bool setFeatureInterrupts(uint16_t flags);

  /**
   * - Read INT1 Status Register (INT_STATUS_INT1, 0x0D).
   * - This register is clear-on-read: reading it clears all asserted bits.
   *
   * Bits (1 = event occurred and was routed to INT1):
   * - bit0  int1_no_motion
   *   - No motion detection output.
   *
   * - bit1  int1_any_motion
   *   - Any motion detection output.
   *
   * - bit2  int1_flat
   *   - Flat detection output.
   *
   * - bit3  int1_orientation
   *   - Orientation detection output.
   *
   * - bit4  int1_step_detector
   *   - Step detector output.
   *
   * - bit5  int1_step_counter
   *   - Step counter watermark output.
   *
   * - bit6  int1_sig_motion
   *   - Significant motion detection output.
   *
   * - bit7  int1_tilt
   *   - Tilt detection output.
   *
   * - bit8  int1_tap
   *   - Tap detection output.
   *
   * - bit9  int1_i3c
   *   - I3C TC sync data-ready interrupt.
   *
   * - bit10 int1_err_status
   *   - Feature engine error or status change.
   *
   * - bit11 int1_temp_drdy
   *   - Temperature data-ready interrupt.
   *
   * - bit12 int1_gyr_drdy
   *   - Gyroscope data-ready interrupt.
   *
   * - bit13 int1_acc_drdy
   *   - Accelerometer data-ready interrupt.
   *
   * - bit14 int1_fwm
   *   - FIFO watermark interrupt.
   *
   * - bit15 int1_ffull
   *   - FIFO full interrupt.
   *
   * Notes:
   * - Only events that are mapped to INT1 (via INT_MAP1 / INT_MAP2) will appear here.
   * - Because this register is clear-on-read, call it once and store the value if you
   *   need to check multiple bits.
   *
   * Returns:
   * - true  : read succeeded and 'status' updated
   * - false : bus read failed
   */
  bool readIntStatusInt1(uint16_t &status);

  /**
   * - Route "no-motion" feature interrupt output to INT1 / INT2 / I3C IBI (or disable).
   *
   * Notes:
   * - Uses INT_MAP1.no_motion_out (2-bit field at bit offset 0).
   * - FeatureIntOut values:
   *   - 0 = interrupt disabled
   *   - 1 = mapped to INT1
   *   - 2 = mapped to INT2
   *   - 3 = mapped to I3C IBI
   */
  bool setNoMotionInt(FeatureIntOut output);

  /**
   * - Read current routing of "no-motion" feature interrupt output.
   *
   * Notes:
   * - Returns the decoded INT_MAP1.no_motion_out field.
   */
  bool getNoMotionInt(FeatureIntOut &output);

  /**
   * - Route "any-motion" feature interrupt output to INT1 / INT2 / I3C IBI (or disable).
   *
   * Notes:
   * - Uses INT_MAP1.any_motion_out (2-bit field at bit offset 2).
   * - FeatureIntOut values:
   *   - 0 = interrupt disabled
   *   - 1 = mapped to INT1
   *   - 2 = mapped to INT2
   *   - 3 = mapped to I3C IBI
   */
  bool setAnyMotionInt(FeatureIntOut output);

  /**
   * - Read current routing of "any-motion" feature interrupt output.
   */
  bool getAnyMotionInt(FeatureIntOut &output);

  /**
   * - Route "flat" feature interrupt output to INT1 / INT2 / I3C IBI (or disable).
   *
   * Notes:
   * - Uses INT_MAP1.flat_out (2-bit field at bit offset 4).
   * - FeatureIntOut values:
   *   - 0 = interrupt disabled
   *   - 1 = mapped to INT1
   *   - 2 = mapped to INT2
   *   - 3 = mapped to I3C IBI
   */
  bool setFlatInt(FeatureIntOut output);

  /**
   * - Read current routing of "flat" feature interrupt output.
   */
  bool getFlatInt(FeatureIntOut &output);

  /**
   * - Route "orientation" feature interrupt output to INT1 / INT2 / I3C IBI (or disable).
   *
   * Notes:
   * - Uses INT_MAP1.orientation_out (2-bit field at bit offset 6).
   * - FeatureIntOut values:
   *   - 0 = interrupt disabled
   *   - 1 = mapped to INT1
   *   - 2 = mapped to INT2
   *   - 3 = mapped to I3C IBI
   */
  bool setOrientationInt(FeatureIntOut output);

  /**
   * - Read current routing of "orientation" feature interrupt output.
   */
  bool getOrientationInt(FeatureIntOut &output);

  /**
   * - Route "step detector" feature interrupt output to INT1 / INT2 / I3C IBI (or disable).
   *
   * Notes:
   * - Uses INT_MAP1.step_detector_out (2-bit field at bit offset 8).
   * - FeatureIntOut values:
   *   - 0 = interrupt disabled
   *   - 1 = mapped to INT1
   *   - 2 = mapped to INT2
   *   - 3 = mapped to I3C IBI
   */
  bool setStepDetectorInt(FeatureIntOut output);

  /**
   * - Read current routing of "step detector" feature interrupt output.
   */
  bool getStepDetectorInt(FeatureIntOut &output);

  /**
   * - Route "step counter watermark" feature interrupt output to INT1 / INT2 / I3C IBI (or disable).
   *
   * Notes:
   * - Uses INT_MAP1.step_counter_out (2-bit field at bit offset 10).
   * - FeatureIntOut values:
   *   - 0 = interrupt disabled
   *   - 1 = mapped to INT1
   *   - 2 = mapped to INT2
   *   - 3 = mapped to I3C IBI
   */
  bool setStepCounterInt(FeatureIntOut output);

  /**
   * - Read current routing of "step counter watermark" feature interrupt output.
   */
  bool getStepCounterInt(FeatureIntOut &output);

  /**
   * - Route "significant motion" feature interrupt output to INT1 / INT2 / I3C IBI (or disable).
   *
   * Notes:
   * - Uses INT_MAP1.sig_motion_out (2-bit field at bit offset 12).
   * - FeatureIntOut values:
   *   - 0 = interrupt disabled
   *   - 1 = mapped to INT1
   *   - 2 = mapped to INT2
   *   - 3 = mapped to I3C IBI
   */
  bool setSigMotionInt(FeatureIntOut output);

  /**
   * - Read current routing of "significant motion" feature interrupt output.
   */
  bool getSigMotionInt(FeatureIntOut &output);

  /**
   * - Route "tilt" feature interrupt output to INT1 / INT2 / I3C IBI (or disable).
   *
   * Notes:
   * - Uses INT_MAP1.tilt_out (2-bit field at bit offset 14).
   * - FeatureIntOut values:
   *   - 0 = interrupt disabled
   *   - 1 = mapped to INT1
   *   - 2 = mapped to INT2
   *   - 3 = mapped to I3C IBI
   */
  bool setTiltInt(FeatureIntOut output);

  /**
   * - Read current routing of "tilt" feature interrupt output.
   */
  bool getTiltInt(FeatureIntOut &output);

  /**
   * - Read the full INT_MAP1 register (0x3A) in one call.
   * - INT_MAP1 controls routing of feature-engine interrupts to INT1 / INT2 / I3C IBI.
   *
   * Route encoding (for every 2-bit field):
   * - 0b00 = interrupt disabled
   * - 0b01 = mapped to INT1
   * - 0b10 = mapped to INT2
   * - 0b11 = mapped to I3C IBI
   *
   * INT_MAP1 layout (each field is 2 bits):
   * - bits [1:0]   no_motion_out
   * - bits [3:2]   any_motion_out
   * - bits [5:4]   flat_out
   * - bits [7:6]   orientation_out
   * - bits [9:8]   step_detector_out
   * - bits [11:10] step_counter_out
   * - bits [13:12] sig_motion_out
   * - bits [15:14] tilt_out
   *
   * Notes:
   * - This returns the raw 16-bit register value.
   * - Use getNoMotionInt()/getAnyMotionInt()/... if you want one decoded field only.
   */
  bool getFeatureInterrupts(uint16_t &flags);

  /**
   * - Read interrupt status register for INT2 output.
   */
  bool readIntStatusInt2(uint16_t &status);

  /**
   * - Read interrupt status register for I3C IBI output.
   */
  bool readIntStatusIbi(uint16_t &status);

  bool readSaturationFlags(bool &accX, bool &accY, bool &accZ,
                           bool &gyrX, bool &gyrY, bool &gyrZ);

  /**
   * - Set accelerometer datapath offset for X axis (temperature independent).
   * - Register: ACC_DP_OFF_X (0x60), field width 14-bit, 0x2000 is invalid.
   * - 1 LSB = 30.52 µg (datasheet).
   *
   * Notes:
   * - Offset field uses a signed 14-bit coding; value -8192 is invalid (raw 0x2000).
   * - Valid range is typically -8191..+8191.
   *
   * Returns:
   * - true  : write succeeded
   * - false : invalid value or bus write failed
   */
  bool setAccelOffsetX(int16_t offset);

  /**
   * - Read accelerometer datapath offset for X axis (temperature independent).
   *
   * Notes:
   * - Returns sign-extended offset as int16_t.
   * - Raw code 0x2000 is invalid per datasheet; if read, this function returns false.
   *
   * Returns:
   * - true  : read succeeded and 'offset' updated
   * - false : bus read failed or invalid raw value detected
   */
  bool getAccelOffsetX(int16_t &offset);

  bool setAccelOffsetY(int16_t offset);
  bool getAccelOffsetY(int16_t &offset);

  bool setAccelOffsetZ(int16_t offset);
  bool getAccelOffsetZ(int16_t &offset);

  bool writeReg8(uint8_t reg, uint8_t value);
  bool readReg8(uint8_t reg, uint8_t &value);

  /**
   * - Set accelerometer datapath digital gain trim for X axis.
   *
   * Register format:
   * - ACC_DP_DGAIN_X (0x61)
   * - bits [7:0]  acc_dp_dgain_x (8-bit)
   * - bits [15:8] reserved (do not modify)
   *
   * Notes:
   * - Digital gain trim adjusts accelerometer sensitivity (datasheet: ±3.125% range).
   * - Use 8-bit register write to avoid overwriting neighboring registers.
   *
   * Returns:
   * - true  : write succeeded
   * - false : bus write failed
   */
  bool setAccelDgainX(uint8_t value);

  /**
   * - Read accelerometer datapath digital gain trim for X axis.
   *
   * Notes:
   * - Returns raw 8-bit gain code from ACC_DP_DGAIN_X.
   *
   * Returns:
   * - true  : read succeeded and 'value' updated
   * - false : bus read failed
   */
  bool getAccelDgainX(uint8_t &value);

  /**
   * - Set accelerometer datapath digital gain trim for Y axis.
   *
   * Register format:
   * - ACC_DP_DGAIN_Y (0x63)
   * - bits [7:0]  acc_dp_dgain_y (8-bit)
   * - bits [15:8] reserved (do not modify)
   *
   * Notes:
   * - Use 8-bit register write to avoid overwriting neighboring registers.
   *
   * Returns:
   * - true  : write succeeded
   * - false : bus write failed
   */
  bool setAccelDgainY(uint8_t value);

  /**
   * - Read accelerometer datapath digital gain trim for Y axis.
   *
   * Returns:
   * - true  : read succeeded and 'value' updated
   * - false : bus read failed
   */
  bool getAccelDgainY(uint8_t &value);

  /**
   * - Set accelerometer datapath digital gain trim for Z axis.
   *
   * Register format:
   * - ACC_DP_DGAIN_Z (0x65)
   * - bits [7:0]  acc_dp_dgain_z (8-bit)
   * - bits [15:8] reserved (do not modify)
   *
   * Notes:
   * - Use 8-bit register write to avoid overwriting neighboring registers.
   *
   * Returns:
   * - true  : write succeeded
   * - false : bus write failed
   */
  bool setAccelDgainZ(uint8_t value);

  /**
   * - Read accelerometer datapath digital gain trim for Z axis.
   *
   * Returns:
   * - true  : read succeeded and 'value' updated
   * - false : bus read failed
   */
  bool getAccelDgainZ(uint8_t &value);

  /**
   * - Read the feature-engine status/error code from FEATURE_IO1.
   *
   * What you get:
   * - 'err' returns the low-nibble status field (bits [3:0]).
   * - Use this to detect whether the feature engine is ready or in an error state.
   *
   * Notes:
   * - Datasheet uses FEATURE_IO1.error_status to indicate initialization/ready state.
   * - Typical driver flow: enableFeatureEngine(true) then poll until status indicates ready.
   *
   * Returns:
   * - true  : read succeeded and 'err' updated
   * - false : bus read failed
   */
  bool getFeatureErrorStatus(uint8_t &err);

  /**
   * - Check if the feature engine is enabled/active.
   *
   * What it checks:
   * - Reads FEATURE_CTRL.engine_en (or engine status register if you implement it that way)
   * - Returns true when the feature engine is enabled and running.
   *
   * Notes:
   * - Some BMI330 modes require initialization writes before the engine becomes ready.
   * - If you enable the engine, you should still waitFeatureEngineReady() before using features.
   *
   * Returns:
   * - true  : feature engine is enabled/active
   * - false : feature engine is disabled (or read failed if you implement it that way)
   */
  bool isFeatureEngineActive();

  /**
   * - Check whether self-calibration has completed.
   *
   * What it does:
   * - Reads a status flag from the feature engine and sets 'result':
   *   - result = true  -> self calibration completed
   *   - result = false -> still running / not done
   *
   * Notes:
   * - This only reports completion state; it does not start calibration by itself.
   * - If you need to start calibration, use the corresponding feature/extended register API.
   *
   * Returns:
   * - true  : read succeeded and 'result' updated
   * - false : bus read failed
   */
  bool isSelfCalibrationComplete(bool &result);

  /**
   * - Check whether self-test has completed.
   *
   * What it does:
   * - Reads a status flag and sets 'result':
   *   - result = true  -> self-test completed
   *   - result = false -> self-test still running / not done
   *
   * Notes:
   * - Completion does not automatically mean PASS/FAIL unless you also read the result code.
   *
   * Returns:
   * - true  : read succeeded and 'result' updated
   * - false : bus read failed
   */
  bool isSelfTestComplete(bool &result);

  /**
   * - Check if axis remap / axis mapping process is complete.
   *
   * What you get:
   * - 'complete' tells you whether axis mapping is finished.
   *
   * Notes:
   * - Only meaningful if you have started an axis remap procedure.
   *
   * Returns:
   * - true  : read succeeded and 'complete' updated
   * - false : bus read failed
   */
  bool isAxisMapComplete(bool &complete);

  /**
   * - Read the feature engine state (raw state code).
   *
   * What you get:
   * - 'state' returns a raw engine state code defined by the device.
   *
   * Notes:
   * - This is useful for debugging feature engine bring-up:
   *   - not initialized / initializing / ready / faulted states.
   *
   * Returns:
   * - true  : read succeeded and 'state' updated
   * - false : bus read failed
   */
  bool getFeatureState(uint8_t &state);

  /**
   * - Enable or disable the feature engine.
   *
   * How to use:
   * - Call enableFeatureEngine(true) once during begin()/setup.
   * - After enabling, the driver performs required init/sync steps and waits for ready.
   *
   * Notes:
   * - Disabling the engine may require reset/power-cycle to enable again (device behavior).
   * - For reliable use, enable engine before enabling any features (step/tap/motion/etc.).
   *
   * Returns:
   * - true  : operation succeeded (and engine becomes ready if enabling)
   * - false : bus error or engine did not become ready in time
   */
  bool enableFeatureEngine(bool enable, uint32_t timeoutMs = 100);

  /**
   * - Enable or disable a single feature bit in FEATURE_IO0.
   *
   * How to use:
   * - enableStepCounter(true) calls this internally.
   * - You can call setFeature() directly if you know the bit index.
   *
   * Notes:
   * - This modifies one feature bit and then calls applyFeatureIO() to sync settings.
   * - Feature must be supported and the feature engine must be enabled first.
   *
   * Returns:
   * - true  : feature bit updated and synced
   * - false : bus error or sync failed
   */
  bool setFeature(BMI330Feature feature, bool enable);

  /**
   * - Apply (sync) FEATURE_IO0..FEATURE_IO3 into the feature engine.
   *
   * How to use:
   * - Call this after changing any of FEATURE_IO0..3.
   * - setFeature() calls this automatically.
   *
   * Notes:
   * - Without calling applyFeatureIO(), changes in FEATURE_IO0..3 may not take effect.
   *
   * Returns:
   * - true  : sync trigger succeeded
   * - false : bus write failed
   */
  bool applyFeatureIO();

  /**
   * - Enable/disable step counter feature.
   *
   * How to use:
   * - enableFeatureEngine(true) first.
   * - Then enableStepCounter(true).
   * - Read the value using readStepCount().
   *
   * Returns:
   * - true  : feature enabled/disabled successfully
   * - false : bus error or sync failed
   */
  bool enableStepCounter(bool enable);

  /**
   * - Enable/disable step detector feature.
   *
   * Notes:
   * - Step detector produces interrupt/status events, while step counter provides a count value.
   *
   * Returns:
   * - true  : feature enabled/disabled successfully
   * - false : bus error or sync failed
   */
  bool enableStepDetector(bool enable);

  /**
   * - Enable tap detection features.
   *
   * Parameters:
   * - singleTap : enable single tap detection
   * - doubleTap : enable double tap detection
   * - tripleTap : enable triple tap detection (default: false)
   *
   * How to use:
   * - enableFeatureEngine(true)
   * - enableTap(true, true)   // enable single + double
   * - Read events using readTapEvent(...)
   *
   * Returns:
   * - true  : configuration applied successfully
   * - false : bus error or sync failed
   */
  bool enableTap(bool singleTap, bool doubleTap, bool tripleTap = false);

  /**
   * - Enable per-axis any-motion and no-motion feature bits.
   *
   * Parameters:
   * - anyX/anyY/anyZ : enable any-motion detection per axis
   * - noX/noY/noZ    : enable no-motion detection per axis
   *
   * How to use:
   * - enableFeatureEngine(true)
   * - enableMotion(true, true, true,  false, false, false)  // any-motion on all axes
   *
   * Returns:
   * - true  : configuration applied successfully
   * - false : bus error or sync failed
   */
  bool enableMotion(bool anyX, bool anyY, bool anyZ,
                    bool noX, bool noY, bool noZ);

  /**
   * - Enable/disable orientation detection feature.
   *
   * Returns:
   * - true  : feature enabled/disabled successfully
   * - false : bus error or sync failed
   */
  bool enableOrientation(bool enable);

  /**
   * - Enable/disable flat detection feature.
   *
   * Returns:
   * - true  : feature enabled/disabled successfully
   * - false : bus error or sync failed
   */
  bool enableFlat(bool enable);

  /**
   * - Enable/disable tilt detection feature.
   *
   * Returns:
   * - true  : feature enabled/disabled successfully
   * - false : bus error or sync failed
   */
  bool enableTilt(bool enable);

  /**
   * - Enable/disable significant motion detection feature.
   *
   * Returns:
   * - true  : feature enabled/disabled successfully
   * - false : bus error or sync failed
   */
  bool enableSignificantMotion(bool enable);

  /**
   * - Read 32-bit step count from the feature engine.
   *
   * How to use:
   * - enableFeatureEngine(true)
   * - enableStepCounter(true)
   * - call readStepCount(steps) periodically
   *
   * Returns:
   * - true  : read succeeded and 'steps' updated
   * - false : bus read failed
   */
  bool readStepCount(uint32_t &steps);

  /**
   * - Reset the step counter value to zero.
   *
   * Notes:
   * - Uses an extended feature register reset bit and then applies changes.
   * - After resetStepCount(), readStepCount() should return 0 (or restart from 0).
   *
   * Returns:
   * - true  : reset sequence completed successfully
   * - false : bus error or sync failed
   */
  bool resetStepCount();

  /**
   * - Read tap event flags (single/double/triple).
   *
   * Parameters:
   * - single    : set true if a single tap event occurred
   * - doubleTap : set true if a double tap event occurred
   * - triple    : set true if a triple tap event occurred
   *
   * Important:
   * - Do NOT use dynamic allocation as a default argument.
   * - Prefer a normal reference parameter; caller can pass a local bool.
   *
   * Example:
   * - bool s,d,t;
   * - readTapEvent(s,d,t);
   *
   * Returns:
   * - true  : read succeeded and flags updated
   * - false : bus read failed
   */
  bool readTapEvent(bool &single, bool &doubleTap, bool &triple);

  /**
   * - Write a 16-bit word to the extended feature register space.
   *
   * How to use:
   * - extWrite(extAddr, value) writes 'value' to the internal feature config memory.
   *
   * Notes:
   * - Sequence is: write FEATURE_DATA_ADDR, then write FEATURE_DATA_TX.
   * - Some extended registers require applyFeatureIO() after changes.
   *
   * Returns:
   * - true  : write succeeded
   * - false : bus write failed
   */
  bool extWrite(uint16_t extAddress, uint16_t value);

  /**
   * - Read a 16-bit word from the extended feature register space.
   *
   * Notes:
   * - Sequence is: write FEATURE_DATA_ADDR, then read back data word.
   * - If the device uses a dedicated RX register (not TX), this function must read that register.
   *
   * Returns:
   * - true  : read succeeded and 'value' updated
   * - false : bus read failed
   */
  bool extRead(uint16_t extAddress, uint16_t &value);

  /**
   * - Write multiple extended feature words starting at a given extended address.
   *
   * Parameters:
   * - startExtAddress : first extended address to write
   * - values          : pointer to an array of 16-bit values
   * - wordCount       : number of 16-bit words to write
   *
   * Notes:
   * - This calls extWrite() repeatedly and increments the extended address by 1 each time.
   *
   * Returns:
   * - true  : all words written successfully
   * - false : bus write failed at some point
   */
  bool extWriteBlock(uint16_t startExtAddress, const uint16_t *values, size_t wordCount);

  void enableAccelSmooth(bool en)
  {
    enable_smooth_accel = en;
    if (!en)
      smooth_accel.reset();
  }
  void enableGyroSmooth(bool en)
  {
    enable_smooth_gyro = en;
    if (!en)
      smooth_gyro.reset();
  }

  void setAccelSmooth(uint16_t alpha, uint8_t shift = 8)
  {
    smooth_accel.setShift(shift);
    smooth_accel.setAlpha(alpha);
  }
  void setGyroSmooth(uint16_t alpha, uint8_t shift = 8)
  {
    smooth_gyro.setShift(shift);
    smooth_gyro.setAlpha(alpha);
  }

private:
  Bus bus = Bus::NONE;

  /* I2C */
  uint8_t address = BMI330_DEFAULT_I2C_ADDR;
  TwoWire *i2c = nullptr;
  uint32_t i2c_clock_speed = 400000;

  /* SPI */
  uint8_t cs = 255;
  SPIClass *spi = nullptr;
  uint32_t spi_clock_speed = 10000000;

  /* Cached ranges for scaling */
  AccRange accel_range = ACC_RANGE_2G;
  GyrRange gyro_range = GYR_RANGE_2000DPS;

private:
  /**
   * - BMI330 specific I2C register read.
   * - Uses repeated-start and discards BMI330 dummy bytes before returning payload.
   */
  bool i2cRead(uint8_t reg, uint8_t *buf, size_t len);

  /**
   * - I2C register write: sends reg address then 'len' bytes.
   */
  bool i2cWrite(uint8_t reg, const uint8_t *buf, size_t len);

  /**
   * - SPI register read: read command + dummy + payload.
   */
  bool spiRead(uint8_t reg, uint8_t *buf, size_t len);

  /**
   * - SPI register write: write command + payload.
   */
  bool spiWrite(uint8_t reg, const uint8_t *buf, size_t len);

  /**
   * - Update a multi-bit field inside a 16-bit register (read-modify-write).
   * - Preserves unrelated bits and reduces configuration mistakes.
   *
   * Notes:
   * - pos+width must fit inside 16 bits.
   */
  bool updateField(uint8_t reg, uint8_t pos, uint8_t width, uint16_t fieldValue);

  /**
   * - Return accelerometer sensitivity in LSB per mg for the current range.
   *
   * Notes:
   * - Convert raw -> mg:
   *   - mg = raw / accelLsbPerMg()
   * - Convert raw -> g:
   *   - g = (raw / accelLsbPerMg()) / 1000
   */
  float accelLsbPerMg() const;

  /**
   * - Return gyroscope sensitivity in LSB per dps for the current range.
   *
   * Notes:
   * - Convert raw -> dps:
   *   - dps = raw / gyroLsbPerDps()
   */
  float gyroLsbPerDps() const;

  /**
   * - Set a 2-bit routing field in INT_MAP1 (internal helper).
   */
  bool setIntMap1Field(uint8_t fieldPos, FeatureIntOut route);

  /**
   * - Read a 2-bit routing field from INT_MAP1 (internal helper).
   */
  bool getIntMap1Field(uint8_t fieldPos, FeatureIntOut &route);

  static int16_t bytesToUint16(uint8_t lsb, uint8_t msb)
  {
    return (int16_t)((uint16_t)msb << 8 | lsb);
  }

  /** Clamp int32 into int16 */
  static inline int16_t bmi330_clamp_i16(int32_t v)
  {
    if (v > 32767)
      return 32767;
    if (v < -32768)
      return -32768;
    return (int16_t)v;
  }

  /** 1st-order  smoother
   *  - y += (alpha * (x - y)) >> shift
   *  - alpha: 0..(2^shift). Bigger alpha = faster response, less smoothing
   *  - shift: scaling, typical 8
   *
   * Example (shift=8):
   *  - alpha=16  => strong smoothing
   *  - alpha=64  => medium smoothing
   *  - alpha=128 => light smoothing
   */
  struct BMI330_Smooth1
  {
    uint8_t shift = 20;
    uint16_t alpha = 50; // default medium
    int32_t y = 0;
    bool inited = false;

    int16_t update(int16_t x)
    {
      if (!inited)
      {
        y = (int32_t)x;
        inited = true;
        return x;
      }
      int32_t err = (int32_t)x - y;
      y += ((int32_t)alpha * err) >> shift;
      return bmi330_clamp_i16(y);
    }

    void reset() { inited = false; }
  };

  /** 3-axis smoothing */
  struct BMI330_Smooth3
  {
    BMI330_Smooth1 fx, fy, fz;

    void setAlpha(uint16_t a)
    {
      fx.alpha = a;
      fy.alpha = a;
      fz.alpha = a;
    }
    void setShift(uint8_t s)
    {
      fx.shift = s;
      fy.shift = s;
      fz.shift = s;
    }
    void reset()
    {
      fx.reset();
      fy.reset();
      fz.reset();
    }

    void update(int16_t &x, int16_t &y, int16_t &z)
    {
      x = fx.update(x);
      y = fy.update(y);
      z = fz.update(z);
    }
  };

  // /** Bitfield helper */
  // bool updateRegisterField(uint8_t reg, uint8_t pos, uint8_t width, uint16_t fieldValue);

  /** Feature engine init polling */
  bool waitFeatureEngineReady(uint32_t timeoutMs = 200);

  bool enable_smooth_accel = false;
  bool enable_smooth_gyro = false;

  BMI330_Smooth3 smooth_accel;
  BMI330_Smooth3 smooth_gyro;
};
