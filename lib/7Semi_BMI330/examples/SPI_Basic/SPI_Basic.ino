/**
 * SPI_Basic.ino — 7Semi BMI330 basic example over SPI
 *
 * What this does
 * - Starts the BMI330 over SPI (CS pin + SPI bus)
 * - Continuously reads accelerometer, gyroscope, and temperature
 * - Prints values to the Serial Monitor
 *
 * Wiring (SPI)
 * - VDD        -> 3V3
 * - GND        -> GND
 * - SCK (SCL)  -> SCK
 * - MISO (SDO) -> MISO
 * - MOSI (SDA) -> MOSI
 * - CS         -> Any GPIO (set below)
 *
 * Notes
 * - BMI330 is a 3.3V device. Use level shifting if your MCU is 5V.
 * - BMI330 SPI is typically MODE0 (CPOL=0, CPHA=0).
 * - Keep SPI wires short and use a solid ground reference for stable reads.
 * - Prefer using the board’s built-in SPI pins (SCK/MOSI/MISO) unless your MCU supports remapping.
 */

#include <7Semi_BMI330.h>

BMI330_7Semi imu;

/**
 * SPI pin notes
 * - On classic AVR Arduino boards (UNO/Nano):
 *   - SCK  = 13
 *   - MISO = 12
 *   - MOSI = 11
 *   - CS   = 10 (but can be any GPIO)
 *
 * - On ESP32:
 *   - SPI pins can be remapped, but it depends on the core/board.
 *   - If you want custom pins, do it using SPI.begin(SCK, MISO, MOSI, CS)
 *     and then call imu.beginSPI(CS, SPI, clock).
 *
 * Important
 * - Do not define SCK/MOSI/MISO macros unless you really need them.
 *   It can conflict with core definitions on some platforms.
 */

// ---- Choose your CS pin here ----
static const uint8_t BMI330_CS = 10;  // AVR default CS pin (change if required)

// ---- Optional: ESP32 pin mapping example (uncomment and edit if needed) ----
// static const int8_t PIN_SCK  = 18;
// static const int8_t PIN_MISO = 19;
// static const int8_t PIN_MOSI = 23;
// static const int8_t PIN_CS   = 5;    // can match BMI330_CS

void setup()
{
  Serial.begin(115200);
  delay(200);


  /**
   * Initialize BMI330 over SPI.
   * - csPin:  BMI330_CS
   * - spi:    SPI instance
   * - clock:  SPI clock in Hz (10 MHz here)
   *
   * Returns false if:
   * - Chip ID check fails
   * - CS pin / SPI wiring is wrong
   * - Power or ground is unstable
   */
  //  if (!imu.beginSPI(BMI330_CS, SPI, 10000000,PIN_SCK, PIN_MISO, PIN_MOSI))
  if (!imu.beginSPI(BMI330_CS, SPI, 10000000))
  {
    Serial.println("beginSPI failed");
    while (1) { delay(100); }
  }

  Serial.println("BMI330 SPI OK");
}

void loop()
{
  float ax, ay, az;
  float gx, gy, gz;
  float t;

  /**
   * Read measurements
   * - readAccel():         acceleration in g
   * - readGyro():          angular rate in dps
   * - readTemperatureC():  temperature in degrees Celsius
   *
   * If any read fails, print "read failed".
   */
  if (imu.readAccel(ax, ay, az) && imu.readGyro(gx, gy, gz) && imu.readTemperatureC(t))
  {
    Serial.print("A[g] ");
    Serial.print(ax, 3); Serial.print(", ");
    Serial.print(ay, 3); Serial.print(", ");
    Serial.print(az, 3);

    Serial.print("  G[dps] ");
    Serial.print(gx, 2); Serial.print(", ");
    Serial.print(gy, 2); Serial.print(", ");
    Serial.print(gz, 2);

    Serial.print("  T[C] ");
    Serial.println(t, 2);
  }
  else
  {
    Serial.println("read failed");
  }

  /**
   * Delay between prints
   * - 200 ms = 5 updates per second.
   * - Reduce delay for faster updates, increase for cleaner serial logs.
   */
  delay(200);
}