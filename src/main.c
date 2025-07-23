#include <stdio.h>
#include <string.h>
#include <math.h>
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
#define ENDERECO_DISP 0x3C

// Estados do sistema
typedef enum {
    STATE_INITIALIZING,
    STATE_READY,
    STATE_RECORDING,
    STATE_ERROR
} system_state_t;

// Variáveis globais para controle
static system_state_t current_state = STATE_INITIALIZING;
static bool is_recording = false;
static uint32_t sample_count = 0;
static uint32_t recording_start_time = 0;
static const char *imu_log_filename = "imu_data.csv";
static bool sd_mounted = false;

// Display
static ssd1306_t ssd;

// Trecho para modo BOOTSEL usando o botão B
#include "pico/bootrom.h"
#define botaoB 6
void gpio_irq_handler(uint gpio, uint32_t events)
{
    reset_usb_boot(0, 0);
}

// Função para atualizar o display baseado no estado
void update_display() {
    char line1[20], line2[20], line3[20], line4[20];
    
    // Limpa o display
    ssd1306_fill(&ssd, false);
    
    // Desenha o layout básico
    ssd1306_rect(&ssd, 3, 3, 122, 60, true, false);
    ssd1306_line(&ssd, 3, 25, 123, 25, true);
    ssd1306_line(&ssd, 3, 37, 123, 37, true);
    
    // Cabeçalho fixo
    ssd1306_draw_string(&ssd, "CEPEDI   TIC37", 8, 6);
    ssd1306_draw_string(&ssd, "IMU DATALOGGER", 8, 16);
    
    // Conteúdo baseado no estado
    switch (current_state) {
        case STATE_INITIALIZING:
            ssd1306_draw_string(&ssd, "Inicializando...", 10, 28);
            ssd1306_draw_string(&ssd, "Aguarde", 35, 41);
            break;
            
        case STATE_READY:
            if (sd_mounted) {
                ssd1306_draw_string(&ssd, "Sistema Pronto", 15, 28);
                ssd1306_draw_string(&ssd, "s=iniciar log", 10, 41);
                ssd1306_draw_string(&ssd, "m=mount u=umount", 5, 52);
            } else {
                ssd1306_draw_string(&ssd, "SD Nao Detectado", 8, 28);
                ssd1306_draw_string(&ssd, "Monte o SD (m)", 12, 41);
            }
            break;
            
        case STATE_RECORDING:
            ssd1306_draw_string(&ssd, "Gravando...", 25, 28);
            
            // Mostra número de amostras
            snprintf(line3, sizeof(line3), "Amostras: %lu", sample_count);
            ssd1306_draw_string(&ssd, line3, 10, 41);
            
            // Mostra tempo de gravação
            uint32_t recording_time = (to_ms_since_boot(get_absolute_time()) - recording_start_time) / 1000;
            snprintf(line4, sizeof(line4), "Tempo: %lus", recording_time);
            ssd1306_draw_string(&ssd, line4, 30, 52);
            break;
            
        case STATE_ERROR:
            ssd1306_draw_string(&ssd, "ERRO!", 45, 28);
            ssd1306_draw_string(&ssd, "Verifique SD", 15, 41);
            break;
    }
    
    ssd1306_send_data(&ssd);
}

// Função para processar comandos do teclado
void process_command(char cmd) {
    switch (cmd) {
        case 's': // Start/Stop recording
            if (!sd_mounted) {
                printf("Erro: SD não montado. Monte primeiro com 'm'.\n");
                current_state = STATE_ERROR;
                break;
            }
            
            if (!is_recording) {
                // Inicia gravação
                if (sdlogger_start(imu_log_filename)) {
                    is_recording = true;
                    sample_count = 0;
                    recording_start_time = to_ms_since_boot(get_absolute_time());
                    current_state = STATE_RECORDING;
                    printf("Gravação iniciada! Pressione 's' novamente para parar.\n");
                } else {
                    current_state = STATE_ERROR;
                    printf("Erro ao iniciar gravação!\n");
                }
            } else {
                // Para gravação
                sdlogger_stop();
                is_recording = false;
                current_state = STATE_READY;
                printf("Gravação parada! Total de amostras: %lu\n", sample_count);
            }
            break;
            
        case 'm': // Mount SD
            printf("Montando SD...\n");
            run_mount();
            sd_mounted = true;
            current_state = STATE_READY;
            break;
            
        case 'u': // Unmount SD
            if (is_recording) {
                sdlogger_stop();
                is_recording = false;
                printf("Gravação interrompida devido ao desmonte do SD.\n");
            }
            printf("Desmontando SD...\n");
            run_unmount();
            sd_mounted = false;
            current_state = STATE_READY;
            break;
            
        case 'l': // List files
            if (sd_mounted) {
                run_ls();
            } else {
                printf("SD não montado. Monte primeiro com 'm'.\n");
            }
            break;
            
        case 'h': // Help
            printf("\nComandos disponíveis:\n");
            printf("s - Iniciar/Parar gravação do IMU\n");
            printf("m - Montar SD card\n");
            printf("u - Desmontar SD card\n");
            printf("l - Listar arquivos no SD\n");
            printf("h - Mostrar ajuda\n\n");
            break;
            
        default:
            printf("Comando não reconhecido. Digite 'h' para ajuda.\n");
            break;
    }
}

// Função principal de captura e gravação do IMU
void imu_capture_loop() {
    if (!is_recording) return;
    
    int16_t accel[3], gyro[3];
    
    // Lê dados do IMU
    if (imu_read_raw(accel, gyro)) {
        // Incrementa contador de amostras
        sample_count++;
        
        // Grava no SD
        if (!sdlogger_log_sample(sample_count, accel, gyro)) {
            printf("Erro ao gravar amostra %lu. Parando gravação.\n", sample_count);
            is_recording = false;
            current_state = STATE_ERROR;
            return;
        }
        
        // Debug: mostra dados a cada 100 amostras
        if (sample_count % 100 == 0) {
            printf("Amostra %lu: ACC[%d,%d,%d] GYRO[%d,%d,%d]\n", 
                   sample_count, accel[0], accel[1], accel[2], 
                   gyro[0], gyro[1], gyro[2]);
        }
    } else {
        printf("Erro na leitura do IMU na amostra %lu\n", sample_count);
    }
}

int main() {
    // Inicialização do modo BOOTSEL com botão B
    gpio_init(botaoB);
    gpio_set_dir(botaoB, GPIO_IN);
    gpio_pull_up(botaoB);
    gpio_set_irq_enabled_with_callback(botaoB, GPIO_IRQ_EDGE_FALL, true, &gpio_irq_handler);

    // Inicialização básica
    stdio_init_all();
    sleep_ms(2000); // Aguarda estabilização
    
    printf("\n=== IMU DATALOGGER INICIANDO ===\n");
    current_state = STATE_INITIALIZING;

    // Inicializa I2C do Display OLED
    i2c_init(I2C_PORT_DISP, 400 * 1000);
    gpio_set_function(I2C_SDA_DISP, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL_DISP, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA_DISP);
    gpio_pull_up(I2C_SCL_DISP);

    // Configura display
    ssd1306_init(&ssd, WIDTH, HEIGHT, false, ENDERECO_DISP, I2C_PORT_DISP);
    ssd1306_config(&ssd);
    
    // Primeira atualização do display
    update_display();
    sleep_ms(1000);

    // Inicializa IMU
    printf("Inicializando IMU...\n");
    bi_decl(bi_2pins_with_func(I2C_SDA, I2C_SCL, GPIO_FUNC_I2C));
    imu_init();
    
    // Testa leitura do IMU
    int16_t test_acc[3], test_gyro[3];
    if (imu_read_raw(test_acc, test_gyro)) {
        printf("IMU inicializado com sucesso!\n");
        printf("Teste - ACC: %d %d %d | GYRO: %d %d %d\n", 
               test_acc[0], test_acc[1], test_acc[2], 
               test_gyro[0], test_gyro[1], test_gyro[2]);
    } else {
        printf("Erro na inicialização do IMU!\n");
        current_state = STATE_ERROR;
    }

    // Muda para estado pronto
    if (current_state != STATE_ERROR) {
        current_state = STATE_READY;
        printf("\nSistema pronto!\n");
        printf("Digite 'h' para ver comandos disponíveis.\n");
    }

    // Variáveis para controle de timing
    uint32_t last_display_update = 0;
    uint32_t last_sample_time = 0;
    const uint32_t DISPLAY_UPDATE_INTERVAL = 500; // 500ms
    const uint32_t SAMPLE_INTERVAL = 10; // 10ms = 100Hz
    
    // Loop principal
    while (1) {
        uint32_t current_time = to_ms_since_boot(get_absolute_time());
        
        // Processa comandos do usuário
        int c = getchar_timeout_us(0);
        if (c != PICO_ERROR_TIMEOUT) {
            process_command((char)c);
        }
        
        // Atualiza display a cada 500ms
        if (current_time - last_display_update >= DISPLAY_UPDATE_INTERVAL) {
            update_display();
            last_display_update = current_time;
        }
        
        // Captura dados do IMU se estiver gravando (100Hz)
        if (current_time - last_sample_time >= SAMPLE_INTERVAL) {
            imu_capture_loop();
            last_sample_time = current_time;
        }
        
        // Pequeno delay para não sobrecarregar o sistema
        sleep_ms(1);
    }
    
    return 0;
}