# i2c_imu.vy — probe an MPU-6050 over I2C and print WHO_AM_I.
#
# Wiring (Arduino Uno R3):
#   VCC -> 3.3V (or 5V on a breakout with a regulator)
#   GND -> GND
#   SDA -> A4
#   SCL -> A5
#
# MPU-6050: 7-bit address 0x68, WHO_AM_I register 0x75, expected value
# 0x68 (104 decimal). An unconnected bus returns -1.

use native arduino;
use external "vi2c/vi2c.vy";

arduino.serial_begin(115200);
arduino.delay(200);

arduino.serial_print("=== I2C IMU probe ===\n");

region probe {
    vi2c.begin();

    # Read WHO_AM_I from the default MPU-6050 address.
    id :: Int64 := vi2c.read_reg(0x68, 0x75);

    arduino.serial_print("WHO_AM_I = ");
    if id < 0 {
        arduino.serial_print("no response\n");
    } else {
        arduino.serial_print(id);
        arduino.serial_print("\n");
        if id == 104 {
            arduino.serial_print("MPU-6050 detected\n");
        } else {
            arduino.serial_print("unexpected identity\n");
        }
    }

    arduino.serial_print("arena used = ");
    arduino.serial_print(arduino.arena_used());
    arduino.serial_print(" B\n");

    vi2c.end();
};
