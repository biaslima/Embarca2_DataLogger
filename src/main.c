#include <stdio.h>
#include <string.h>
#include <math.h>

#include "pico/stdlib.h"
#include "pico/binary_info.h"
#include "pico/bootrom.h"
#include "hardware/i2c.h"
#include "../lib/FatFs_SPI/sd_driver/hw_config.h"

#include "../inc/ssd1306.h"
#include "../inc/font.h"
#include "../inc/imu.h"
#include "../inc/sdlogger.h"
#include "../inc/interface.h"

//CONFIGURAÇÕES DO DISPLAY
#define I2C_PORT_DISP i2c1
#define I2C_SDA_DISP 14
#define I2C_SCL_DISP 15
#define ENDERECO_DISP 0x3C

//CONFIGURAÇÕES DO SISTEMA
#define SAMPLE_RATE_HZ 100
#define SAMPLE_INTERVAL_MS (1000 / SAMPLE_RATE_HZ)
#define DISPLAY_UPDATE_INTERVAL_MS 250  // ALTERAÇÃO: Reduzido de 500ms para 250ms
#define SD_CHECK_INTERVAL_MS 500        // NOVA: Intervalo para verificar SD card

//VARIÁVEIS GLOBAIS
static system_state_t current_state = STATE_INITIALIZING;
static bool is_recording = false;
static bool sd_mounted = false;
static uint32_t sample_count = 0;
static uint32_t recording_start_time = 0;
static const char *imu_log_filename = "imu_data.csv";
static ssd1306_t ssd;

// NOVA FUNÇÃO: Verifica se o SD card está fisicamente presente e montado
bool check_sd_status(void) {
    // Tenta verificar se o SD está montado tentando acessar informações do sistema de arquivos
    DWORD fre_clust, fre_sect, tot_sect;
    FATFS *p_fs = sd_get_fs_by_name(sd_get_by_num(0)->pcName);
    if (!p_fs) return false;
    
    FRESULT fr = f_getfree(sd_get_by_num(0)->pcName, &fre_clust, &p_fs);
    return (fr == FR_OK);
}

// NOVA FUNÇÃO: Auto-detecção e montagem do SD
void auto_mount_sd(void) {
    if (!sd_mounted) {
        printf("Tentando montar SD automaticamente...\n");
        interface_sd_access_indication(true); // Mostra azul durante montagem
        run_mount();
        if (check_sd_status()) {
            sd_mounted = true;
            current_state = STATE_READY;
            buzzer_play_sequence(BUZZER_SD_MOUNT);
            printf("SD montado automaticamente!\n");
        } else {
            printf("SD não detectado.\n");
            current_state = STATE_READY; // Vai para READY mas sem SD (ficará roxo)
        }
        interface_sd_access_indication(false);
    }
}

//FUNÇÕES DO DISPLAY
void display_init(void) {
    // Inicializa I2C do Display OLED
    i2c_init(I2C_PORT_DISP, 400 * 1000);
    gpio_set_function(I2C_SDA_DISP, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL_DISP, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA_DISP);
    gpio_pull_up(I2C_SCL_DISP);

    // Configura display
    ssd1306_init(&ssd, WIDTH, HEIGHT, false, ENDERECO_DISP, I2C_PORT_DISP);
    ssd1306_config(&ssd);
}

void display_update(void) {
    char line[20];

    ssd1306_fill(&ssd, false);
    ssd1306_rect(&ssd, 3, 3, 122, 60, true, false);
    ssd1306_line(&ssd, 3, 25, 123, 25, true);
    ssd1306_line(&ssd, 3, 37, 123, 37, true);
    
    // Cabeçalho fixo
    ssd1306_draw_string(&ssd, "CEPEDI   TIC37", 8, 6);
    ssd1306_draw_string(&ssd, "IMU DATALOGGER", 8, 16);
    
    switch (current_state) {
        case STATE_INITIALIZING:
            ssd1306_draw_string(&ssd, "Inicializando...", 10, 28);
            ssd1306_draw_string(&ssd, "Aguarde", 35, 41);
            break;
            
        case STATE_READY:
            if (sd_mounted) {
                ssd1306_draw_string(&ssd, "Sistema Pronto", 15, 28);
                ssd1306_draw_string(&ssd, "A=gravar B=ejetar", 8, 41);
            } else {
                ssd1306_draw_string(&ssd, "SD Nao Detectado", 8, 28);
                ssd1306_draw_string(&ssd, "Insira cartao SD", 12, 41);
            }
            break;
            
        case STATE_RECORDING:
            ssd1306_draw_string(&ssd, "GRAVANDO...", 25, 28);
            
            snprintf(line, sizeof(line), "Amostras: %lu", sample_count);
            ssd1306_draw_string(&ssd, line, 10, 41);
            
            uint32_t recording_time = (to_ms_since_boot(get_absolute_time()) - recording_start_time) / 1000;
            snprintf(line, sizeof(line), "Tempo: %lus", recording_time);
            ssd1306_draw_string(&ssd, line, 30, 52);
            break;
            
        case STATE_ERROR:
            ssd1306_draw_string(&ssd, "ERRO!", 45, 28);
            ssd1306_draw_string(&ssd, "Verifique SD", 15, 41);
            ssd1306_draw_string(&ssd, "B=tentar novamente", 5, 52);
            break;
    }
    
    ssd1306_send_data(&ssd);
}

//Outras Funções
bool start_recording(void) {
    if (!sd_mounted) {
        printf("Erro: SD não montado!\n");
        current_state = STATE_ERROR;
        buzzer_play_sequence(BUZZER_ERROR);
        return false;
    }
    
    // Indicação visual imediata
    interface_sd_access_indication(true);
    
    if (sdlogger_start(imu_log_filename)) {
        is_recording = true;
        sample_count = 0;
        recording_start_time = to_ms_since_boot(get_absolute_time());
        current_state = STATE_RECORDING;
        
        // Finaliza indicação de acesso ao SD
        interface_sd_access_indication(false);
        
        // Feedback sonoro
        buzzer_play_sequence(BUZZER_START_RECORDING);
        printf("Gravação iniciada! (%s)\n", imu_log_filename);
        
        return true;
    } else {
        interface_sd_access_indication(false);
        current_state = STATE_ERROR;
        buzzer_play_sequence(BUZZER_ERROR);
        printf("Erro ao iniciar gravação!\n");
        return false;
    }
}

void stop_recording(void) {
    if (is_recording) {
        interface_sd_access_indication(true);
        
        sdlogger_stop();
        is_recording = false;
        current_state = STATE_READY;
        
        interface_sd_access_indication(false);
        
        buzzer_play_sequence(BUZZER_STOP_RECORDING);
        printf("Gravação parada! Total: %lu amostras\n", sample_count);
    }
}

void toggle_recording(void) {
    if (is_recording) {
        stop_recording();
    } else {
        start_recording();
    }
}

bool mount_sd(void) {
    printf("Montando SD...\n");
    interface_sd_access_indication(true);
    
    run_mount();
    sd_mounted = check_sd_status(); // ALTERAÇÃO: Usa nova função de verificação
    if (sd_mounted) {
        current_state = STATE_READY;
        buzzer_play_sequence(BUZZER_SD_MOUNT);
        printf("SD montado com sucesso!\n");
    } else {
        current_state = STATE_ERROR;
        buzzer_play_sequence(BUZZER_ERROR);
        printf("Falha ao montar SD!\n");
    }
    
    interface_sd_access_indication(false);
    return sd_mounted;
}

void unmount_sd(void) {
    if (is_recording) {
        stop_recording();
        printf("Gravação interrompida devido ao desmonte do SD.\n");
    }
    
    printf("Desmontando SD...\n");
    interface_sd_access_indication(true);
    
    run_unmount();
    sd_mounted = false;
    current_state = STATE_READY;
    
    buzzer_play_sequence(BUZZER_SD_UNMOUNT);
    printf("SD desmontado!\n");
    
    interface_sd_access_indication(false);
}

void process_serial_command(char cmd) {
    switch (cmd) {
        case 's': // Start/Stop recording
            toggle_recording();
            break;
            
        case 'm': // Mount SD
            mount_sd();
            break;
            
        case 'u': // Unmount SD
            unmount_sd();
            break;
            
        case 'l': // List files
            if (sd_mounted) {
                interface_sd_access_indication(true);
                run_ls();
                interface_sd_access_indication(false);
            } else {
                printf("SD não montado. Monte primeiro com 'm'.\n");
            }
            break;
            
        case 'h': // Help
            printf("\n=== COMANDOS DISPONÍVEIS ===\n");
            printf("s - Iniciar/Parar gravação do IMU\n");
            printf("m - Montar SD card\n");
            printf("u - Desmontar SD card\n");
            printf("l - Listar arquivos no SD\n");
            printf("h - Mostrar ajuda\n");
            printf("=============================\n\n");
            break;
            
        default:
            printf("Comando não reconhecido. Digite 'h' para ajuda.\n");
            break;
    }
}

void process_buttons(void) {
    // Botão A: Iniciar/Parar gravação
    if (button_a_get_pressed()) {
        printf("Botão A pressionado - Toggle Recording\n");
        toggle_recording();
    }
    
    // Botão B: Desmontar SD quando montado, ou tentar montar quando não montado
    if (button_b_get_pressed()) {
        printf("Botão B pressionado - SD Management\n");
        if (sd_mounted) {
            // Se SD montado, desmonta
            unmount_sd();
        } else {
            // Se SD não montado, tenta montar
            mount_sd();
        }
    }
}

void capture_imu_sample(void) {
    if (!is_recording) return;
    
    int16_t accel[3], gyro[3];
    
    if (imu_read_raw(accel, gyro)) {
        sample_count++;
        
        // Indicação rápida de acesso ao SD
        interface_sd_access_indication(true);
        bool success = sdlogger_log_sample(sample_count, accel, gyro);
        interface_sd_access_indication(false);
        
        if (!success) {
            printf("Erro ao gravar amostra %lu. Parando gravação.\n", sample_count);
            stop_recording();
            current_state = STATE_ERROR;
            return;
        }
        
        // Debug reduzido para não impactar performance
        if (sample_count % 1000 == 0) { // A cada 10 segundos a 100Hz
            printf("Amostra %lu: ACC[%d,%d,%d] GYRO[%d,%d,%d]\n", 
                   sample_count, accel[0], accel[1], accel[2], 
                   gyro[0], gyro[1], gyro[2]);
        }
    } else {
        printf("Erro na leitura do IMU na amostra %lu\n", sample_count);
        static uint8_t error_count = 0;
        error_count++;
        if (error_count >= 5) { // Mais tolerante a erros pontuais
            printf("Muitos erros de leitura do IMU. Parando gravação.\n");
            stop_recording();
            current_state = STATE_ERROR;
            error_count = 0;
        }
    }
}
// ============================

int main(void) {
    stdio_init_all();
    sleep_ms(2000);
    
    printf("IMU DATALOGGER\n");
    printf("CEPEDI - TIC37 - Feira de Santana\n");
    printf("Iniciando sistema...\n\n");
    
    current_state = STATE_INITIALIZING;
    
    // Inicializa perifericos
    display_init();
    display_update();
    interface_init();
    interface_update_state(current_state, sd_mounted, sample_count);
    
    // Sinal de inicialização
    buzzer_play_sequence(BUZZER_INIT);
    sleep_ms(500);
    
    // Inicializa IMU
    printf("Inicializando IMU MPU6050...\n");
    bi_decl(bi_2pins_with_func(I2C_SDA, I2C_SCL, GPIO_FUNC_I2C));
    imu_init();
    
    // Testa leitura do IMU
    int16_t test_acc[3], test_gyro[3];
    if (imu_read_raw(test_acc, test_gyro)) {
        printf("✓ IMU inicializado com sucesso!\n");
        printf("  Teste - ACC: %d %d %d | GYRO: %d %d %d\n", 
               test_acc[0], test_acc[1], test_acc[2], 
               test_gyro[0], test_gyro[1], test_gyro[2]);
    } else {
        printf("✗ Erro na inicialização do IMU!\n");
        current_state = STATE_ERROR;
        buzzer_play_sequence(BUZZER_ERROR);
    }
    
    // NOVA: Tenta montar SD automaticamente na inicialização
    printf("Verificando SD card...\n");
    auto_mount_sd();
    
    // Se chegou aqui e não teve erro no IMU, está pronto
    if (current_state != STATE_ERROR) {
        current_state = STATE_READY;
    }
    
    printf("SISTEMA PRONTO\n");
    printf("Comandos: 's'=gravar, 'm'=montar SD, 'h'=ajuda\n");
    printf("Botões: A=gravar, B=SD\n");
    printf("Taxa de amostragem: %d Hz\n", SAMPLE_RATE_HZ);
    
    // Variáveis de timing otimizadas
    uint32_t last_display_update = 0;
    uint32_t last_sample_time = 0;
    uint32_t last_interface_update = 0;
    uint32_t last_sd_check = 0;
    uint32_t last_button_check = 0;

    while (true) {
        uint32_t current_time = to_ms_since_boot(get_absolute_time());
        
        // MÁXIMA PRIORIDADE: Atualização da interface (LEDs) - 20ms para responsividade
        if (current_time - last_interface_update >= 20) {
            interface_update_state(current_state, sd_mounted, sample_count);
            last_interface_update = current_time;
        }

        // ALTA PRIORIDADE: Verificação de botões - 50ms para responsividade
        if (current_time - last_button_check >= 50) {
            process_buttons();
            last_button_check = current_time;
        }
        
        // PRIORIDADE MÉDIA: Comandos seriais (verificação a cada loop)
        int c = getchar_timeout_us(0);
        if (c != PICO_ERROR_TIMEOUT) {
            process_serial_command((char)c);
        }
        
        // PRIORIDADE MÉDIA: Atualização do display - 200ms
        if (current_time - last_display_update >= 200) {
            display_update();
            last_display_update = current_time;
        }
        
        // PRIORIDADE ALTA: Captura de amostras IMU - conforme SAMPLE_INTERVAL_MS
        if (current_time - last_sample_time >= SAMPLE_INTERVAL_MS) {
            capture_imu_sample();
            last_sample_time = current_time;
        }
        
        // BAIXA PRIORIDADE: Verificação periódica do SD - 1000ms
        if (current_time - last_sd_check >= 1000) {
            bool previous_sd_state = sd_mounted;
            if (sd_mounted) {
                // Indica acesso ao SD durante verificação
                interface_sd_access_indication(true);
                sd_mounted = check_sd_status();
                interface_sd_access_indication(false);
                
                if (!sd_mounted && previous_sd_state) {
                    printf("SD card removido!\n");
                    if (is_recording) {
                        stop_recording();
                        printf("Gravação interrompida - SD removido!\n");
                    }
                    current_state = STATE_READY; // Volta ao estado ready mas sem SD (ficará roxo)
                }
            } else {
                auto_mount_sd();
            }
            last_sd_check = current_time;
        }
        
        // Sleep mínimo para não sobrecarregar o processador
        sleep_us(500); // 500 microssegundos ao invés de 1ms
    }
    
    return 0;
}