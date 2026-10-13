#include "7Semi_BMI330.h"

BMI330_7Semi::BMI330_7Semi() {}


/**
 * - Initialize BMI330 using the selected bus (I2C or SPI).
 * - If no bus is selected, it defaults to I2C with default parameters.
 * - Performs a CHIP_ID check, soft reset, and applies a safe default configuration.
 *
 * Notes:
 *   - Accel: 100 Hz, ±2 g, BW=ODR/2, AVG=64, High Performance
 *   - Gyro : 100 Hz, ±2000 dps, BW=ODR/2, AVG=64, High Performance
 *
 * Returns:
 * - true  : device detected and configured successfully
 * - false : bus init failed, CHIP_ID mismatch, reset failed, or configuration failed
 */
bool BMI330_7Semi::begin() {
  // If bus not selected, default to I2C
  if (bus == Bus::NONE) {
    if (!beginI2C())
      return false;
  }

  uint16_t chip = 0;
  if (!readChipID(chip))
    return false;

  /* Important:
   * - Only compare lower byte of CHIP_ID.
   * - Use parentheses to avoid operator precedence bugs.
   */
  if (((chip & 0xFF) != (BMI330_CHIP_ID_VALUE & 0xFF)))
    return false;

  if (!softReset())
    return false;
  delay(10);

  // Default configuration
  if (!setAccelConfig(ODR_100HZ, ACC_RANGE_2G, BW_ODR_DIV2, AVG_64, MODE_HIGH_PERFORMANCE))
    return false;
  if (!setGyroConfig(ODR_100HZ, GYR_RANGE_2000DPS, BW_ODR_DIV2, AVG_64, MODE_HIGH_PERFORMANCE))
    return false;

  return true;
}

/**
 * - Select and initialize BMI330 on I2C.
 * - Sets internal driver state (bus type, address, Wire port, I2C clock speed).
 * - Starts the I2C peripheral and verifies that the device ACKs the address.
 * - Calls begin() to run CHIP_ID check + soft reset + default configuration.
 *
 * Parameters:
 * - i2cAddress    : 7-bit I2C address (common: 0x68 or 0x69 depending on SDO)
 * - i2CPort       : Wire instance to use (Wire, Wire1, etc.)
 * - ClockSpeed    : I2C clock in Hz (typical: 100000, 400000)
 * - SDA / SCL     : ESP32/ESP8266 pin selection (ignored on most other platforms)
 *
 * Notes:
 * - This function should not call itself again internally (avoid recursion loops).
 * - Address check uses a quick beginTransmission/endTransmission to confirm presence.
 * - If you want “only bus init without configuring sensor”, do not call begin().
 *
 * Returns:
 * - true  : I2C bus started, device ACKed, and begin() completed successfully
 * - false : I2C did not ACK, bus init failed, or begin() failed
 */
bool BMI330_7Semi::beginI2C(uint8_t i2cAddress,
                            TwoWire &i2CPort,
                            uint32_t ClockSpeed,
                            uint8_t SDA,
                            uint8_t SCL) {
  bus = Bus::I2C;

  address = i2cAddress;
  i2c = &i2CPort;
  i2c_clock_speed = ClockSpeed;

#if defined(ARDUINO_ARCH_ESP32) || defined(ARDUINO_ARCH_ESP8266)
  i2c->setPins(SDA, SCL);
  i2c->begin();
#else
  i2c->begin();
#endif

  i2c->setClock(i2c_clock_speed);

  i2c->beginTransmission(address);
  if (i2c->endTransmission(true) != 0)
    return false;

  // IMPORTANT: do NOT call beginI2C again (avoid recursion)
  return begin();
}

/**
 * - Select and initialize BMI330 on SPI.
 * - Sets internal driver state (bus type, CS pin, SPI port, SPI clock speed).
 * - Configures CS pin as output and deasserts it (HIGH).
 * - Starts the SPI peripheral.
 * - Calls begin() to run CHIP_ID check + soft reset + default configuration.
 *
 * Parameters:
 * - spiCS         : chip select pin
 * - spiPort       : SPI instance to use (SPI, SPI1, etc.)
 * - ClockSpeed    : SPI clock in Hz (keep conservative for long wires / noisy environments)
 * - sck/miso/mosi : ESP32 pin selection (ignored on most other platforms)
 *
 * Notes:
 * - Some BMI330 setups are more reliable if you perform a “dummy read” after power-up
 *   before validating CHIP_ID. If you see occasional SPI bring-up failures, add:
 *   - one throw-away readChipID() before the real check in begin().
 * - Keep CS HIGH when idle. Always toggle CS LOW/HIGH around every SPI transfer.
 *
 * Returns:
 * - true  : SPI started and begin() completed successfully
 * - false : begin() failed (CHIP_ID mismatch, reset/config failure, etc.)
 */
bool BMI330_7Semi::beginSPI(uint8_t spiCS,
                            SPIClass &spiPort,
                            uint32_t ClockSpeed,
                            uint8_t sck,
                            uint8_t miso,
                            uint8_t mosi) {
  bus = Bus::SPI;

  cs = spiCS;
  spi = &spiPort;
  spi_clock_speed = ClockSpeed;

  pinMode(cs, OUTPUT);
  digitalWrite(cs, HIGH);

#if defined(ARDUINO_ARCH_ESP32)
  spi->begin(sck, miso, mosi);
#else
  spi->begin();
#endif

  return begin();
}

/**
 * - Write a 16-bit value into consecutive registers (little-endian: LSB then MSB).
 * - Automatically selects I2C or SPI depending on the active bus.
 *
 * Notes:
 * - BMI330 uses little-endian ordering for 16-bit register pairs in most cases:
 *   - First byte written is LSB, second byte is MSB.
 * - This helper is meant for “register pairs” (2 bytes). For bigger writes, use writeBurst.
 *
 * Returns:
 * - true  : write completed successfully on the selected bus
 * - false : bus error, device NACK (I2C), or invalid state
 */
bool BMI330_7Semi::writeReg(uint8_t reg, uint16_t value) {
  uint8_t data[2] = { 0, 0 };
  data[0] = (uint8_t)(value & 0xFF);
  data[1] = (uint8_t)((value >> 8) & 0xFF);

  if (bus == Bus::I2C) {
    return i2cWrite(reg, data, 2);
  }

  return spiWrite(reg, data, 2);
}

/**
 * - Read a 16-bit register value from consecutive registers (little-endian).
 * - Uses readBurst() so the correct bus (I2C/SPI) and protocol timing is applied.
 *
 * Notes:
 * - The register pair is interpreted as:
 *   - value = (MSB << 8) | LSB
 * - This function assumes the device returns LSB first, then MSB.
 *
 * Returns:
 * - true  : read completed and 'value' updated
 * - false : bus error or device did not respond correctly
 */
bool BMI330_7Semi::readReg(uint8_t reg, uint16_t &value) {
  uint8_t buf[2] = { 0, 0 };
  if (!readBurst(reg, buf, 2))
    return false;

  value = ((uint16_t)buf[1] << 8) | (uint16_t)buf[0];
  return true;
}

/**
 * - Read multiple bytes starting from a register address.
 * - Automatically selects I2C or SPI depending on the active bus.
 *
 * Notes:
 * - For BMI330, standard I2C register reads include dummy bytes (handled in i2cRead()).
 * - For FIFO reads or special modes, ensure the underlying read method matches the required timing.
 *
 * Returns:
 * - true  : transfer completed successfully
 * - false : bus error or invalid parameters
 */
bool BMI330_7Semi::readBurst(uint8_t startReg, uint8_t *buffer, size_t length) {
  if ((buffer == nullptr) && (length > 0))
    return false;

  if (bus == Bus::I2C) {
    return i2cRead(startReg, buffer, length);
  }

  return spiRead(startReg, buffer, length);
}

/**
 * - Low-level I2C register read for BMI330.
 * - Writes the register address, then performs a repeated-start read.
 * - Discards BMI330's I2C dummy bytes before returning the actual payload.
 *
 * Notes:
 * - BMI330 I2C read response includes dummy bytes before valid data.
 * - Repeated-start is used (endTransmission(false)) so the bus is not released between
 *   register address write and read phase.
 * - If endTransmission(false) fails, do NOT continue; return false immediately.
 *
 * Parameters:
 * - reg : register address to read from
 * - buf : output buffer (must be valid when len > 0)
 * - len : number of valid payload bytes required (excluding dummy bytes)
 *
 * Returns:
 * - true  : read completed successfully and 'buf' contains payload data
 * - false : device NACK, bus error, or short read
 */
bool BMI330_7Semi::i2cRead(uint8_t reg, uint8_t *buf, size_t len) {
  if ((buf == nullptr) && (len > 0))
    return false;

  i2c->beginTransmission(address);
  i2c->write(reg);

  /* Critical:
   * - If this fails, the next requestFrom may return garbage.
   */
  if (i2c->endTransmission(false) != 0)
    return false;

  const size_t dummyBytes = 2;
  const size_t total = len + dummyBytes;

  if (i2c->requestFrom((int)address, (int)total) != (int)total)
    return false;

  for (size_t i = 0; i < dummyBytes; i++)
    (void)i2c->read();  // discard dummy bytes

  for (size_t i = 0; i < len; i++)
    buf[i] = (uint8_t)i2c->read();

  return true;
}

/**
 * - Low-level I2C register write for BMI330.
 * - Sends register address followed by 'len' data bytes.
 *
 * Notes:
 * - For multi-byte writes, the device will write consecutive registers.
 * - Always pass the correct 'len' value; do not hard-code 2 bytes.
 *
 * Parameters:
 * - reg : register address to write
 * - buf : data to write (must be valid when len > 0)
 * - len : number of bytes to write
 *
 * Returns:
 * - true  : device ACKed and the write finished successfully
 * - false : device NACK or bus error
 */
bool BMI330_7Semi::i2cWrite(uint8_t reg, const uint8_t *buf, size_t len) {
  if ((buf == nullptr) && (len > 0))
    return false;

  i2c->beginTransmission(address);
  i2c->write(reg);
  i2c->write(buf, len);
  return (i2c->endTransmission() == 0);
}

/**
 * - Low-level SPI register read for BMI330.
 * - Sends read command (MSB=1), performs dummy transfer, then reads 'len' bytes.
 *
 * Notes:
 * - BMI330 SPI read typically requires dummy cycles after the address phase.
 * - SPI mode is MODE0 and MSB-first for standard Bosch IMU SPI.
 * - Chip select must remain LOW for the full transfer.
 *
 * Parameters:
 * - reg : register address to read
 * - buf : output buffer (must be valid when len > 0)
 * - len : number of payload bytes to read
 *
 * Returns:
 * - true  : transfer completed (SPI has no ACK; caller must validate data if needed)
 * - false : invalid parameters
 */
bool BMI330_7Semi::spiRead(uint8_t reg, uint8_t *buf, size_t len) {
  if ((buf == nullptr) && (len > 0))
    return false;

  spi->beginTransaction(SPISettings(spi_clock_speed, MSBFIRST, SPI_MODE0));
  digitalWrite(cs, LOW);

  spi->transfer((uint8_t)(0x80 | (reg & 0x7F)));  // MSB=1 for read
  (void)spi->transfer(0x00);                      // dummy byte

  for (size_t i = 0; i < len; i++)
    buf[i] = spi->transfer(0x00);

  digitalWrite(cs, HIGH);
  spi->endTransaction();

  return true;
}

/**
 * - Low-level SPI register write for BMI330.
 * - Sends write command (MSB=0) and writes 'len' bytes to consecutive registers.
 *
 * Notes:
 * - SPI has no ACK; caller should read back critical registers if verification is required.
 * - Chip select must remain LOW for the full transfer.
 *
 * Parameters:
 * - reg : register address to write
 * - buf : data to write (must be valid when len > 0)
 * - len : number of bytes to write
 *
 * Returns:
 * - true  : transfer completed
 * - false : invalid parameters
 */
bool BMI330_7Semi::spiWrite(uint8_t reg, const uint8_t *buf, size_t len) {
  if ((buf == nullptr) && (len > 0))
    return false;

  spi->beginTransaction(SPISettings(spi_clock_speed, MSBFIRST, SPI_MODE0));
  digitalWrite(cs, LOW);

  spi->transfer((uint8_t)(reg & 0x7F));  // MSB=0 for write
  for (size_t i = 0; i < len; i++)
    spi->transfer(buf[i]);

  digitalWrite(cs, HIGH);
  spi->endTransaction();

  return true;
}


/**
 * - Send a soft reset command to BMI330.
 * - This returns only the bus write result (ACK/transfer ok).
 *
 * Notes:
 * - After soft reset, BMI330 needs a short delay before register accesses are stable.
 *
 * Returns:
 * - true  : command write completed on the active bus
 * - false : bus error or device did not ACK (I2C)
 */
bool BMI330_7Semi::softReset() {
  return writeReg(BMI330_REG_CMD, BMI330_CMD_SOFT_RESET);
}


/**
 * - Read the BMI330 CHIP_ID register word.
 * - This is used to confirm that the sensor is present and responding.
 *
 * Notes:
 * - Only the lower 8 bits are typically used for ID compare (chipId & 0xFF).
 * - The upper byte can be 0x00 depending on the device behavior.
 * - For SPI bring-up, a throw-away read before the real check can improve reliability
 *   on some boards (power-up timing / mode switching).
 *
 * Returns:
 * - true  : read completed and chipId updated
 * - false : bus error or short read
 */
bool BMI330_7Semi::readChipID(uint16_t &chipId) {
  if (!readReg(BMI330_REG_CHIP_ID, chipId))
    return false;
  return true;
}

/**
 * - Read the BMI330 STATUS register word.
 * - Useful to check data-ready flags, internal state, and whether the device is operational.
 *
 * Notes:
 * - STATUS is a bitfield. Do not treat it as a simple number.
 * - For best debugging, read STATUS and ERR_REG together when bring-up fails.
 *
 * Returns:
 * - true  : read completed and status updated
 * - false : bus error or short read
 */
bool BMI330_7Semi::readStatus(uint16_t &status) {
  return readReg(BMI330_REG_STATUS, status);
}

/**
 * - Read the BMI330 error register (ERR_REG) word.
 * - This is the first place to check if configuration writes or mode changes fail.
 *
 * Notes:
 * - ERR_REG is a bitfield. Each bit indicates a specific error condition.
 * - Clear behavior depends on the bit. Some bits clear on read, some on reset, etc.
 *
 * Returns:
 * - true  : read completed and error updated
 * - false : bus error or short read
 */
bool BMI330_7Semi::readError(uint16_t &error) {
  return readReg(BMI330_REG_ERR_REG, error);
}

/**
 * - Read one bit from a 16-bit register and return it as a boolean.
 * - This is a small helper for checking status/error/config flags.
 *
 * Notes:
 * - BMI330 registers used here are 16-bit words (LSB+MSB).
 * - bitPos must be 0..15 (out of range bit positions are not valid).
 * - This is a safe helper for “read-only flags”.
 *
 * Returns:
 * - true  : register read succeeded and bitValue is updated
 * - false : bus read failed or bitPos is out of range
 */
bool BMI330_7Semi::readBit(uint8_t reg, uint8_t bitPos, bool &bitValue) {
  if (bitPos > 15)
    return false;

  uint16_t v = 0;
  if (!readReg(reg, v))
    return false;

  bitValue = (((v >> bitPos) & 0x01u) != 0u);
  return true;
}

/**
 * - Write one bit inside a 16-bit register (read-modify-write).
 * - Useful for enabling/disabling a single feature bit.
 *
 * Notes:
 * - This performs a read-modify-write sequence:
 *   - read register word
 *   - set/clear one bit
 *   - write full 16-bit word back
 * - Not interrupt-safe by default:
 *   - If an ISR or another task writes the same register at the same time,
 *     one update may overwrite the other.
 * - bitPos must be 0..15.
 *
 * Returns:
 * - true  : register updated successfully
 * - false : bus error or bitPos out of range
 */
bool BMI330_7Semi::writeBit(uint8_t reg, uint8_t bitPos, bool bitVal) {
  if (bitPos > 15)
    return false;

  uint16_t v = 0;
  if (!readReg(reg, v))
    return false;

  const uint16_t mask = (uint16_t)(1u << bitPos);

  if (bitVal)
    v |= mask;
  else
    v &= (uint16_t)(~mask);

  return writeReg(reg, v);
}

/**
 * - Update a multi-bit field inside a 16-bit register (read-modify-write).
 * - This is safer than writing raw values because it preserves unrelated bits.
 *
 * Notes:
 * - pos/width define the field location inside the 16-bit word.
 * - fieldValue is automatically masked to the field width.
 * - Use this for ODR/range/bandwidth/average fields to avoid accidental bit changes.
 *
 * Returns:
 * - true  : field updated successfully
 * - false : invalid width/position or bus error
 */
bool BMI330_7Semi::updateField(uint8_t reg, uint8_t pos, uint8_t width, uint16_t fieldValue) {
  if ((width == 0) || (width > 16))
    return false;
  if (pos > 15)
    return false;
  if ((pos + width) > 16)
    return false;

  uint16_t v = 0;
  if (!readReg(reg, v))
    return false;

  const uint16_t mask = (uint16_t)(((1u << width) - 1u) << pos);
  const uint16_t field = (uint16_t)((fieldValue << pos) & mask);

  v = (uint16_t)((v & (uint16_t)(~mask)) | field);
  return writeReg(reg, v);
}

/**
 * - Configure accelerometer output data rate, range, bandwidth, averaging and power mode.
 * - Updates the internal accel_range so later scaling functions can convert raw -> g.
 *
 * Field layout (verify with BMI330 register map):
 * - ODR   : bits [3:0]   (4 bits)
 * - RANGE : bits [6:4]   (3 bits)
 * - BW    : bit  [7]     (1 bit)
 * - AVG   : bits [10:8]  (3 bits)
 * - MODE  : bits [14:12] (3 bits)
 *
 * Notes:
 * - All fields are masked before writing to prevent accidental corruption
 *   if an enum value is out of range.
 * - This function performs a read-modify-write to preserve unrelated bits.
 * - For industrial reliability, consider verifying by reading the register back
 *   (optional, can be behind a macro to save flash).
 *
 * Returns:
 * - true  : configuration applied successfully
 * - false : register read/write failed
 */
bool BMI330_7Semi::setAccelConfig(ODR odr, AccRange range, BW bw, AVG avg, Mode mode) {
  accel_range = range;

  uint16_t v = 0;
  if (!readReg(BMI330_REG_ACC_CONF, v))
    return false;

  v &= (uint16_t) ~((uint16_t)0x000F << BMI330_ODR_BIT);
  v |= (uint16_t)(((uint16_t)odr & 0x0F) << BMI330_ODR_BIT);

  v &= (uint16_t) ~((uint16_t)0x0007 << BMI330_RANGE_BIT);
  v |= (uint16_t)(((uint16_t)range & 0x07) << BMI330_RANGE_BIT);

  v &= (uint16_t) ~((uint16_t)0x0001 << BMI330_BW_BIT);
  v |= (uint16_t)(((uint16_t)bw & 0x01) << BMI330_BW_BIT);

  v &= (uint16_t) ~((uint16_t)0x0007 << BMI330_AVG_BIT);
  v |= (uint16_t)(((uint16_t)avg & 0x07) << BMI330_AVG_BIT);

  v &= (uint16_t) ~((uint16_t)0x0007 << BMI330_MODE_BIT);
  v |= (uint16_t)(((uint16_t)mode & 0x07) << BMI330_MODE_BIT);

  if (!writeReg(BMI330_REG_ACC_CONF, v))
    return false;

  return true;
}

/**
 * - Configure gyroscope output data rate, range, bandwidth, averaging and power mode.
 * - Updates the internal gyro_range so later scaling functions can convert raw -> dps.
 *
 * Field layout (verify with BMI330 register map):
 * - ODR   : bits [3:0]   (4 bits)
 * - RANGE : bits [6:4]   (3 bits)
 * - BW    : bit  [7]     (1 bit)
 * - AVG   : bits [10:8]  (3 bits)
 * - MODE  : bits [14:12] (3 bits)
 *
 * Notes:
 * - All fields are masked before writing.
 * - Read-modify-write preserves unrelated bits.
 *
 * Returns:
 * - true  : configuration applied successfully
 * - false : register read/write failed
 */
bool BMI330_7Semi::setGyroConfig(ODR odr, GyrRange range, BW bw, AVG avg, Mode mode) {
  gyro_range = range;

  uint16_t v = 0;
  if (!readReg(BMI330_REG_GYR_CONF, v))
    return false;

  v &= (uint16_t) ~((uint16_t)0x000F << BMI330_ODR_BIT);
  v |= (uint16_t)(((uint16_t)odr & 0x0F) << BMI330_ODR_BIT);

  v &= (uint16_t) ~((uint16_t)0x0007 << BMI330_RANGE_BIT);
  v |= (uint16_t)(((uint16_t)range & 0x07) << BMI330_RANGE_BIT);

  v &= (uint16_t) ~((uint16_t)0x0001 << BMI330_BW_BIT);
  v |= (uint16_t)(((uint16_t)bw & 0x01) << BMI330_BW_BIT);

  v &= (uint16_t) ~((uint16_t)0x0007 << BMI330_AVG_BIT);
  v |= (uint16_t)(((uint16_t)avg & 0x07) << BMI330_AVG_BIT);

  v &= (uint16_t) ~((uint16_t)0x0007 << BMI330_MODE_BIT);
  v |= (uint16_t)(((uint16_t)mode & 0x07) << BMI330_MODE_BIT);

  if (!writeReg(BMI330_REG_GYR_CONF, v))
    return false;

  return true;
}

/**
 * - Read accelerometer in units of g (gravity).
 * - Internally reads raw 16-bit samples and converts them using the current accel range.
 *
 * Notes:
 * - Conversion uses the driver sensitivity helper:
 *   - lsbPerMg = accelLsbPerMg()
 *   - mg = raw / lsbPerMg
 *   - g  = mg / 1000
 * - If accel_range was not configured, conversion may be incorrect.
 *
 * Returns:
 * - true  : read succeeded and ax_g/ay_g/az_g updated
 * - false : raw read failed or scaling is invalid
 */
bool BMI330_7Semi::readAccel(float &ax_g, float &ay_g, float &az_g) {
  int16_t ax, ay, az;
  if (!readAccelRaw(ax, ay, az))
    return false;

  const float lsbPerMg = accelLsbPerMg();
  if (lsbPerMg <= 0.0f)
    return false;

  ax_g = ((float)ax / lsbPerMg) / 1000.0f;
  ay_g = ((float)ay / lsbPerMg) / 1000.0f;
  az_g = ((float)az / lsbPerMg) / 1000.0f;
  return true;
}

/**
 * - Read gyroscope in units of degrees per second (dps).
 * - Internally reads raw 16-bit samples and converts them using the current gyro range.
 *
 * Notes:
 * - Conversion uses the driver sensitivity helper:
 *   - lsbPerDps = gyroLsbPerDps()
 *   - dps = raw / lsbPerDps
 *
 * Returns:
 * - true  : read succeeded and gx_dps/gy_dps/gz_dps updated
 * - false : raw read failed or scaling is invalid
 */
bool BMI330_7Semi::readGyro(float &gx_dps, float &gy_dps, float &gz_dps) {
  int16_t gx, gy, gz;
  if (!readGyroRaw(gx, gy, gz))
    return false;

  const float lsbPerDps = gyroLsbPerDps();
  if (lsbPerDps <= 0.0f)
    return false;

  gx_dps = (float)gx / lsbPerDps;
  gy_dps = (float)gy / lsbPerDps;
  gz_dps = (float)gz / lsbPerDps;
  return true;
}

/**
 * - Read temperature and return degrees Celsius.
 * - Internally reads raw temperature register and applies BMI330 scaling.
 *
 * Notes:
 * - Conversion rule used here:
 *   - tempC = (raw / 256) + 23
 * - This assumes the standard BMI330 temperature format where:
 *   - 0x0000 corresponds to 23°C
 *   - 256 LSB corresponds to 1°C
 * - If you need full control, use readTemperatureRaw() and apply your own scaling.
 *
 * Returns:
 * - true  : read succeeded and tempC updated
 * - false : raw read failed
 */
bool BMI330_7Semi::readTemperatureC(float &tempC) {
  int16_t tRaw;
  if (!readTemperatureRaw(tRaw))
    return false;

  tempC = ((float)tRaw / 256.0f) + 23.0f;
  return true;
}

/**
 * - Read raw accelerometer output (X, Y, Z) as signed 16-bit values.
 *
 * Notes:
 * - Output registers are read in one burst for consistency.
 * - Data is little-endian per axis: LSB first, then MSB.
 * - Raw values must be scaled using the currently configured accel_range.
 *
 * Returns:
 * - true  : read succeeded, ax/ay/az updated
 * - false : bus read failed
 */
bool BMI330_7Semi::readAccelRaw(int16_t &ax, int16_t &ay, int16_t &az) {
  uint8_t buf[6] = { 0 };
  if (!readBurst(BMI330_REG_ACC_DATA_X, buf, sizeof(buf)))
    return false;

  ax = (int16_t)(((uint16_t)buf[1] << 8) | (uint16_t)buf[0]);
  ay = (int16_t)(((uint16_t)buf[3] << 8) | (uint16_t)buf[2]);
  az = (int16_t)(((uint16_t)buf[5] << 8) | (uint16_t)buf[4]);

  if (enable_smooth_accel) smooth_accel.update(ax, ay, az);

  return true;
}

/**
 * - Read raw gyroscope output (X, Y, Z) as signed 16-bit values.
 *
 * Notes:
 * - Output registers are read in one burst for consistency.
 * - Data is little-endian per axis: LSB first, then MSB.
 * - Raw values must be scaled using the currently configured gyro_range.
 *
 * Returns:
 * - true  : read succeeded, gx/gy/gz updated
 * - false : bus read failed
 */
bool BMI330_7Semi::readGyroRaw(int16_t &gx, int16_t &gy, int16_t &gz) {
  uint8_t buf[6] = { 0 };
  if (!readBurst(BMI330_REG_GYR_DATA_X, buf, sizeof(buf)))
    return false;

  gx = (int16_t)(((uint16_t)buf[1] << 8) | (uint16_t)buf[0]);
  gy = (int16_t)(((uint16_t)buf[3] << 8) | (uint16_t)buf[2]);
  gz = (int16_t)(((uint16_t)buf[5] << 8) | (uint16_t)buf[4]);

  if (enable_smooth_gyro) smooth_gyro.update(gx, gy, gz);

  return true;
}

/**
 * - Read raw temperature sensor value as signed 16-bit.
 *
 * Notes:
 * - Raw temperature must be converted using BMI330 temperature formula.
 * - Typical conversion:
 *   - T(°C) = (raw / 256) + 23
 * - Read in a burst to avoid partial register reads.
 *
 * Returns:
 * - true  : read succeeded and t updated
 * - false : bus read failed
 */
bool BMI330_7Semi::readTemperatureRaw(int16_t &t) {
  uint8_t buf[2] = { 0 };
  if (!readBurst(BMI330_REG_TEMP_DATA, buf, sizeof(buf)))
    return false;

  t = (int16_t)(((uint16_t)buf[1] << 8) | (uint16_t)buf[0]);
  return true;
}

/**
 * - Read the internal sensor time counter and return it as a 32-bit value.
 * - The counter is stored as two 16-bit words:
 *   - SENSOR_TIME_0 (0x0A) = bits [15:0]   (least significant word)
 *   - SENSOR_TIME_1 (0x0B) = bits [31:16]  (most significant word)
 *
 * Notes:
 * - 1 LSB corresponds to 39.0625 us (25.6 kHz tick).
 * - Read is done as one burst (4 bytes) so the value is consistent.
 * - Returned value is raw ticks. Convert to microseconds like:
 *   - us = ticks * 39.0625
 *   - For integer math: us ≈ (ticks * 390625) / 10000
 *
 * Returns:
 * - true  : read succeeded and time32 updated
 * - false : bus read failed
 */
bool BMI330_7Semi::readSensorTime(uint32_t &time32) {
  uint8_t buf[4] = { 0 };

  /* Read SENSOR_TIME_0 (LSW) and SENSOR_TIME_1 (MSW) in one transfer */
  if (!readBurst(BMI330_REG_SENSOR_TIME_0, buf, sizeof(buf)))
    return false;

  const uint16_t lsw = (uint16_t)(((uint16_t)buf[1] << 8) | (uint16_t)buf[0]);
  const uint16_t msw = (uint16_t)(((uint16_t)buf[3] << 8) | (uint16_t)buf[2]);

  time32 = ((uint32_t)msw << 16) | (uint32_t)lsw;
  return true;
}

/**
 * - Return accelerometer sensitivity as "LSB per mg" for the current range.
 * - Used to convert raw accel samples to mg or g without needing the datasheet.
 *
 * Notes:
 * - Conversion rule:
 *   - mg = raw / accelLsbPerMg()
 *   - g  = (raw / accelLsbPerMg()) / 1000
 * - The constants here assume a 16-bit signed output format with binary scaling:
 *   - ±2g  -> 16384 LSB/g -> 16.384 LSB/mg
 *   - ±4g  -> 8192  LSB/g -> 8.192  LSB/mg
 *   - ±8g  -> 4096  LSB/g -> 4.096  LSB/mg
 *   - ±16g -> 2048  LSB/g -> 2.048  LSB/mg
 * - If the sensor output format/range scaling differs, these values must be updated.
 *
 * Returns:
 * - Sensitivity in LSB/mg for the currently configured accel_range
 */
float BMI330_7Semi::accelLsbPerMg() const {
  switch (accel_range) {
    case ACC_RANGE_2G:
      return 16.384f;
    case ACC_RANGE_4G:
      return 8.192f;
    case ACC_RANGE_8G:
      return 4.10f;
    case ACC_RANGE_16G:
      return 2.05f;
    default:
      /* Safe fallback: assume ±2g if range is unknown */
      return 16.384f;
  }
}


/**
 * - Return gyroscope sensitivity as "LSB per dps" for the current range.
 * - Used to convert raw gyro samples to degrees/second without needing the datasheet.
 *
 * Notes:
 * - Conversion rule:
 *   - dps = raw / gyroLsbPerDps()
 * - The constants here follow binary scaling for a 16-bit signed output:
 *   - ±125 dps  -> 262.144 LSB/dps
 *   - ±250 dps  -> 131.072 LSB/dps
 *   - ±500 dps  -> 65.536  LSB/dps
 *   - ±1000 dps -> 32.768  LSB/dps
 *   - ±2000 dps -> 16.384  LSB/dps
 *
 * Returns:
 * - Sensitivity in LSB/dps for the currently configured gyro_range
 */
float BMI330_7Semi::gyroLsbPerDps() const {
  switch (gyro_range) {
    case GYR_RANGE_125DPS:
      return 262.144f;
    case GYR_RANGE_250DPS:
      return 131.072f;
    case GYR_RANGE_500DPS:
      return 65.536f;
    case GYR_RANGE_1000DPS:
      return 32.768f;
    case GYR_RANGE_2000DPS:
      return 16.4f;
    default:
      return 16.4f;
  }
}


/**
 * - Configure FIFO behavior and enable which sensor data is written into FIFO.
 *
 * Bits used in FIFO_CONF:
 * - bit0  fifo_stop_on_full : 0 = overwrite oldest, 1 = stop writing when full
 * - bit8  fifo_time_en      : include sensor time in FIFO frames
 * - bit9  fifo_acc_en       : include accel XYZ (3 words)
 * - bit10 fifo_gyr_en       : include gyro  XYZ (3 words)
 * - bit11 fifo_temp_en      : include temperature (1 word)
 *
 * Notes:
 * - Frame layout depends on which sources are enabled.
 * - Read-modify-write preserves unrelated bits.
 *
 * Returns:
 * - true  : register updated successfully
 * - false : bus read/write failed
 */
bool BMI330_7Semi::setFifoConfig(const FifoConfig &cfg) {
  uint16_t v = 0;
  if (!readReg(BMI330_REG_FIFO_CONF, v))
    return false;

  v &= (uint16_t) ~((uint16_t)1u << BMI330_FIFO_STOP_ON_FULL_BIT);
  v |= (uint16_t)((cfg.stop_on_full ? 1u : 0u) << BMI330_FIFO_STOP_ON_FULL_BIT);

  v &= (uint16_t) ~((uint16_t)1u << BMI330_FIFO_TIME_EN_BIT);
  v |= (uint16_t)((cfg.enable_time ? 1u : 0u) << BMI330_FIFO_TIME_EN_BIT);

  v &= (uint16_t) ~((uint16_t)1u << BMI330_FIFO_ACC_EN_BIT);
  v |= (uint16_t)((cfg.enable_accel ? 1u : 0u) << BMI330_FIFO_ACC_EN_BIT);

  v &= (uint16_t) ~((uint16_t)1u << BMI330_FIFO_GYR_EN_BIT);
  v |= (uint16_t)((cfg.enable_gyro ? 1u : 0u) << BMI330_FIFO_GYR_EN_BIT);

  v &= (uint16_t) ~((uint16_t)1u << BMI330_FIFO_TEMP_EN_BIT);
  v |= (uint16_t)((cfg.enable_temp ? 1u : 0u) << BMI330_FIFO_TEMP_EN_BIT);

  return writeReg(BMI330_REG_FIFO_CONF, v);
}

/**
 * - Set FIFO watermark level.
 *
 * Notes:
 * - Field width is 10 bits (0..1023).
 * - Value is masked to 10 bits to avoid writing reserved bits.
 *
 * Returns:
 * - true  : write succeeded
 * - false : bus write failed
 */
bool BMI330_7Semi::setFifoWatermark(uint16_t watermark) {
  watermark &= (uint16_t)BMI330_REG_FIFO_WATERMARK;
  return writeReg(BMI330_REG_FIFO_WATERMARK, watermark);
}

/**
 * - Read FIFO fill level .
 *
 * Notes:
 * - Field width is 11 bits (0..2047).
 * - Returned value is masked to 11 bits.
 *
 * Returns:
 * - true  : read succeeded and 'data' updated
 * - false : bus read failed
 */
bool BMI330_7Semi::getFifoFillLevel(uint16_t &data) {
  if (!readReg(BMI330_REG_FIFO_FILL_LEVEL, data))
    return false;

  data &= (uint16_t)BMI330_FIFO_FILL_LEVEL_MASK;
  return true;
}

/**
 * - Flush (clear) the FIFO data buffer.
 *
 * Notes:
 * - FIFO_CTRL.fifo_flush is bit0.
 * - Writing '1' clears the FIFO data buffer and resets the FIFO word counter.
 *
 * Returns:
 * - true  : flush command written successfully
 * - false : bus write failed
 */
bool BMI330_7Semi::flushFifo() {
  const uint16_t v = (uint16_t)(1u << BMI330_FIFO_FLUSH_BIT);
  return writeReg(BMI330_REG_FIFO_CTRL, v);
}
/**
 * - Read FIFO words into a uint16_t buffer.
 *
 * Notes:
 * - FIFO data is read as bytes and assembled into 16-bit words (LSB first).
 * - This avoids endian and alignment problems from reading directly into uint16_t*.
 *
 * Returns:
 * - true  : FIFO read completed
 * - false : bus read failed or dst is null
 */
bool BMI330_7Semi::readFifo(uint16_t *dst, size_t fifoCount) {
  if (fifoCount == 0)
    return true;
  if (dst == nullptr)
    return false;

  /* Read in small chunks to reduce stack usage */
  while (fifoCount > 0) {
    const size_t chunkWords = (fifoCount > 16) ? 16 : fifoCount;
    uint8_t buf[16 * 2] = { 0 };

    if (!readBurst(BMI330_REG_FIFO_DATA, buf, chunkWords * 2))
      return false;

    for (size_t i = 0; i < chunkWords; i++) {
      dst[i] = (uint16_t)(((uint16_t)buf[i * 2 + 1] << 8) | (uint16_t)buf[i * 2 + 0]);
    }

    dst += chunkWords;
    fifoCount -= chunkWords;
  }

  return true;
}


/**
 * - Read the SAT_FLAGS register.
 * - Saturation flags indicate that the sensor output clipped at the selected range.
 *
 * Notes:
 * - Bits (default axis config):
 *   - bit0 acc_x, bit1 acc_y, bit2 acc_z
 *   - bit3 gyr_x, bit4 gyr_y, bit5 gyr_z
 *
 * Returns:
 * - true  : read succeeded and flags updated
 * - false : bus read failed
 */
bool BMI330_7Semi::readSaturationFlagReg(uint16_t &flags) {
  return readReg(BMI330_REG_SATURATION_FLAG, flags);
}

/**
 * - Check whether accelerometer axis is saturated.
 *
 * Parameters:
 * - axis:
 *   - 0 = X, 1 = Y, 2 = Z
 *
 * Notes:
 * - Returns false if axis is out of range.
 *
 * Returns:
 * - true  : read succeeded and 'saturated' updated
 * - false : bus read failed or invalid axis
 */
bool BMI330_7Semi::isAccelSaturated(uint8_t axis, bool &saturated) {
  if (axis > 2)
    return false;

  uint16_t v = 0;
  if (!readReg(BMI330_REG_SATURATION_FLAG, v))
    return false;

  saturated = (((v >> axis) & 0x01u) != 0u);
  return true;
}

/**
 * - Check whether gyroscope axis is saturated.
 *
 * Parameters:
 * - axis:
 *   - 0 = X, 1 = Y, 2 = Z
 *
 * Notes:
 * - Gyro saturation bits follow accel bits:
 *   - bit3 = gyr_x, bit4 = gyr_y, bit5 = gyr_z
 * - Returns false if axis is out of range.
 *
 * Returns:
 * - true  : read succeeded and 'saturated' updated
 * - false : bus read failed or invalid axis
 */
bool BMI330_7Semi::isGyroSaturated(uint8_t axis, bool &saturated) {
  if (axis > 2)
    return false;

  uint16_t v = 0;
  if (!readReg(BMI330_REG_SATURATION_FLAG, v))
    return false;

  saturated = (((v >> (axis + 3)) & 0x01u) != 0u);
  return true;
}

/**
 * - Decode one FIFO data frame into a Sample structure.
 * - The frame layout depends on which sources are enabled in FIFO_CONF.
 *
 * Data sources and word counts (when enabled):
 * - accel : 3 words (ax, ay, az)
 * - gyro  : 3 words (gx, gy, gz)
 * - temp  : 1 word  (t)
 * - time  : size depends on FIFO format (verify: 1 word or 2 words)
 *
 * Notes:
 * - BMI330 FIFO frames are organized based on FIFO_CONF enable flags.
 * - This function currently reads FIFO_CONF from the device on every call.
 *   - Cache fifoConf when setFifoConfig() is called, or
 *   - Pass fifoConf / flags into decodeFifoFrame().
 * - Dummy frames:
 *   - Some FIFO formats generate dummy frames that must be skipped.
 *   - Dummy signatures are FIFO-format dependent and must match your chosen mode.
 *
 * Returns:
 * - true  : frame decoded successfully and 'out' updated
 * - false : frame is dummy/invalid, too short, or a bus/config issue occurred
 */
bool BMI330_7Semi::decodeFifoFrame(const uint16_t *dataFrame, size_t dataFrameCount, Sample &out) {
  out = Sample{};

  size_t idx = 0;

  uint16_t fifoConf = 0;
  if (!readReg(BMI330_REG_FIFO_CONF, fifoConf))
    return false;

  const bool accelEn = ((fifoConf >> 9) & 0x01u) != 0u;
  const bool gyroEn = ((fifoConf >> 10) & 0x01u) != 0u;
  const bool tempEn = ((fifoConf >> 11) & 0x01u) != 0u;
  const bool timeEn = ((fifoConf >> 8) & 0x01u) != 0u;

  const size_t needed =
    (accelEn ? 3 : 0) + (gyroEn ? 3 : 0) + (tempEn ? 1 : 0) + (timeEn ? 1 : 0); /* verify FIFO time width */

  if ((dataFrame == nullptr) || (dataFrameCount < needed))
    return false;

  /* Dummy frame signatures must match the chosen FIFO format */
  if ((accelEn && dataFrame[0] == 0x7F01u) || (gyroEn && dataFrame[0] == 0x7F02u) || (!accelEn && !gyroEn && tempEn && dataFrame[0] == 0x8000u)) {
    return false;
  }

  if (accelEn) {
    out.ax = (int16_t)dataFrame[idx + 0];
    out.ay = (int16_t)dataFrame[idx + 1];
    out.az = (int16_t)dataFrame[idx + 2];

    const float lsbPerMg = accelLsbPerMg();
    if (lsbPerMg <= 0.0f)
      return false;

    out.ax_g = ((float)out.ax / lsbPerMg) / 1000.0f;
    out.ay_g = ((float)out.ay / lsbPerMg) / 1000.0f;
    out.az_g = ((float)out.az / lsbPerMg) / 1000.0f;

    out.has_accel = true;
    idx += 3;
  }

  if (gyroEn) {
    out.gx = (int16_t)dataFrame[idx + 0];
    out.gy = (int16_t)dataFrame[idx + 1];
    out.gz = (int16_t)dataFrame[idx + 2];

    const float lsbPerDps = gyroLsbPerDps();
    if (lsbPerDps <= 0.0f)
      return false;

    out.gx_dps = (float)out.gx / lsbPerDps;
    out.gy_dps = (float)out.gy / lsbPerDps;
    out.gz_dps = (float)out.gz / lsbPerDps;

    out.has_gyro = true;
    idx += 3;
  }

  if (tempEn) {
    out.t = (int16_t)dataFrame[idx];
    out.has_temp = true;
    idx += 1;
  }

  if (timeEn) {
    /* Important:
     * - FIFO time width must be verified.
     * - If FIFO provides 32-bit time, consume 2 words here.
     */
    out.sensor_time = (uint32_t)dataFrame[idx];
    out.has_time = true;
    idx += 1;
  }

  return true;
}

/**
 * - Configure INT1 pin electrical behavior and enable/disable the output driver.
 *
 * Notes:
 * - This controls the pin characteristics only (polarity, open-drain, enable).
 * - Actual interrupt routing is configured in INT_MAP registers.
 *
 * Parameters:
 * - enable     : true to enable INT1 driver, false to disable
 * - activeHigh : true = active HIGH level, false = active LOW level
 * - openDrain  : true = open-drain output, false = push-pull output
 *
 * Returns:
 * - true  : configuration applied
 * - false : bus error (read-modify-write failed)
 */
bool BMI330_7Semi::configInt1Pin(bool enable, bool activeHigh, bool openDrain) {
  if (!writeBit(BMI330_REG_IO_INT_CTRL, 0, activeHigh))
    return false;
  if (!writeBit(BMI330_REG_IO_INT_CTRL, 1, openDrain))
    return false;
  if (!writeBit(BMI330_REG_IO_INT_CTRL, 2, enable))
    return false;

  return true;
}

/**
 * - Configure INT2 pin electrical behavior and enable/disable the output driver.
 *
 * Notes:
 * - Bit locations must match BMI330 IO_INT_CTRL register map.
 * - Uses bit positions [10:8] for INT2 (verify in register map).
 */
bool BMI330_7Semi::configInt2Pin(bool enable, bool activeHigh, bool openDrain) {
  if (!writeBit(BMI330_REG_IO_INT_CTRL, 8, activeHigh))
    return false;
  if (!writeBit(BMI330_REG_IO_INT_CTRL, 9, openDrain))
    return false;
  if (!writeBit(BMI330_REG_IO_INT_CTRL, 10, enable))
    return false;

  return true;
}

/**
 * - Configure interrupt latching behavior.
 *
 * Notes:
 * - When latched, the interrupt output stays asserted until status is cleared.
 * - When not latched, output is a pulse or level depending on device settings.
 *
 * Returns:
 * - true  : latch config updated successfully
 * - false : bus error
 */
bool BMI330_7Semi::configIntLatch(bool latched) {
  return writeBit(BMI330_REG_INT_CONF, BMI330_INT_LATCH_BIT, latched);
}

/**
 * - Map interrupts (FIFO + data-ready + error + tap) to INT1 / INT2 / I3C IBI.
 * - Uses INT_MAP2 register (0x3B).
 *
 * INT_MAP2 field layout (2-bit fields):
 * - tap_out            : bits [1:0]   (offset 0)
 * - i3c_out            : bits [3:2]   (offset 2)  (not set by this function)
 * - err_status         : bits [5:4]   (offset 4)
 * - temp_drdy_int      : bits [7:6]   (offset 6)
 * - gyr_drdy_int       : bits [9:8]   (offset 8)
 * - acc_drdy_int       : bits [11:10] (offset 10)
 * - fifo_watermark_int : bits [13:12] (offset 12)
 * - fifo_full_int      : bits [15:14] (offset 14)
 *
 * IntRoute values (2-bit):
 * - 0 = disabled
 * - 1 = INT1
 * - 2 = INT2
 * - 3 = I3C IBI
 *
 * Notes:
 * - This does one read-modify-write for speed (no repeated writeBit calls).
 * - Only the lowest 2 bits of each IntRoute are used.
 *
 * Returns:
 * - true  : mapping applied successfully
 * - false : bus read/write failed
 */
bool BMI330_7Semi::mapBasicInterrupts(IntRoute fifoFull,
                                      IntRoute fifoWm,
                                      IntRoute accelDrdy,
                                      IntRoute gyroDrdy,
                                      IntRoute tempDrdy,
                                      IntRoute errStatus,
                                      IntRoute tapOut) {
  uint16_t v = 0;
  if (!readReg(BMI330_REG_INT_MAP2, v))
    return false;

  /* Clear the fields we manage (keep i3c_out bits [3:2] untouched) */
  const uint16_t clearMask =
    (uint16_t)(0x3u << 0) |  /* tap_out */
    (uint16_t)(0x3u << 4) |  /* err_status */
    (uint16_t)(0x3u << 6) |  /* temp_drdy_int */
    (uint16_t)(0x3u << 8) |  /* gyr_drdy_int */
    (uint16_t)(0x3u << 10) | /* acc_drdy_int */
    (uint16_t)(0x3u << 12) | /* fifo_watermark_int */
    (uint16_t)(0x3u << 14);  /* fifo_full_int */

  v &= (uint16_t)~clearMask;

  /* Set new routes (mask to 2-bit) */
  v |= (uint16_t)(((uint16_t)tapOut & 0x3u) << 0);
  v |= (uint16_t)(((uint16_t)errStatus & 0x3u) << 4);
  v |= (uint16_t)(((uint16_t)tempDrdy & 0x3u) << 6);
  v |= (uint16_t)(((uint16_t)gyroDrdy & 0x3u) << 8);
  v |= (uint16_t)(((uint16_t)accelDrdy & 0x3u) << 10);
  v |= (uint16_t)(((uint16_t)fifoWm & 0x3u) << 12);
  v |= (uint16_t)(((uint16_t)fifoFull & 0x3u) << 14);

  return writeReg(BMI330_REG_INT_MAP2, v);
}

/**
 * - Configure feature-engine interrupt routing flags (INT_MAP1).
 *
 * Notes:
 * - This register typically contains multiple 2-bit routing fields for features
 *   like no-motion, any-motion, orientation, step detector, etc.
 * - For industrial reliability, do not clear reserved bits unless you know the mask.
 *
 * Returns:
 * - true  : mapping updated successfully
 * - false : bus error
 */
bool BMI330_7Semi::setFeatureInterrupts(uint16_t flags) {
  uint16_t v = 0;

  /* Bug fix: check failure correctly */
  if (!getFeatureInterrupts(v))
    return false;

  /* If you want to overwrite only known bits, apply a mask here.
   * Until the full mask is confirmed, keep it simple:
   */
  v = flags;

  return writeReg(BMI330_REG_INT_MAP1, v);
}

/**
 * - Read interrupt status for INT1 line.
 *
 * Notes:
 * - Status is a bitfield of interrupt causes routed to INT1.
 */
bool BMI330_7Semi::readIntStatusInt1(uint16_t &status) {
  return readReg(BMI330_REG_INT_STATUS_INT1, status);
}

/**
 * - Set a 2-bit routing field in INT_MAP1.
 * - Used by feature interrupt mapping functions.
 *
 * Notes:
 * - fieldPos is the bit offset of the 2-bit field (0,2,4,...14).
 * - Only the lowest 2 bits of 'route' are used.
 */
bool BMI330_7Semi::setIntMap1Field(uint8_t fieldPos, FeatureIntOut route) {
  uint16_t v = 0;
  if (!readReg(BMI330_REG_INT_MAP1, v))
    return false;

  v &= (uint16_t) ~((uint16_t)0x3u << fieldPos);
  v |= (uint16_t)(((uint16_t)route & 0x3u) << fieldPos);

  return writeReg(BMI330_REG_INT_MAP1, v);
}

/**
 * - Read a 2-bit routing field from INT_MAP1.
 * - Used by feature interrupt mapping getter functions.
 */
bool BMI330_7Semi::getIntMap1Field(uint8_t fieldPos, FeatureIntOut &route) {
  uint16_t v = 0;
  if (!readReg(BMI330_REG_INT_MAP1, v))
    return false;

  route = (FeatureIntOut)((v >> fieldPos) & 0x3u);
  return true;
}


/**
 * - Convert signed int16 offset to 14-bit raw code.
 * - Rejects -8192 because raw 0x2000 is invalid per datasheet.
 */
static bool uint_16ToUint_14(int16_t offset, uint16_t &raw14) {
  if (offset < -8191 || offset > 8191)
    return false;

  /* Convert to 14-bit two’s complement */
  uint16_t u = (uint16_t)offset;
  raw14 = u & (uint16_t)BMI330_ACC_DP_OFF_MASK;

  if (raw14 == (uint16_t)BMI330_ACC_DP_OFF_INVALID)
    return false;

  return true;
}

/**
 * - Convert 14-bit raw code to signed int16 (sign-extended).
 * - Returns false if raw 0x2000 is seen (invalid).
 */
static bool uint16Touint14(uint16_t reg16, int16_t &offset) {
  const uint16_t raw = reg16 & (uint16_t)BMI330_ACC_DP_OFF_MASK;

  if (raw == (uint16_t)BMI330_ACC_DP_OFF_INVALID)
    return false;

  /* Sign-extend bit13 */
  if (raw & 0x2000u)
    offset = (int16_t)(raw | 0xC000u);
  else
    offset = (int16_t)raw;

  return true;
}
/**
 * - Set accelerometer datapath offset for X axis (ACC_DP_OFF_X, 0x60).
 * - 14-bit field, 0x2000 invalid, 1 LSB = 30.52 µg.
 */
bool BMI330_7Semi::setAccelOffsetX(int16_t offset) {
  uint16_t raw14 = 0;
  if (!uint_16ToUint_14(offset, raw14))
    return false;

  return writeReg(BMI330_REG_ACC_DP_OFF_X, raw14);
}

bool BMI330_7Semi::getAccelOffsetX(int16_t &offset) {
  uint16_t v = 0;
  if (!readReg(BMI330_REG_ACC_DP_OFF_X, v))
    return false;

  return uint16Touint14(v, offset);
}

/* Y */
bool BMI330_7Semi::setAccelOffsetY(int16_t offset) {
  uint16_t raw14 = 0;
  if (!uint_16ToUint_14(offset, raw14))
    return false;

  return writeReg(BMI330_REG_ACC_DP_OFF_Y, raw14);
}

bool BMI330_7Semi::getAccelOffsetY(int16_t &offset) {
  uint16_t v = 0;
  if (!readReg(BMI330_REG_ACC_DP_OFF_Y, v))
    return false;

  return uint16Touint14(v, offset);
}

/* Z */
bool BMI330_7Semi::setAccelOffsetZ(int16_t offset) {
  uint16_t raw14 = 0;
  if (!uint_16ToUint_14(offset, raw14))
    return false;

  return writeReg(BMI330_REG_ACC_DP_OFF_Z, raw14);
}

bool BMI330_7Semi::getAccelOffsetZ(int16_t &offset) {
  uint16_t v = 0;
  if (!readReg(BMI330_REG_ACC_DP_OFF_Z, v))
    return false;

  return uint16Touint14(v, offset);
}

bool BMI330_7Semi::writeReg8(uint8_t reg, uint8_t value) {
  if (bus == Bus::I2C)
    return i2cWrite(reg, &value, 1);
  return spiWrite(reg, &value, 1);
}

bool BMI330_7Semi::readReg8(uint8_t reg, uint8_t &value) {
  uint8_t b = 0;
  if (!readBurst(reg, &b, 1))
    return false;
  value = b;
  return true;
}

/**
 * - Set accelerometer datapath re-scale (ACC_DP_DGAIN_X, 0x61).
 * - 8-bit field (upper byte reserved), covers ±3.125% sensitivity.
 */
bool BMI330_7Semi::setAccelDgainX(uint8_t value) {
  return writeReg8(BMI330_REG_ACC_DP_DGAIN_X, value);
}

bool BMI330_7Semi::getAccelDgainX(uint8_t &value) {
  return readReg8(BMI330_REG_ACC_DP_DGAIN_X, value);
}

bool BMI330_7Semi::setAccelDgainY(uint8_t value) {
  return writeReg8(BMI330_REG_ACC_DP_DGAIN_Y, value);
}

bool BMI330_7Semi::getAccelDgainY(uint8_t &value) {
  return readReg8(BMI330_REG_ACC_DP_DGAIN_Y, value);
}

bool BMI330_7Semi::setAccelDgainZ(uint8_t value) {
  return writeReg8(BMI330_REG_ACC_DP_DGAIN_Z, value);
}

bool BMI330_7Semi::getAccelDgainZ(uint8_t &value) {
  return readReg8(BMI330_REG_ACC_DP_DGAIN_Z, value);
}

/**
 * - Route "no-motion" feature interrupt output to INT1 / INT2 / I3C IBI (or disable).
 *
 * Notes:
 * - Uses the corresponding 2-bit field inside INT_MAP1.
 * - FeatureIntOut is treated as 2-bit:
 */
bool BMI330_7Semi::setNoMotionInt(FeatureIntOut output) {
  return setIntMap1Field(0, output);
}

/**
 * - Read current routing of the "no-motion" feature interrupt.
 */
bool BMI330_7Semi::getNoMotionInt(FeatureIntOut &output) {
  return getIntMap1Field(0, output);
}

/**
 * - Route "any-motion" feature interrupt output to INT1 / INT2 / I3C IBI (or disable).
 */
bool BMI330_7Semi::setAnyMotionInt(FeatureIntOut output) {
  return setIntMap1Field(2, output);
}

/**
 * - Read current routing of the "any-motion" feature interrupt.
 */
bool BMI330_7Semi::getAnyMotionInt(FeatureIntOut &output) {
  return getIntMap1Field(2, output);
}

/**
 * - Route "flat" feature interrupt output to INT1 / INT2 / I3C IBI (or disable).
 */
bool BMI330_7Semi::setFlatInt(FeatureIntOut output) {
  return setIntMap1Field(4, output);
}

/**
 * - Read current routing of the "flat" feature interrupt.
 */
bool BMI330_7Semi::getFlatInt(FeatureIntOut &output) {
  return getIntMap1Field(4, output);
}


/**
 * - Route "orientation" feature interrupt output to INT1 / INT2 / I3C IBI (or disable).
 */
bool BMI330_7Semi::setOrientationInt(FeatureIntOut output) {
  return setIntMap1Field(6, output);
}

/**
 * - Read current routing of the "orientation" feature interrupt.
 */
bool BMI330_7Semi::getOrientationInt(FeatureIntOut &output) {
  return getIntMap1Field(6, output);
}

/**
 * - Route "step detector" feature interrupt output to INT1 / INT2 / I3C IBI (or disable).
 */
bool BMI330_7Semi::setStepDetectorInt(FeatureIntOut output) {
  return setIntMap1Field(8, output);
}

/**
 * - Read current routing of the "step detector" feature interrupt.
 */
bool BMI330_7Semi::getStepDetectorInt(FeatureIntOut &output) {
  return getIntMap1Field(8, output);
}
/**
 * - Route "step counter" feature interrupt output to INT1 / INT2 / I3C IBI (or disable).
 */
bool BMI330_7Semi::setStepCounterInt(FeatureIntOut output) {
  return setIntMap1Field(10, output);
}

/**
 * - Read current routing of the "step counter" feature interrupt.
 */
bool BMI330_7Semi::getStepCounterInt(FeatureIntOut &output) {
  return getIntMap1Field(10, output);
}
/**
 * - Route "significant motion" feature interrupt output to INT1 / INT2 / I3C IBI (or disable).
 */
bool BMI330_7Semi::setSigMotionInt(FeatureIntOut output) {
  return setIntMap1Field(12, output);
}

/**
 * - Read current routing of the "significant motion" feature interrupt.
 */
bool BMI330_7Semi::getSigMotionInt(FeatureIntOut &output) {
  return getIntMap1Field(12, output);
}

/**
 * - Route "tilt" feature interrupt output to INT1 / INT2 / I3C IBI (or disable).
 */
bool BMI330_7Semi::setTiltInt(FeatureIntOut output) {
  return setIntMap1Field(14, output);
}

/**
 * - Read current routing of the "tilt" feature interrupt.
 */
bool BMI330_7Semi::getTiltInt(FeatureIntOut &output) {
  return getIntMap1Field(14, output);
}

/**
 * - Read the raw INT_MAP1 register (feature interrupt mapping register).
 *
 * Notes:
 * - Contains multiple 2-bit routing fields.
 * - Useful for debugging and for bulk configuration.
 */
bool BMI330_7Semi::getFeatureInterrupts(uint16_t &flags) {
  return readReg(BMI330_REG_INT_MAP1, flags);
}

/**
 * - Read interrupt status register for INT2 output.
 */
bool BMI330_7Semi::readIntStatusInt2(uint16_t &status) {
  return readReg(BMI330_REG_INT_STATUS_INT2, status);
}

/**
 * - Read interrupt status register for I3C IBI output.
 */
bool BMI330_7Semi::readIntStatusIbi(uint16_t &status) {
  return readReg(BMI330_REG_INT_STATUS_IBI, status);
}



bool BMI330_7Semi::getFeatureErrorStatus(uint8_t &err) {
  uint16_t val = 0;

  if (!readReg(BMI330_REG_FEATURE_IO1, val))
    return false;

  /** 
   * FEATURE_IO1.error_status is in the low nibble.
   * - Mask first, then cast to avoid precedence/implicit conversion surprises.
   */
  err = static_cast<uint8_t>(val & 0x000Fu);
  return true;
}

/**
 * Wait for feature engine to report "initialized/ready".
 * - Datasheet sequence polls FEATURE_IO1.error_status for value 0b001.
 * - timeoutMs should be generous during bring-up (e.g., 100-500ms).
 */
bool BMI330_7Semi::waitFeatureEngineReady(uint32_t timeoutMs) {
  const uint32_t start = millis();

  while ((millis() - start) < timeoutMs) {
    uint8_t err = 0;

    if (!getFeatureErrorStatus(err))
      return false;

    /** 
     * Ready condition per datasheet:
     * - FEATURE_IO1.error_status == 0b005
     */
    if (err == 0x05u)
      return true;

    delay(1);
  }

  return false;
}

bool BMI330_7Semi::enableFeatureEngine(bool enable, uint32_t timeoutMs)
{
    if (!enable)
    {
        /** 
         * - Re-enabling typically requires a soft reset / power cycle.
         */
        return writeBit(BMI330_REG_FEATURE_CTRL, BMI330_FEATURE_ENGINE_EN_BIT, false);
    }

    /**
     * 1 Disable sensors
     * 2 Write 0x012C to FEATURE_IO2
     * 3 Write 0x0001 to FEATURE_IO_STATUS (sync)
     * 4 Set FEATURE_CTRL.engine_en = 1
     * 5 Poll FEATURE_IO1.error_status for 0b001
     */
    if (!writeReg(BMI330_REG_FEATURE_IO2, 0x012Cu))
        return false;

    if (!writeReg(BMI330_REG_FEATURE_IO_STATUS, 0x0001u))
        return false;

    if (!writeBit(BMI330_REG_FEATURE_CTRL, BMI330_FEATURE_ENGINE_EN_BIT, true))
        return false;

    return waitFeatureEngineReady(timeoutMs);
}

bool BMI330_7Semi::setFeature(BMI330Feature feature, bool enable) {
  /**
   * Enable/disable a single feature bit in FEATURE_IO0.
   * - After changing FEATURE_IO0..3, we must sync via FEATURE_IO_STATUS.
   */
  if (!writeBit(BMI330_REG_FEATURE_IO0, feature, enable))
    return false;

  return applyFeatureIO();
}


bool BMI330_7Semi::applyFeatureIO() {
  /**
   * Trigger sync of FEATURE_IO0..3 with the feature engine.
   * - Writing 0x0001 applies the configuration.
   */
  return writeReg(BMI330_REG_FEATURE_IO_STATUS, 0x0001u);
}

bool BMI330_7Semi::enableStepCounter(bool enable) {
  return setFeature(STEP_COUNTER, enable);
}

bool BMI330_7Semi::enableStepDetector(bool enable) {
  return setFeature(STEP_DETECTOR, enable);
}

bool BMI330_7Semi::readStepCount(uint32_t &steps) {
  uint16_t lo = 0, hi = 0;

  if (!readReg(BMI330_REG_FEATURE_IO2, lo))
    return false;

  if (!readReg(BMI330_REG_FEATURE_IO3, hi))
    return false;

  /**
   * Step count is a 32-bit value split across two 16-bit words.
   * - FEATURE_IO3 is MSW, FEATURE_IO2 is LSW.
   */
  steps = (static_cast<uint32_t>(hi) << 16) | static_cast<uint32_t>(lo);
  return true;
}


/**
 * Extended register sequence:
 * - Read EXT address 0x0010
 * - Set bit 10 (reset), apply, then clear bit 10, apply again
 */
bool BMI330_7Semi::resetStepCount() {
  uint16_t v = 0;
  if (!extRead(0x0010u, v))
    return false;

  v |= (1u << 10);
  if (!extWrite(0x0010u, v))
    return false;

  if (!applyFeatureIO())
    return false;

  v &= ~(1u << 10);
  if (!extWrite(0x0010u, v))
    return false;

  return applyFeatureIO();
}

/**
 * Enable tap features individually.
 * - Return immediately on first failure.
 */
bool BMI330_7Semi::enableTap(bool singleTap, bool doubleTap, bool tripleTap) {

  if (!setFeature(TAP_S, singleTap))
    return false;

  if (!setFeature(TAP_D, doubleTap))
    return false;

  if (!setFeature(TAP_T, tripleTap))
    return false;

  return true;
}

bool BMI330_7Semi::readTapEvent(bool &single, bool &doubleTap, bool &tripleTap)
{
    uint16_t v = 0;

    /**
     * NOTE:
     * - Replace 0x47 with a named register constant in your header if available.
     */
    if (!readReg(BMI330_REG_FEATURE_EVENT_EXT, v))
        return false;

    single    = (v & (1u << 3)) != 0;
    doubleTap = (v & (1u << 4)) != 0;
    tripleTap = (v & (1u << 5)) != 0;
    return true;
}

bool BMI330_7Semi::enableMotion(bool anyX, bool anyY, bool anyZ,
                                bool noX, bool noY, bool noZ) {
  /**
   * Enable motion bits per-axis.
   * - Any-motion and No-motion are independent.
   */
  if (!setFeature(ANY_MOTION_X, anyX)) return false;
  if (!setFeature(ANY_MOTION_Y, anyY)) return false;
  if (!setFeature(ANY_MOTION_Z, anyZ)) return false;

  if (!setFeature(NO_MOTION_X, noX)) return false;
  if (!setFeature(NO_MOTION_Y, noY)) return false;
  if (!setFeature(NO_MOTION_Z, noZ)) return false;

  return true;
}
bool BMI330_7Semi::enableOrientation(bool enable) {
  return setFeature(ORIENTATION, enable);
}

bool BMI330_7Semi::enableFlat(bool enable) {
  return setFeature(FLAT, enable);
}

bool BMI330_7Semi::enableTilt(bool enable) {
  return setFeature(TILT, enable);
}

bool BMI330_7Semi::enableSignificantMotion(bool enable) {
  return setFeature(SIG_MOTION, enable);
}

bool BMI330_7Semi::extWrite(uint16_t extAddress, uint16_t value) {
  /**
   * Write to extended register space:
   * - Set FEATURE_DATA_ADDR, then write FEATURE_DATA_TX.
   */
  if (!writeReg(BMI330_REG_FEATURE_DATA_ADDR, extAddress))
    return false;

  return writeReg(BMI330_REG_FEATURE_DATA_TX, value);
}

bool BMI330_7Semi::extRead(uint16_t extAddress, uint16_t &value) {
  /**
   * Read from extended register space:
   * - Set FEATURE_DATA_ADDR, then read back the data word.
   */
  if (!writeReg(BMI330_REG_FEATURE_DATA_ADDR, extAddress))
    return false;

  return readReg(BMI330_REG_FEATURE_DATA_TX, value);
}

bool BMI330_7Semi::extWriteBlock(uint16_t startExtAddress, const uint16_t *values, size_t wordCount) {
  /**
   * Sequential extended-register write.
   * - Each word increments the extended address by 1.
   */
  for (size_t i = 0; i < wordCount; i++) {
    if (!extWrite(static_cast<uint16_t>(startExtAddress + i), values[i]))
      return false;
  }

  return true;
}
