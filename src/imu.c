#include "../inc/imu.h"
#include "hardware/i2c.h"
#include "pico/stdlib.h"


static int addr = MPU6050_ADDR;

void imu_init(void) {
    // Inicializa I2C
    i2c_init(I2C_PORT, 400 * 1000);
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA);
    gpio_pull_up(I2C_SCL);

    // Sai do modo sleep
    uint8_t buf[] = {MPU6050_REG_PWR_MGMT_1, 0x00};
    i2c_write_blocking(I2C_PORT, addr, buf, 2, false);
    sleep_ms(100);
}

bool imu_read_raw(int16_t accel[3], int16_t gyro[3]) {
    uint8_t buffer[6];

    // Lê acelerômetro
    uint8_t reg = MPU6050_REG_ACCEL_XOUT_H;
    if (i2c_write_blocking(I2C_PORT, addr, &reg, 1, true) < 0) return false;
    if (i2c_read_blocking(I2C_PORT, addr, buffer, 6, false) < 0) return false;

    for (int i = 0; i < 3; i++)
        accel[i] = (buffer[i * 2] << 8) | buffer[i * 2 + 1];

    // Lê giroscópio
    reg = MPU6050_REG_GYRO_XOUT_H;
    if (i2c_write_blocking(I2C_PORT, addr, &reg, 1, true) < 0) return false;
    if (i2c_read_blocking(I2C_PORT, addr, buffer, 6, false) < 0) return false;

    for (int i = 0; i < 3; i++)
        gyro[i] = (buffer[i * 2] << 8) | buffer[i * 2 + 1];

    return true;
}

void imu_reset()
{
    // Dois bytes para reset: primeiro o registrador, segundo o dado
    uint8_t buf[] = {0x6B, 0x80};
    i2c_write_blocking(I2C_PORT, addr, buf, 2, false);
    sleep_ms(100); // Aguarda reset e estabilização

    // Sai do modo sleep (registrador 0x6B, valor 0x00)
    buf[1] = 0x00;
    i2c_write_blocking(I2C_PORT, addr, buf, 2, false);
    sleep_ms(10); // Aguarda estabilização após acordar
}