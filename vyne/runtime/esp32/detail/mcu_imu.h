// runtime/esp32/detail/mcu_imu.h
//
// Minimal MPU6050 driver over `<Wire.h>`. No external IMU library is
// required: the register map is small and stable. Default address 0x68
// (AD0 low); pass 0x69 for AD0 tied high.
//
// Swapping to an ICM-20948 is one-file work: the public Vyne surface is
// identical because both chips expose acceleration, angular rate, and an
// on-die temperature sensor. Only the register addresses and scale
// factors in this header change.
#pragma once
#include "mcu_types.h"
#include <Wire.h>
#include <math.h>

#define VYNE_MPU6050_REG_SMPLRT_DIV 0x19
#define VYNE_MPU6050_REG_CONFIG 0x1A
#define VYNE_MPU6050_REG_GYRO_CONFIG 0x1B
#define VYNE_MPU6050_REG_ACCEL_CONFIG 0x1C
#define VYNE_MPU6050_REG_ACCEL_XOUT_H 0x3B
#define VYNE_MPU6050_REG_PWR_MGMT_1 0x6B
#define VYNE_MPU6050_REG_WHO_AM_I 0x75

static uint8_t g_vyne_imu_addr = 0x68;
static bool g_vyne_imu_ok = false;

static float g_vyne_imu_ax = 0.0f;
static float g_vyne_imu_ay = 0.0f;
static float g_vyne_imu_az = 0.0f;
static float g_vyne_imu_gx = 0.0f;
static float g_vyne_imu_gy = 0.0f;
static float g_vyne_imu_gz = 0.0f;
static float g_vyne_imu_temp = 0.0f;

static inline bool _vyne_imu_write_reg(uint8_t reg, uint8_t value) {
    Wire.beginTransmission(g_vyne_imu_addr);
    Wire.write(reg);
    Wire.write(value);
    return Wire.endTransmission() == 0;
}

static inline bool _vyne_imu_read_reg(uint8_t reg, uint8_t* out) {
    Wire.beginTransmission(g_vyne_imu_addr);
    Wire.write(reg);
    if (Wire.endTransmission(false) != 0)
        return false;
    if (Wire.requestFrom(g_vyne_imu_addr, (uint8_t)1) != 1)
        return false;
    *out = (uint8_t)Wire.read();
    return true;
}

static inline bool _vyne_imu_update(void) {
    if (!g_vyne_imu_ok)
        return false;

    Wire.beginTransmission(g_vyne_imu_addr);
    Wire.write(VYNE_MPU6050_REG_ACCEL_XOUT_H);
    if (Wire.endTransmission(false) != 0)
        return false;
    if (Wire.requestFrom(g_vyne_imu_addr, (uint8_t)14) != 14)
        return false;

    int16_t raw[7];
    for (int i = 0; i < 7; ++i) {
        int hi = Wire.read();
        int lo = Wire.read();
        raw[i] = (int16_t)((hi << 8) | (lo & 0xFF));
    }

    // Default full-scale ranges: +/-2 g and +/-250 deg/s.
    g_vyne_imu_ax = (float)raw[0] / 16384.0f;
    g_vyne_imu_ay = (float)raw[1] / 16384.0f;
    g_vyne_imu_az = (float)raw[2] / 16384.0f;
    g_vyne_imu_temp = (float)raw[3] / 340.0f + 36.53f;
    g_vyne_imu_gx = (float)raw[4] / 131.0f;
    g_vyne_imu_gy = (float)raw[5] / 131.0f;
    g_vyne_imu_gz = (float)raw[6] / 131.0f;
    return true;
}

static inline VyneValue esp32_imu_init(VyneValue address) {
    int a = (int)address.as.i64;
    g_vyne_imu_addr = (a == 0) ? 0x68 : (uint8_t)a;
    g_vyne_imu_ok = false;

    Wire.begin();
    uint8_t who = 0;
    if (!_vyne_imu_read_reg(VYNE_MPU6050_REG_WHO_AM_I, &who))
        return vyne_bool(false);

    // 0x68 = MPU6050. Also accept the MPU6500/9250 family (0x70/0x71/0x73)
    // because they share the same register layout for the fields we use.
    if (who != 0x68 && who != 0x70 && who != 0x71 && who != 0x73)
        return vyne_bool(false);

    _vyne_imu_write_reg(VYNE_MPU6050_REG_PWR_MGMT_1, 0x00);
    _vyne_imu_write_reg(VYNE_MPU6050_REG_SMPLRT_DIV, 0x07);
    _vyne_imu_write_reg(VYNE_MPU6050_REG_CONFIG, 0x00);
    _vyne_imu_write_reg(VYNE_MPU6050_REG_GYRO_CONFIG, 0x00);
    _vyne_imu_write_reg(VYNE_MPU6050_REG_ACCEL_CONFIG, 0x00);
    delay(10);

    g_vyne_imu_ok = true;
    return vyne_bool(true);
}

static inline VyneValue esp32_imu_ok(void) {
    return vyne_bool(g_vyne_imu_ok);
}

static inline VyneValue esp32_imu_accel_x(void) {
    _vyne_imu_update();
    return vyne_float((double)g_vyne_imu_ax);
}

static inline VyneValue esp32_imu_accel_y(void) {
    _vyne_imu_update();
    return vyne_float((double)g_vyne_imu_ay);
}

static inline VyneValue esp32_imu_accel_z(void) {
    _vyne_imu_update();
    return vyne_float((double)g_vyne_imu_az);
}

static inline VyneValue esp32_imu_gyro_x(void) {
    _vyne_imu_update();
    return vyne_float((double)g_vyne_imu_gx);
}

static inline VyneValue esp32_imu_gyro_y(void) {
    _vyne_imu_update();
    return vyne_float((double)g_vyne_imu_gy);
}

static inline VyneValue esp32_imu_gyro_z(void) {
    _vyne_imu_update();
    return vyne_float((double)g_vyne_imu_gz);
}

static inline VyneValue esp32_imu_temp(void) {
    _vyne_imu_update();
    return vyne_float((double)g_vyne_imu_temp);
}

static inline int64_t _vyne_imu_quant(float v, float lo, float hi) {
    // Map [lo, hi] onto 0..511. Out-of-range values clamp.
    float t = (v - lo) / (hi - lo);
    if (t < 0.0f)
        t = 0.0f;
    if (t > 1.0f)
        t = 1.0f;
    return (int64_t)(t * 511.0f);
}

static inline VyneValue esp32_imu_read(void) {
    // The spec calls for a 7 x 10-bit packed word, but 70 bits do not fit
    // in an Int64. This implementation uses 7 x 9 = 63 bits instead:
    //   bits  0..8  ax,  9..17 ay, 18..26 az,
    //   bits 27..35 gx, 36..44 gy, 45..53 gz,
    //   bits 54..62 temp.
    // The individual accessors remain the precise interface.
    _vyne_imu_update();
    int64_t q[7];
    q[0] = _vyne_imu_quant(g_vyne_imu_ax, -4.0f, 4.0f);
    q[1] = _vyne_imu_quant(g_vyne_imu_ay, -4.0f, 4.0f);
    q[2] = _vyne_imu_quant(g_vyne_imu_az, -4.0f, 4.0f);
    q[3] = _vyne_imu_quant(g_vyne_imu_gx, -500.0f, 500.0f);
    q[4] = _vyne_imu_quant(g_vyne_imu_gy, -500.0f, 500.0f);
    q[5] = _vyne_imu_quant(g_vyne_imu_gz, -500.0f, 500.0f);
    q[6] = _vyne_imu_quant(g_vyne_imu_temp, -40.0f, 85.0f);

    int64_t packed = 0;
    for (int i = 0; i < 7; ++i)
        packed |= (q[i] & 0x1FF) << (9 * i);
    return vyne_int(packed);
}
