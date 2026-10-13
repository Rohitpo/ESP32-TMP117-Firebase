/**
 * SerialPlotter_I2C.ino — 7Semi BMI330 Serial Plotter example (I2C)
 *
 * What this does
 * - Starts the BMI330 over I2C
 * - Reads accel + gyro (and optional temperature)
 * - Prints NUMBERS ONLY in a fixed order so Arduino Serial Plotter can graph them
 *
 * Serial Plotter tips
 * - Open: Tools -> Serial Plotter
 * - Set baud rate to 115200 (must match Serial.begin)
 * - Print only numbers separated by spaces or tabs (no labels)
 * - Keep the same number of fields every line for stable plots
 *
 * Wiring (I2C)
 * - VDD  -> 3V3
 * - GND  -> GND
 * - SDA  -> SDA
 * - SCL  -> SCL
 *
 * Notes
 * - BMI330 is a 3.3V device. Use level shifting if your MCU is 5V.
 * - I2C address is typically 0x68 or 0x69 depending on SDO/SA0 wiring.
 * - This example uses beginI2C(0x68, Wire, 400000). Change address if needed.
 */

#include <Wire.h>
#include <7Semi_BMI330.h>

BMI330_7Semi imu;

static const uint8_t BMI330_ADDR = 0x68;  // change to 0x69 if your board uses that address

void setup()
{
  Serial.begin(115200);
  delay(200);

  /**
   * Initialize BMI330 over I2C
   * - address: BMI330_ADDR (0x68 or 0x69)
   * - wire:    Wire
   * - clock:   400kHz fast-mode
   */
  if (!imu.beginI2C(BMI330_ADDR, Wire, 400000))
  {
    // Avoid printing extra text continuously; one-time error is fine
    Serial.println("BMI330 beginI2C failed");
    while (1) { delay(100); }
  }

  /**
   * Print a single header line for humans (optional).
   * - Arduino Serial Plotter may show it briefly, but plots will still work
   *   once numeric lines begin.
   * - If you want strictly numeric output only, comment this out.
   */
  Serial.println("ax ay az gx gy gz t");
}

void loop()
{
  float ax, ay, az;
  float gx, gy, gz;
  float t;

  /**
   * Read sensors
   * - For Serial Plotter, it is better to always output the same number of fields.
   * - If a read fails, we output zeros so the plot stays stable.
   */
  bool okA = imu.readAccel(ax, ay, az);
  bool okG = imu.readGyro(gx, gy, gz);
  bool okT = imu.readTemperatureC(t);

  if (!okA) { ax = 0; ay = 0; az = 0; }
  if (!okG) { gx = 0; gy = 0; gz = 0; }
  if (!okT) { t = 0; }

  /**
   * Serial Plotter output (numbers only)
   * - Fields are space-separated:
   *   ax ay az gx gy gz t
   */
  Serial.print(ax, 6); Serial.print(' ');
  Serial.print(ay, 6); Serial.print(' ');
  Serial.print(az, 6); Serial.print(' ');
  Serial.print(gx, 3); Serial.print(' ');
  Serial.print(gy, 3); Serial.print(' ');
  Serial.print(gz, 3); Serial.print(' ');
  Serial.println(t, 2);

  /**
   * Plot update rate
   * - 20 ms -> 50 lines/sec
   * - If the plot feels noisy/fast, increase this delay.
   */
  delay(20);
}