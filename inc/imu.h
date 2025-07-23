#ifndef IMU_H
#define IMU_H

#include <stdint.h>
#include <stdbool.h>

// Definições padrão do sensor
#define MPU6050_ADDR  0x68
#define MPU6050_REG_PWR_MGMT_1  0x6B
#define MPU6050_REG_ACCEL_XOUT_H 0x3B
#define MPU6050_REG_GYRO_XOUT_H  0x43

#define I2C_PORT i2c0
#define I2C_SDA  0
#define I2C_SCL  1

void imu_init(void);
bool imu_read_raw(int16_t accel[3], int16_t gyro[3]);
void imu_reset(void);

#endif
