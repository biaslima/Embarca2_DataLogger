#include <stdio.h>
#include <string.h>
#include <math.h>                    // Para as funções trigonométricas
#include "pico/stdlib.h"
#include "pico/binary_info.h"
#include "hardware/i2c.h"
#include "../inc/ssd1306.h"
#include "../inc/font.h"
#include "../inc/imu.h"
#include "../inc/sdlogger.h"

// Definição dos pinos I2C para o display OLED
#define I2C_PORT_DISP i2c1
#define I2C_SDA_DISP 14
#define I2C_SCL_DISP 15
#define ENDERECO_DISP 0x3C            // Endereço I2C do display

// Variáveis para controle da gravação
bool is_recording = false;
uint32_t sample_count = 0;
const char *imu_log_filename = "imu_data.csv";

// Trecho para modo BOOTSEL usando o botão B
#include "pico/bootrom.h"
#define botaoB 6
void gpio_irq_handler(uint gpio, uint32_t events)
{
    reset_usb_boot(0, 0);
}

int main()
{
    // Inicialização do modo BOOTSEL com botão B
    gpio_init(botaoB);
    gpio_set_dir(botaoB, GPIO_IN);
    gpio_pull_up(botaoB);
    gpio_set_irq_enabled_with_callback(botaoB, GPIO_IRQ_EDGE_FALL, true, &gpio_irq_handler);

    stdio_init_all();
    sleep_ms(5000);

    // Inicializa a I2C do Display OLED em 400kHz
    i2c_init(I2C_PORT_DISP, 400 * 1000);
    gpio_set_function(I2C_SDA_DISP, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL_DISP, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA_DISP);
    gpio_pull_up(I2C_SCL_DISP);

    ssd1306_t ssd;
    ssd1306_init(&ssd, WIDTH, HEIGHT, false, ENDERECO_DISP, I2C_PORT_DISP);
    ssd1306_config(&ssd);
    ssd1306_send_data(&ssd);

    // Limpa o display
    ssd1306_fill(&ssd, false);
    ssd1306_send_data(&ssd);

    int16_t acc[3], gyro[3];
    imu_init();
    if (imu_read_raw(acc, gyro)) {
    printf("ACC: %d %d %d | GYRO: %d %d %d\n", acc[0], acc[1], acc[2], gyro[0], gyro[1], gyro[2]);
    }

    // Declara os pinos como I2C na Binary Info
    bi_decl(bi_2pins_with_func(I2C_SDA, I2C_SCL, GPIO_FUNC_I2C));
    imu_reset();

    printf("Pressione 's' para iniciar/parar o log do IMU.\n");
    printf("Pressione 'm' para montar o SD.\n");
    printf("Pressione 'u' para desmontar o SD.\n");
    printf("Pressione 'l' para listar arquivos.\n");

    bool cor = true;    
    while (1)
    {
        // Leitura dos dados de aceleração, giroscópio e temperatura
        imu_read_raw(acc, gyro);

        // Conversão para unidade de 'g'
        float ax = acc[0] / 16384.0f;
        float ay = acc[1] / 16384.0f;
        float az = acc[2] / 16384.0f;

        // Cálculo dos ângulos em graus (Roll e Pitch)
        float roll  = atan2(ay, az) * 180.0f / M_PI;
        float pitch = atan2(-ax, sqrt(ay*ay + az*az)) * 180.0f / M_PI;

        // Montagem das strings para o display
        char str_roll[20];
        char str_pitch[20];
        char str_status[20];
        char str_samples[20];

        
        snprintf(str_roll,  sizeof(str_roll),  "%5.1f", roll);
        snprintf(str_pitch, sizeof(str_pitch), "%5.1f", pitch);
        

        // Exibição no display
        ssd1306_fill(&ssd, !cor);                            // Limpa o display
        ssd1306_rect(&ssd, 3, 3, 122, 60, cor, !cor);        // Desenha um retângulo
        ssd1306_line(&ssd, 3, 25, 123, 25, cor);             // Desenha uma linha horizontal
        ssd1306_line(&ssd, 3, 37, 123, 37, cor);             // Desenha outra linha horizontal
        ssd1306_draw_string(&ssd, "CEPEDI   TIC37", 8, 6);   // Escreve texto no display
        ssd1306_draw_string(&ssd, "EMBARCATECH", 20, 16);    // Escreve texto no display
        ssd1306_draw_string(&ssd, "IMU    MPU6050", 10, 28); // Escreve texto no display
        ssd1306_line(&ssd, 63, 35, 63, 60, cor);             // Desenha uma linha vertical
        ssd1306_draw_string(&ssd, "roll", 14, 41);           // Escreve texto no display
        ssd1306_draw_string(&ssd, str_roll, 14, 52);         // Exibe Roll
        ssd1306_draw_string(&ssd, "pitch", 73, 41);           // Escreve texto no display        
        ssd1306_draw_string(&ssd, str_pitch, 73, 52);        // Exibe Pitch
        ssd1306_send_data(&ssd);
        sleep_ms(500);
    }
}
