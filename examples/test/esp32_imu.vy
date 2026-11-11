# esp32_imu.vy — MPU6050 accel/gyro/temperature for 5 seconds.
#
# Requires: VYNE_USE_IMU; MPU6050 on I2C at 0x68 (AD0 low)
# Hardware: ESP32-WROOM-32, SDA=21, SCL=22
# Flash:    vynec --esp32 esp32 --esp32-flash --esp32-port COM3 examples/test/esp32_imu.vy
# Expect:   ax/ay/az/gx/gy/gz/temp readings, then "imu: DONE"

use native esp32;

esp32.serial_begin(115200);
esp32.delay(300);

esp32.serial_println("=== ESP32 IMU ===");

ok :: Bool := esp32.imu_init(0);
if (ok) {
    esp32.serial_println("MPU6050 detected");
} else {
    esp32.serial_println("MPU6050 NOT detected (check wiring/address)");
}

if (ok) {
    start :: Int64 := esp32.millis();
    while ((esp32.millis() - start) < 5000) {
        esp32.serial_print("a=(");
        esp32.serial_print(esp32.imu_accel_x());
        esp32.serial_print(", ");
        esp32.serial_print(esp32.imu_accel_y());
        esp32.serial_print(", ");
        esp32.serial_print(esp32.imu_accel_z());
        esp32.serial_print(") g  g=(");
        esp32.serial_print(esp32.imu_gyro_x());
        esp32.serial_print(", ");
        esp32.serial_print(esp32.imu_gyro_y());
        esp32.serial_print(", ");
        esp32.serial_print(esp32.imu_gyro_z());
        esp32.serial_print(") dps  t=");
        esp32.serial_print(esp32.imu_temp());
        esp32.serial_println(" C");
        esp32.delay(500);
    }
}

esp32.serial_println("imu: DONE");
