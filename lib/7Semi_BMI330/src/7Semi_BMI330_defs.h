#pragma once
#include <Arduino.h>

/**
 * BMI330 register map 
 * - Default I2C address used by most BMI330 boards.
 * - If your board straps SDO/SA0 differently, your address may differ.
 */
#ifndef BMI330_DEFAULT_I2C_ADDR
#define BMI330_DEFAULT_I2C_ADDR (0x68u)
#endif

/**
 * - CHIP_ID (read-only).
 * - Only bits[7:0] contain chip_id. Upper byte is reserved.
 * - Expected chip id LSB is 0x48. 
 */
#define BMI330_REG_CHIP_ID           (0x00u)

/**
 * - ERR_REG (read/clear-on-read fields).
 * - Reports configuration errors and feature engine fault conditions.
 */
#define BMI330_REG_ERR_REG           (0x01u)  /* Datasheet: ERR_REG at 0x01. 

/**
 * - STATUS register.
 * - Contains basic DRDY flags and POR detection.
 */
#define BMI330_REG_STATUS            (0x02u)

/**
 * - Accelerometer output registers (16-bit signed, two's complement).
 * - Burst read from ACC_DATA_X provides consistent XYZ values.
 */
#define BMI330_REG_ACC_DATA_X        (0x03u)
#define BMI330_REG_ACC_DATA_Y        (0x04u)
#define BMI330_REG_ACC_DATA_Z        (0x05u)

/**
 * - Gyroscope output registers (16-bit signed, two's complement).
 */
#define BMI330_REG_GYR_DATA_X        (0x06u)
#define BMI330_REG_GYR_DATA_Y        (0x07u)
#define BMI330_REG_GYR_DATA_Z        (0x08u)

/**
 * - Temperature output register (16-bit signed).
 */
#define BMI330_REG_TEMP_DATA         (0x09u)

/**
 * - Sensor time (32-bit) is split across two 16-bit words.
 * - SENSOR_TIME_0 = bits[15:0]
 * - SENSOR_TIME_1 = bits[31:16]
 * - 1 LSB corresponds to 39.0625 us (from your datasheet excerpt).
 */
#define BMI330_REG_SENSOR_TIME_0     (0x0Au)
#define BMI330_REG_SENSOR_TIME_1     (0x0Bu)

/**
 * - SAT_FLAGS register.
 * - Per-axis saturation flags for accel/gyro signals. 
 */
#define BMI330_REG_SATURATION_FLAG         (0x0Cu)

/**
 * - Interrupt status registers (clear-on-read).
 * - Separate status exists for INT1, INT2, and IBI. 
 */
#define BMI330_REG_INT_STATUS_INT1   (0x0Du)
#define BMI330_REG_INT_STATUS_INT2   (0x0Eu)
#define BMI330_REG_INT_STATUS_IBI    (0x0Fu)

/**
 * - Feature engine I/O registers (word addressed).
 * - FEATURE_IO0 contains enable bits for advanced features. 
 */
#define BMI330_REG_FEATURE_IO0       (0x10u)
#define BMI330_REG_FEATURE_IO1       (0x11u)
#define BMI330_REG_FEATURE_IO2       (0x12u)
#define BMI330_REG_FEATURE_IO3       (0x13u)

/**
 * - FEATURE_IO_STATUS:
 * - Used to apply/synchronize FEATURE_IO0..3 settings into the feature engine. 
 */
#define BMI330_REG_FEATURE_IO_STATUS (0x14u)

/**
 * - FIFO registers:
 * - FIFO_FILL_LEVEL is in words. 
 * - FIFO_DATA is a 16-bit word output; burst read is required and address does not increment
 *   when bursting at FIFO_DATA (host must read as a stream). (From your excerpt.)
 */
#define BMI330_REG_FIFO_FILL_LEVEL   (0x15u)
#define BMI330_REG_FIFO_DATA         (0x16u)

/**
 * - Main sensor configuration registers:
 * - ACC_CONF and GYR_CONF hold ODR/range/bw/avg/mode fields. 
 */
#define BMI330_REG_ACC_CONF          (0x20u)
#define BMI330_REG_GYR_CONF          (0x21u)

/**
 * - ALT_* configuration registers (auto-mode feature support).
 */
#define BMI330_REG_ALT_ACC_CONF      (0x28u)
#define BMI330_REG_ALT_GYR_CONF      (0x29u)
#define BMI330_REG_ALT_CONF          (0x2Au)
#define BMI330_REG_ALT_STATUS        (0x2Bu)

/**
 * - FIFO watermark/conf/control.
 */
#define BMI330_REG_FIFO_WATERMARK    (0x35u)
#define BMI330_REG_FIFO_CONF         (0x36u)
#define BMI330_REG_FIFO_CTRL         (0x37u)

/**
 * - Interrupt pin electrical configuration and mapping.
 */
#define BMI330_REG_IO_INT_CTRL       (0x38u)
#define BMI330_REG_INT_CONF          (0x39u)
#define BMI330_REG_INT_MAP1          (0x3Au)
#define BMI330_REG_INT_MAP2          (0x3Bu)

/**
 * - Feature engine control and data interface.
 */
#define BMI330_REG_FEATURE_CTRL          (0x40u)
#define BMI330_REG_FEATURE_DATA_ADDR     (0x41u)
#define BMI330_REG_FEATURE_DATA_TX       (0x42u)
#define BMI330_REG_FEATURE_DATA_STATUS   (0x43u)
#define BMI330_REG_FEATURE_ENGINE_STATUS (0x45u)
#define BMI330_REG_FEATURE_EVENT_EXT     (0x47u)

/**
 * - Digital interface configuration registers.
 */
#define BMI330_REG_IO_SPI_IF         (0x50u)
#define BMI330_REG_IO_I2C_IF         (0x52u)

/**
 * - Data-path offset/gain registers for accelerometer and gyroscope.
 * - ACC data-path offset is 14-bit in the word; dgain is 8-bit in the low byte.
 */
#define BMI330_REG_ACC_DP_OFF_X      (0x60u)
#define BMI330_REG_ACC_DP_DGAIN_X    (0x61u)
#define BMI330_REG_ACC_DP_OFF_Y      (0x62u)
#define BMI330_REG_ACC_DP_DGAIN_Y    (0x63u)
#define BMI330_REG_ACC_DP_OFF_Z      (0x64u)
#define BMI330_REG_ACC_DP_DGAIN_Z    (0x65u)

#define BMI330_REG_GYR_DP_OFF_X      (0x66u)
#define BMI330_REG_GYR_DP_DGAIN_X    (0x67u)
#define BMI330_REG_GYR_DP_OFF_Y      (0x68u)
#define BMI330_REG_GYR_DP_DGAIN_Y    (0x69u)
#define BMI330_REG_GYR_DP_OFF_Z      (0x6Au)
#define BMI330_REG_GYR_DP_DGAIN_Z    (0x6Bu)

/**
 * - CMD register.
 * - Used to send commands such as soft reset.
 */
#define BMI330_REG_CMD               (0x7Eu)

/**
 * - CHIP_ID expected register value:
 * - Word read returns 0x0048 (chip_id in LSB, MSB reserved). 
 */
#define BMI330_CHIP_ID_VALUE         (0x0048u)

/**
 * - Soft reset command value (CMD).
 * - Datasheet shows soft reset command is 0xDEAF.
 *
 * Important:
 * - Your current code uses 0x00B6 in some places; that is NOT the datasheet soft reset command.
 * - If you want Bosch datasheet behavior, use 0xDEAF.
 */
#define BMI330_CMD_SOFT_RESET        (0xDEAFu)

/**
 * - FIFO_CONF fields (1-bit each).
 * - These control which items are written into FIFO and whether FIFO overwrites or stops on full.
 */
#define BMI330_FIFO_STOP_ON_FULL_BIT (0u)
#define BMI330_FIFO_TIME_EN_BIT      (8u)
#define BMI330_FIFO_ACC_EN_BIT       (9u)
#define BMI330_FIFO_GYR_EN_BIT       (10u)
#define BMI330_FIFO_TEMP_EN_BIT      (11u)

#define BMI330_ODR_BIT 0 
#define BMI330_RANGE_BIT 4 
#define BMI330_BW_BIT 7 
#define BMI330_AVG_BIT 8 
#define BMI330_MODE_BIT 12

/**
 * - FIFO_CTRL fields.
 * - Writing 1 to fifo_flush clears FIFO buffer. (From your excerpt.)
 */
#define BMI330_FIFO_FLUSH_BIT        (0u)

/**
 * - FIFO field masks (word sized).
 */
#define BMI330_FIFO_FILL_LEVEL_MASK  (0x07FFu) /* 11-bit fill level. 
#define BMI330_REG_FIFO_WATERMARK    (0x03FFu) /* 10-bit watermark field. */

/**
 * - INT_CONF fields.
 * - int_latch controls latched vs non-latched interrupt behavior. 
 */
#define BMI330_INT_LATCH_BIT         (0u)

/**
 * - FEATURE_CTRL fields.
 * - Enables the feature engine.
 */
#define BMI330_FEATURE_ENGINE_EN_BIT (0u)

/**
 * - ACC data-path offset field:
 * - Stored as 14-bit value in the register word (mask 0x3FFF).
 * - Code 0x2000 is invalid (your existing check matches that rule).
 */
#define BMI330_ACC_DP_OFF_MASK       (0x3FFFu)
#define BMI330_ACC_DP_OFF_INVALID    (0x2000u)

#define BMI330_FEAT_NO_MOTION_X_EN   (0u)
#define BMI330_FEAT_NO_MOTION_Y_EN   (1u)
#define BMI330_FEAT_NO_MOTION_Z_EN   (2u)

#define BMI330_FEAT_ANY_MOTION_X_EN  (3u)
#define BMI330_FEAT_ANY_MOTION_Y_EN  (4u)
#define BMI330_FEAT_ANY_MOTION_Z_EN  (5u)

#define BMI330_FEAT_FLAT_EN          (6u)
#define BMI330_FEAT_ORIENTATION_EN   (7u)

#define BMI330_FEAT_STEP_DETECTOR_EN (8u)
#define BMI330_FEAT_STEP_COUNTER_EN  (9u)

#define BMI330_FEAT_SIG_MOTION_EN    (10u)
#define BMI330_FEAT_TILT_EN          (11u)

#define BMI330_FEAT_TAP_S_EN         (12u)
#define BMI330_FEAT_TAP_D_EN         (13u)
#define BMI330_FEAT_TAP_T_EN         (14u)

#define BMI330_FEAT_I3C_SYNC_EN      (15u)



