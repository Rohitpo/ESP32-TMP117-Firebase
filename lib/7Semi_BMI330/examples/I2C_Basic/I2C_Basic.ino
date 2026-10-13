/**
 * Basic_Read.ino — 7Semi BMI330 basic read example
 *
 * What this does
 * - Initializes the BMI330 using the library default I2C settings
 * - Continuously reads accelerometer, gyroscope, and temperature
 * - Prints values to the Serial Monitor
 *
 * Wiring (I2C)
 * - VDD  -> 3V3
 * - GND  -> GND
 * - SDA  -> SDA
 * - SCL  -> SCL
 *
 * Notes
 * - The default I2C address is usually 0x68 (or 0x69 if SDO/SA0 is pulled HIGH).
 * - This sketch uses imu.begin() which relies on the library’s default address.
 */

#include <7Semi_BMI330.h>

BMI330_7Semi imu; 

void setup()
{
  Serial.begin(115200);

  while (!Serial) {}

  /**
   * Initialize BMI330 on I2C.
   * - Uses default address from the library (typically 0x68).
   * - Returns false if the sensor is not detected / chip ID check fails.
   */
  if (!imu.begin())
  {
    Serial.println("BMI330 begin() failed");
    while (1) {}
  }

  Serial.println("BMI330 OK");
}

void loop()
{
  float ax, ay, az;
  float gx, gy, gz;
  float t;

  /**
   * Read all three measurements.
   * - readAccel():         acceleration in g
   * - readGyro():          angular rate in dps
   * - readTemperatureC():  temperature in degrees Celsius
   *
   * If any read fails, nothing is printed for this cycle.
   */
  if (imu.readAccel(ax, ay, az) && imu.readGyro(gx, gy, gz) && imu.readTemperatureC(t))
  {
    Serial.print("A[g] ");
    Serial.print(ax, 4); Serial.print(", ");
    Serial.print(ay, 4); Serial.print(", ");
    Serial.print(az, 4);

    Serial.print(" | G[dps] ");
    Serial.print(gx, 2); Serial.print(", ");
    Serial.print(gy, 2); Serial.print(", ");
    Serial.print(gz, 2);

    Serial.print(" | T[C] ");
    Serial.println(t, 2);
  }

  /**
   * 50 ms delay = ~20 prints/second.
   * Increase delay if you want fewer prints and a cleaner serial log.
   */
  delay(50);
}