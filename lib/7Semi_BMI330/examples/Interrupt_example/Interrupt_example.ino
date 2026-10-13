/**
 * Interrupt_Example.ino — 7Semi BMI330 motion + data-ready status polling (INT1)
 *
 * What this does
 * - Initializes BMI330 over I2C
 * - Enables the feature engine (needed for Any-Motion / No-Motion features)
 * - Routes motion interrupts to INT1
 * - (Optional) Maps Accel/Gyro/Temp data-ready to INT1
 * - Polls INT_STATUS_INT1 and prints which events occurred
 *
 * Wiring (I2C + INT1)
 * - VDD   -> 3V3
 * - GND   -> GND
 * - SDA   -> SDA
 * - SCL   -> SCL
 * - INT1  -> Any GPIO input (optional, only if you want a physical interrupt pin)
 *
 * Notes
 * - BMI330 is a 3.3V device. Use level shifting if your MCU is 5V.
 * - The I2C address is usually 0x68 or 0x69 depending on SDO/SA0 wiring.
 * - This example polls INT_STATUS_INT1 over I2C.
 *   You can ALSO wire INT1 to a GPIO and use attachInterrupt() if desired.
 */

#include <7Semi_BMI330.h>

BMI330_7Semi imu;

/**
 * INT_STATUS_INT1 bit masks
 * - These bit positions match the library’s INT_STATUS_INT1 register layout.
 */
#define INT_NO_MOTION   (1u << 0)
#define INT_ANY_MOTION  (1u << 1)
#define INT_TMP_DRDY    (1u << 11)
#define INT_GYR_DRDY    (1u << 12)
#define INT_ACC_DRDY    (1u << 13)

void setup()
{
  Serial.begin(115200);

  while (!Serial) { delay(10); }

  /**
   * Start I2C bus.
   * - On most Arduino boards, SDA/SCL pins are fixed (use the board’s SDA/SCL pins).
   * - On ESP32 you may be able to choose pins in Wire.begin(sda, scl).
   */
  Wire.begin();

  /**
   * Initialize BMI330 over I2C.
   * - Uses library defaults (address + Wire instance).
   * - Fails if chip ID check fails or the sensor is not detected.
   */
  if (!imu.beginI2C())
  {
    Serial.println("BMI330 beginI2C failed");
    while (1) delay(1000);
  }
  delay(1000);

  /**
   *  Enable the BMI330 feature engine
   * - Required for motion features (Any-Motion / No-Motion).
   * - The second argument is a timeout in ms while waiting for ready state.
   */
  if (!imu.enableFeatureEngine(true, 200))
  {
    Serial.println("Feature engine not ready!");
  }

  /**
   *  Enable motion features
   * - Arguments depend on your library implementation.
   * - This call enables motion detections/features that drive the feature interrupts.
   */
  imu.enableMotion(true, true, true, true, true, true);

  /**
   *  Route feature interrupts to INT1
   * - Motion outputs are produced by the feature engine.
   * - Here we map Any-Motion and No-Motion to INT1 output.
   */
  imu.setAnyMotionInt(BMI330_7Semi::FEAT_INT_INT1);
  imu.setNoMotionInt(BMI330_7Semi::FEAT_INT_INT1);

  /**
   *  Configure INT1 pin electrical behavior
   * configInt1Pin(activeHigh, pushPull, outputEnable)
   *   - activeHigh: true  -> INT1 goes HIGH when asserted
   *   - pushPull:   true  -> push-pull (false mean open-drain)
   *   - outputEnable: if true interrupt enable else disable
   *
   * configIntLatch(true)
   * - Latched interrupts stay asserted until status is read/cleared.
   * - Helpful when you are polling instead of using a GPIO interrupt.
   */
  imu.configInt1Pin(true, true, false);
  imu.configIntLatch(true);

  /**
   *  Optional but recommended: map data-ready to INT1 too
   * - If your library supports mapBasicInterrupts(), you can route DRDY bits to INT1.
   * - If you do NOT map DRDY, the ACC/GYR/TMP data-ready bits may never appear
   *   in INT_STATUS_INT1 even though motion interrupts do.
   */
  imu.mapBasicInterrupts(
    BMI330_7Semi::INT_DISABLED,  // fifoFull
    BMI330_7Semi::INT_DISABLED,  // fifoWm
    BMI330_7Semi::INT_TO_INT1,   // accelDrdy -> INT1
    BMI330_7Semi::INT_TO_INT1,   // gyroDrdy  -> INT1
    BMI330_7Semi::INT_TO_INT1,   // tempDrdy  -> INT1
    BMI330_7Semi::INT_DISABLED,  // errStatus
    BMI330_7Semi::INT_DISABLED   // tapOut
  );

  Serial.println("Started: motion + data-ready status polling");
}

void loop()
{
  uint16_t st = 0;

  /**
   * Read the INT1 interrupt status register.
   * - Even if INT1 is not physically connected, the status register still updates.
   */
  imu.readIntStatusInt1(st);

  // /**
  //  * Print raw status when any bit is set.
  //  * - Useful for debugging bit mapping and confirming interrupt routing.
  //  */
  // if (st)
  // {
  //   Serial.print("INT_STATUS_INT1 = 0x");
  //   Serial.println(st, HEX);
  // }

  /**
   * Motion events
   * - Any-Motion triggers when movement crosses configured thresholds.
   * - No-Motion triggers after the sensor detects stillness for the configured duration.
   */
  if (st & INT_ANY_MOTION)
  {
    Serial.println("ANY MOTION!");
  }
  if (st & INT_NO_MOTION)
  {
    Serial.println("NO MOTION!");
  }

  /**
   * Data-ready events
   * - These bits typically indicate a new sample is available.
   * - After seeing the bit, read the corresponding sensor data.
   */
  if (st & INT_ACC_DRDY)
  {
    float ax, ay, az;
    imu.readAccel(ax, ay, az);

    Serial.print("ACC ");
    Serial.print(ax, 6); Serial.print(' ');
    Serial.print(ay, 6); Serial.print(' ');
    Serial.println(az, 6);
  }

  if (st & INT_GYR_DRDY)
  {
    float gx, gy, gz;
    imu.readGyro(gx, gy, gz);

    Serial.print("GYR ");
    Serial.print(gx, 6); Serial.print(' ');
    Serial.print(gy, 6); Serial.print(' ');
    Serial.println(gz, 6);
  }

  if (st & INT_TMP_DRDY)
  {
    float tc;
    imu.readTemperatureC(tc);

    Serial.print("Temperature: ");
    Serial.println(tc, 2);
  }

  Serial.println();
  delay(50);
}