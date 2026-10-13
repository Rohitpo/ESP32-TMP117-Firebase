/**
 * FIFO_Example.ino — 7Semi BMI330 FIFO read + decode example
 *
 * What this does
 * - Starts BMI330 over I2C
 * - Configures accel + gyro for a stable 100 Hz setup
 * - Enables FIFO (accel + gyro only) to keep FIFO load low and stable
 * - Polls FIFO fill level, drains FIFO in chunks, and decodes frames
 * - Prints every Nth decoded sample to avoid Serial overload (important on UNO)
 *
 * Wiring (I2C)
 * - VDD  -> 3V3
 * - GND  -> GND
 * - SDA  -> SDA
 * - SCL  -> SCL
 *
 * Notes
 * - I2C address is typically 0x68 or 0x69 depending on SDO/SA0 wiring..
 * - Keep MAX_WORDS small on AVR boards to avoid RAM issues.
 */
#include <7Semi_BMI330.h>

BMI330_7Semi imu;

/**
 * FIFO read buffer
 * - Smaller buffer = safer on boards with low RAM (e.g., UNO/Nano).
 * - Each entry is a 16-bit word read from FIFO.
 */
static const size_t MAX_WORDS = 32;
uint16_t fifoWords[MAX_WORDS];

void setup()
{
  /**
   * Serial output
   * - Adjust baud rate based on your board and how much you print.
   * - Higher baud helps reduce time spent blocking on Serial writes.
   */
  Serial.begin(115200);
  delay(200);

  /**
   * Start BMI330 over I2C
   * - address: 0x69 here (use 0x68 if your board uses that address)
   * - wire:    Wire
   * - clock:   400kHz fast-mode
   */
  if (!imu.beginI2C(0x69, Wire, 400000))
  {
    Serial.println("beginI2C failed");
    while (1) { delay(100); }
  }

  /**
   * Safe 100 Hz configuration
   *
   * Accelerometer
   * - ODR:   100 Hz
   * - Range: ±4g
   * - BW:    ODR/2
   * - AVG:   2 samples (light smoothing)
   * - Mode:  Continuous low-power (keeps bandwidth/CPU stable)
   */
  imu.setAccelConfig(
      BMI330_7Semi::ODR_100HZ,
      BMI330_7Semi::ACC_RANGE_4G,
      BMI330_7Semi::BW_ODR_DIV2,
      BMI330_7Semi::AVG_2,
      BMI330_7Semi::MODE_CONTINUOUS_LP);

  /**
   * Gyroscope
   * - ODR:   100 Hz
   * - Range: ±2000 dps (wide range to avoid saturation)
   * - BW:    ODR/2
   * - AVG:   2 samples
   * - Mode:  Continuous low-power
   */
  imu.setGyroConfig(
      BMI330_7Semi::ODR_100HZ,
      BMI330_7Semi::GYR_RANGE_2000DPS,
      BMI330_7Semi::BW_ODR_DIV2,
      BMI330_7Semi::AVG_2,
      BMI330_7Semi::MODE_CONTINUOUS_LP);

  /**
   * FIFO configuration
   * - Enable only accel + gyro for stability and lower FIFO bandwidth.
   * - Disable temp/time to reduce FIFO load on slow MCUs and long I2C wiring.
   */
  BMI330_7Semi::FifoConfig cfg;
  cfg.enable_accel = true;
  cfg.enable_gyro  = true;
  cfg.enable_temp  = false;
  cfg.enable_time  = false;
  cfg.stop_on_full = false;

  if (!imu.setFifoConfig(cfg))
  {
    Serial.println("setFifoConfig failed");
    while (1) { delay(100); }
  }

  /**
   * FIFO watermark
   * - If you route FIFO watermark to an interrupt pin, this value controls when it triggers.
   * - Even without interrupts, a watermark helps define “enough data to read” logic.
   */
  imu.setFifoWatermark(16);

  Serial.println("BMI330 FIFO running");
}

void loop()
{
  uint16_t fill = 0;

  /**
   * Query FIFO fill level.
   * - If the call fails, do nothing this cycle.
   */
  if (!imu.getFifoFillLevel(fill))
    return;

  /**
   * No data available yet.
   */
  if (fill == 0)
    return;

  /**
   * Drain FIFO completely.
   * - Read in chunks up to MAX_WORDS.
   * - Decode frames from each chunk.
   */
  while (fill > 0)
  {
    /**
     * Decide how many 16-bit words to read this pass.
     * - Keep reads bounded by MAX_WORDS to avoid overrunning the local buffer.
     */
    size_t toRead = (fill > MAX_WORDS) ? MAX_WORDS : fill;

    /**
     * Read raw FIFO words into fifoWords[].
     * - If read fails, exit loop (bus error, sensor reset, etc.).
     */
    if (!imu.readFifo(fifoWords, toRead))
      return;

    /**
     * Decode FIFO frame(s)
     * - This example decodes one sample from the current buffer.
     * - If your library supports iterating through multiple frames per chunk,
     *   you can decode in a loop to process every frame in fifoWords[].
     */
    BMI330_7Semi::Sample s;
    if (imu.decodeFifoFrame(fifoWords, toRead, s))
    {
        Serial.print("A[g]: ");
        Serial.print(s.ax_g); Serial.print(", ");
        Serial.print(s.ay_g); Serial.print(", ");
        Serial.print(s.az_g);

        Serial.print(" | G[dps]: ");
        Serial.print(s.gx_dps); Serial.print(", ");
        Serial.print(s.gy_dps); Serial.print(", ");
        Serial.print(s.gz_dps);

        Serial.println();
    }

    /**
     * Update remaining fill level.
     */
    fill -= toRead;
  }
}