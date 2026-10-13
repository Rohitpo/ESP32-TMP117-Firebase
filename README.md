# ESP32 Sensor + Firebase Realtime Database (PlatformIO)

## Hardware / wiring
For a standard ESP32 DevKit:

- ESP32 3V3 -> TMP117 VIN (breakout board), and BMI330 module VDD/VDDIO as specified by its board
- ESP32 GND -> both sensor GND pins
- GPIO 21 -> TMP117 SDA and BMI330 SDI/SDA
- GPIO 22 -> TMP117 SCL and BMI330 SCK/SCL
- GPIO 2 -> onboard LED (verify the board LED pin/polarity)

Use USB power for the ESP32 while debugging. Do not connect an unidentified battery or power BMI330 with 5 V. Both sensors must share GND. The TMP117 breakout in the user's photo is marked TMP117 and has VIN, GND, SCL, SDA pins.

## Build in VS Code
1. Install VS Code and the PlatformIO IDE extension.
2. Open this whole folder (`esp32_firebase_task`) in VS Code.
3. Edit `include/secrets.h`: enter Wi-Fi credentials and the Firebase Realtime Database URL.
4. Click PlatformIO **Build** (checkmark) to compile.
5. Connect ESP32 over USB, select the correct upload port in `platformio.ini`/PlatformIO settings, then click **Upload**.
6. Open PlatformIO Serial Monitor at 115200 baud.

## Firmware binary for ESP Flash Download Tool
After a successful PlatformIO build, the application binary is normally at:
` .pio/build/esp32dev/firmware.bin `

The ESP Flash Download Tool also needs the matching bootloader, partition table, and boot_app0 binary for the selected ESP32 board/flash layout. The simplest option is PlatformIO Upload. If using ESP Flash Download Tool, use the generated files from the same PlatformIO build and the offsets from the corresponding `flash_args`/build output; do not guess offsets.

## Firebase
The code PATCHes `temperature` and `steps` to `/device1` every 10 seconds and reads `ledOn` from the same object every 2 seconds. In Firebase Realtime Database, create `/device1/ledOn` with a boolean value (`true` or `false`). Ensure database authentication/rules permit this device to read and write the node.

`tls.setInsecure()` is used only to simplify a time-limited demo. It skips TLS certificate verification and should be replaced with root-CA verification for production. Never expose a publicly writable Firebase database.

## Troubleshooting
- If I2C scan finds no devices, check 3.3 V, GND, SDA=21, SCL=22 and common ground. Disconnect modem/other devices while debugging.
- TMP117 address is usually `0x48`.
- BMI330 modules commonly use `0x68` or `0x69`; this sketch tries both through the 7Semi library.
- The earlier `invalid header: 0xffffffff` message is a flash/boot issue, not a sensor error. Disconnect all peripherals and flash a valid build over USB first.
