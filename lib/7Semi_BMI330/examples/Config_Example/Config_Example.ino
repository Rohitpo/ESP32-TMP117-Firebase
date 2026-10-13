/*
 * Config_Example.ino — 7Semi BMI330 custom configuration example
 *
 * What this does
 * - Starts BMI330 over I2C
 * - Applies a custom accelerometer + gyroscope configuration
 * - Prints accel/gyro/temp periodically
 *
 * Wiring (I2C)
 * - VDD  -> 3V3
 * - GND  -> GND
 * - SDA  -> SDA
 * - SCL  -> SCL
 *
 * Notes
 * - The I2C address is typically 0x68 or 0x69 depending on SDO.
 */

#include <7Semi_BMI330.h>

BMI330_7Semi imu;

void setup() {
  Serial.begin(115200);
  delay(200);

  /**
   * Start the sensor on I2C.
   * - address: 0x69 in this example (change to 0x68 if your board uses that)
   * - wire:    Wire instance
   * - clock:   400kHz fast-mode
   */
  if (!imu.beginI2C(0x69, Wire, 400000)) {
    Serial.println("beginI2C failed");
    while (1) delay(100);
  }

  /**
   * Example configuration (change as needed)
   *
   * Accelerometer
   * - ODR:   100 Hz
   * - Range: ±4g
   * - BW:    ODR/2
   * - AVG:   2 samples (smoothing)
   * - Mode:  High performance
   */
  if (!imu.setAccelConfig(BMI330_7Semi::ODR_100HZ,
                          BMI330_7Semi::ACC_RANGE_4G,
                          BMI330_7Semi::BW::BW_ODR_DIV2,
                          BMI330_7Semi::AVG_2,
                          BMI330_7Semi::Mode::MODE_HIGH_PERFORMANCE)) {
    Serial.println("setAccelConfig failed");
  }

  /**
   * Gyroscope
   * - ODR:   100 Hz
   * - Range: ±500 dps
   * - BW:    ODR/2
   * - AVG:   2 samples (smoothing)
   * - Mode:  High performance
   *
   * Important
   * - Your original snippet used 125 dps while the comment said 500 dps.
   * - This is corrected to 500 dps below so comment and config match.
   */
  if (!imu.setGyroConfig(BMI330_7Semi::ODR_100HZ,
                         BMI330_7Semi::GyrRange::GYR_RANGE_500DPS,
                         BMI330_7Semi::BW::BW_ODR_DIV2,
                         BMI330_7Semi::AVG_2,
                         BMI330_7Semi::Mode::MODE_HIGH_PERFORMANCE)) {
    Serial.println("setGyroConfig failed");
  }

  Serial.println("BMI330 configured");
}

void loop() {
  float ax, ay, az;
  float gx, gy, gz;
  float t;

  /**
   * Read accel + gyro + temperature.
   * - Using a single combined "if" keeps the printout clean:
   *   either everything prints, or we print a single "read failed".
   */
  if (imu.readAccel(ax, ay, az) && imu.readGyro(gx, gy, gz) && imu.readTemperatureC(t)) {
    Serial.print("A[g] ");
    Serial.print(ax, 3);
    Serial.print(", ");
    Serial.print(ay, 3);
    Serial.print(", ");
    Serial.print(az, 3);

    Serial.print("  G[dps] ");
    Serial.print(gx, 2);
    Serial.print(", ");
    Serial.print(gy, 2);
    Serial.print(", ");
    Serial.print(gz, 2);

    Serial.print("  T[C] ");
    Serial.println(t, 2);
  } else {
    Serial.println("read failed");
  }

  delay(200);
}