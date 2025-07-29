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

// CONFIGURAÇÕES DO DISPLAY
#define I2C_PORT_DISP i2c1
#define I2C_SDA_DISP 14
#define I2C_SCL_DISP 15
#define ENDERECO_DISP 0x3C

// CONFIGURAÇÕES DO SISTEMA
#define SAMPLE_RATE_HZ 10
#define SAMPLE_INTERVAL_MS (1000 / SAMPLE_RATE_HZ)
#define DISPLAY_UPDATE_INTERVAL_MS 250
#define SD_CHECK_INTERVAL_MS 500

// VARIÁVEIS GLOBAIS
static system_state_t current_state = STATE_INITIALIZING;
static bool is_recording = false;
static bool sd_mounted = false;
static uint32_t sample_count = 0;
static uint32_t recording_start_time = 0;
static const char *imu_log_filename = "imu_data.csv";
static ssd1306_t ssd;

// ESTADOS PARA DISPLAY DO SD
typedef enum {
    SD_STATE_IDLE,
    SD_STATE_MOUNTING,
    SD_STATE_UNMOUNTING,
    SD_STATE_MOUNT_SUCCESS,
    SD_STATE_MOUNT_FAIL,
    SD_STATE_UNMOUNT_SUCCESS
} sd_display_status_t;

static sd_display_status_t sd_display_status = SD_STATE_IDLE;
static uint32_t sd_display_status_time = 0;
static const uint32_t SD_DISPLAY_STATUS_DURATION_MS = 1500; 

// PROTÓTIPOS DE FUNÇÕES
bool check_sd_status(void);
void display_init(void);
void display_update(void);
bool start_recording(void);
void stop_recording(void);
void toggle_recording(void);
bool mount_sd(void);
void unmount_sd(void);
void process_serial_command(char cmd);
void process_buttons(void);
void capture_imu_sample(void);

int main(void) {
    stdio_init_all();
    sleep_ms(2000);
    
    printf("IMU DATALOGGER\n");
    printf("CEPEDI - TIC37 - Feira de Santana\n");
    printf("Iniciando sistema...\n\n");
    
    current_state = STATE_INITIALIZING;
    
    display_init();
    interface_init();
    
    buzzer_play_sequence(BUZZER_INIT);
    sleep_ms(500);
    
    printf("Inicializando IMU MPU6050...\n");
    bi_decl(bi_2pins_with_func(I2C_SDA, I2C_SCL, GPIO_FUNC_I2C));
    imu_init();
    
    int16_t test_acc[3], test_gyro[3];
    if (imu_read_raw(test_acc, test_gyro)) {
        printf("IMU inicializado com sucesso!\n");
    } else {
        printf("Erro na inicialização do IMU!\n");
        current_state = STATE_ERROR;
        buzzer_play_sequence(BUZZER_ERROR);
    }
    
    if (current_state != STATE_ERROR) {
        current_state = STATE_READY;
    }
    
    printf("SISTEMA PRONTO\n");
    printf("Comandos: 's'=gravar, 'm'=montar SD (serial), 'h'=ajuda\n");
    printf("Botões: A=gravar, B=SD (Montar/Desmontar)\n");
    printf("Taxa de amostragem: %d Hz\n", SAMPLE_RATE_HZ);
    
    uint32_t last_display_update = 0;
    uint32_t last_sample_time = 0;
    uint32_t last_interface_update = 0;
    uint32_t last_sd_check = 0;
    uint32_t last_button_check = 0;

    while (true) {
        uint32_t current_time = to_ms_since_boot(get_absolute_time());
        
        if (current_time - last_interface_update >= 20) {
            interface_update_state(current_state, sd_mounted, is_recording);
            last_interface_update = current_time;
        }

        if (current_time - last_button_check >= 50) {
            process_buttons();
            last_button_check = current_time;
        }
        
        int c = getchar_timeout_us(0);
        if (c != PICO_ERROR_TIMEOUT) {
            process_serial_command((char)c);
        }
        
        if (current_time - last_display_update >= DISPLAY_UPDATE_INTERVAL_MS) {
            display_update();
            last_display_update = current_time;
        }
        
        if (current_time - last_sample_time >= SAMPLE_INTERVAL_MS) {
            capture_imu_sample();
            last_sample_time = current_time;
        }
        
        if (current_time - last_sd_check >= 1000) {
            bool previous_sd_state = sd_mounted;
            if (sd_mounted) {
                interface_sd_access_indication(true);
                sd_mounted = check_sd_status();
                interface_sd_access_indication(false);
                
                if (!sd_mounted && previous_sd_state) {
                    if (is_recording) {
                        stop_recording();
                    }
                    current_state = STATE_READY;
                }
            }
            last_sd_check = current_time;
        }
        
        sleep_us(500);
    }
    
    return 0;
}

//Verifica se o SD card está fisicamente presente e montado.
bool check_sd_status(void) {
    DWORD fre_clust, fre_sect, tot_sect;
    FATFS *p_fs = sd_get_fs_by_name(sd_get_by_num(0)->pcName);
    if (!p_fs) return false;
    
    FRESULT fr = f_getfree(sd_get_by_num(0)->pcName, &fre_clust, &p_fs);
    return (fr == FR_OK);
}

//Inicializa o display OLED.
void display_init(void) {
    i2c_init(I2C_PORT_DISP, 400 * 1000);
    gpio_set_function(I2C_SDA_DISP, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL_DISP, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA_DISP);
    gpio_pull_up(I2C_SCL_DISP);

    ssd1306_init(&ssd, WIDTH, HEIGHT, false, ENDERECO_DISP, I2C_PORT_DISP);
    ssd1306_config(&ssd);
}

//Atualiza o conteúdo exibido no display OLED com base no estado do sistema.
void display_update(void) {
    char line[20];
    uint32_t current_time = to_ms_since_boot(get_absolute_time());

    ssd1306_fill(&ssd, false);
    ssd1306_rect(&ssd, 3, 3, 122, 60, true, false);
    ssd1306_line(&ssd, 3, 25, 123, 25, true);
    ssd1306_line(&ssd, 3, 37, 123, 37, true);
    
    ssd1306_draw_string(&ssd, "CEPEDI   TIC37", 8, 6);
    ssd1306_draw_string(&ssd, "IMU DATALOGGER", 8, 16);
    
    if (sd_display_status != SD_STATE_IDLE && 
        (current_time - sd_display_status_time < SD_DISPLAY_STATUS_DURATION_MS)) {
        switch (sd_display_status) {
            case SD_STATE_MOUNTING:
                ssd1306_draw_string(&ssd, "Montando SD...", 10, 28);
                ssd1306_draw_string(&ssd, "Aguarde", 35, 41);
                break;
            case SD_STATE_UNMOUNTING:
                ssd1306_draw_string(&ssd, "Desmontando SD...", 5, 28);
                ssd1306_draw_string(&ssd, "Aguarde", 35, 41);
                break;
            case SD_STATE_MOUNT_SUCCESS:
                ssd1306_draw_string(&ssd, "SD Montado!", 25, 28);
                ssd1306_draw_string(&ssd, "Pronto p/ uso", 15, 41);
                break;
            case SD_STATE_MOUNT_FAIL:
                ssd1306_draw_string(&ssd, "Falha Montar SD!", 5, 28);
                ssd1306_draw_string(&ssd, "Verifique cartao", 8, 41);
                break;
            case SD_STATE_UNMOUNT_SUCCESS:
                ssd1306_draw_string(&ssd, "SD Desmontado!", 10, 28);
                ssd1306_draw_string(&ssd, "Remova c/ seg", 10, 41);
                break;
            default:
                break;
        }
    } else {
        sd_display_status = SD_STATE_IDLE;
        switch (current_state) {
            case STATE_INITIALIZING:
                ssd1306_draw_string(&ssd, "Inicializando...", 10, 28);
                ssd1306_draw_string(&ssd, "Aguarde", 35, 41);
                break;
            
            case STATE_MOUNTING_SD:
                ssd1306_draw_string(&ssd, "Montando SD...", 10, 28);
                ssd1306_draw_string(&ssd, "Aguarde", 35, 41);
                break;
            case STATE_UNMOUNTING_SD:
                ssd1306_draw_string(&ssd, "Desmontando SD...", 5, 28);
                ssd1306_draw_string(&ssd, "Aguarde", 35, 41);
                break;
                
            case STATE_READY:
                if (sd_mounted) {
                    ssd1306_draw_string(&ssd, "Sistema Pronto", 15, 28);
                    ssd1306_draw_string(&ssd, "A=gravar B=ejetar", 8, 41);
                } else {
                    ssd1306_draw_string(&ssd, "SD Nao Montado", 8, 28);
                    ssd1306_draw_string(&ssd, "B=Montar SD", 12, 41);
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
                ssd1306_draw_string(&ssd, "Verifique SD/IMU", 15, 41);
                ssd1306_draw_string(&ssd, "B=tentar nov", 5, 52);
                break;
        }
    }
    ssd1306_send_data(&ssd);
}

//Inicia a gravação de dados do IMU no SD card.
bool start_recording(void) {
    if (!sd_mounted) {
        current_state = STATE_ERROR;
        buzzer_play_sequence(BUZZER_ERROR);
        return false;
    }
    
    interface_sd_access_indication(true);
    
    if (sdlogger_start(imu_log_filename)) {
        is_recording = true;
        sample_count = 0;
        recording_start_time = to_ms_since_boot(get_absolute_time());
        current_state = STATE_RECORDING;
        
        interface_sd_access_indication(false);
        
        buzzer_play_sequence(BUZZER_START_RECORDING);
        return true;
    } else {
        interface_sd_access_indication(false);
        current_state = STATE_ERROR;
        buzzer_play_sequence(BUZZER_ERROR);
        return false;
    }
}

//Para a gravação de dados do IMU.

void stop_recording(void) {
    if (is_recording) {
        interface_sd_access_indication(true);
        sdlogger_stop();
        is_recording = false;
        current_state = STATE_READY;
        interface_sd_access_indication(false);
        buzzer_play_sequence(BUZZER_STOP_RECORDING);
    }
}

//Alterna o estado de gravação (inicia se parado, para se gravando).
void toggle_recording(void) {
    if (is_recording) {
        stop_recording();
    } else {
        start_recording();
    }
}

//Tenta montar o SD card.
bool mount_sd(void) {
    if (sd_mounted) {
        return true;
    }
    
    current_state = STATE_MOUNTING_SD; 
    sd_display_status = SD_STATE_MOUNTING;
    sd_display_status_time = to_ms_since_boot(get_absolute_time());

    interface_update_state(current_state, sd_mounted, is_recording); 
    display_update();

    interface_sd_access_indication(true);
    
    run_mount();
    sd_mounted = check_sd_status();
    
    if (sd_mounted) {
        current_state = STATE_READY;
        buzzer_play_sequence(BUZZER_SD_MOUNT);
        sd_display_status = SD_STATE_MOUNT_SUCCESS;
        sd_display_status_time = to_ms_since_boot(get_absolute_time());
    } else {
        current_state = STATE_ERROR;
        buzzer_play_sequence(BUZZER_ERROR);
        sd_display_status = SD_STATE_MOUNT_FAIL;
        sd_display_status_time = to_ms_since_boot(get_absolute_time());
    }
    
    interface_sd_access_indication(false);
    return sd_mounted;
}

//Desmonta o SD card com segurança.
void unmount_sd(void) {
    if (!sd_mounted) {
        return;
    }
    if (is_recording) {
        stop_recording();
    }
    
    current_state = STATE_UNMOUNTING_SD;
    sd_display_status = SD_STATE_UNMOUNTING;
    sd_display_status_time = to_ms_since_boot(get_absolute_time());

    interface_update_state(current_state, sd_mounted, is_recording); 
    display_update();

    interface_sd_access_indication(true);
    
    run_unmount();
    sd_mounted = false;
    current_state = STATE_READY;
    
    buzzer_play_sequence(BUZZER_SD_UNMOUNT);
    sd_display_status = SD_STATE_UNMOUNT_SUCCESS;
    sd_display_status_time = to_ms_since_boot(get_absolute_time());
    
    interface_sd_access_indication(false);
}

//Processa comandos recebidos via serial.
void process_serial_command(char cmd) {
    switch (cmd) {
        case 's':
            toggle_recording();
            break;
            
        case 'm':
            mount_sd();
            break;
            
        case 'u':
            unmount_sd();
            break;
            
        case 'l':
            if (sd_mounted) {
                interface_sd_access_indication(true);
                run_ls();
                interface_sd_access_indication(false);
            } else {
                printf("SD não montado. Monte primeiro com 'm'.\n");
            }
            break;
            
        case 'h':
            printf("\n=== COMANDOS DISPONÍVEIS ===\n");
            printf("s - Iniciar/Parar gravação do IMU\n");
            printf("m - Montar SD card (serial apenas)\n");
            printf("u - Desmontar SD card (serial apenas)\n");
            printf("l - Listar arquivos no SD\n");
            printf("h - Mostrar ajuda\n");
            printf("=============================\n\n");
            break;
            
        default:
            printf("Comando não reconhecido. Digite 'h' para ajuda.\n");
            break;
    }
}

//Verifica e processa o pressionamento dos botões.
void process_buttons(void) {
    if (button_a_get_pressed()) {
        toggle_recording();
    }
    
    if (button_b_get_pressed()) {
        if (sd_mounted) {
            unmount_sd();
        } else {
            mount_sd();
        }
    }
}

//Captura uma amostra de dados do IMU e a registra no SD card se a gravação estiver ativa.
void capture_imu_sample(void) {
    if (!is_recording) return;
    
    int16_t accel[3], gyro[3];
    
    if (imu_read_raw(accel, gyro)) {
        sample_count++;
        interface_sd_access_indication(true); 
        bool success = sdlogger_log_sample(sample_count, accel, gyro);
        interface_sd_access_indication(false);
        
        if (!success) {
            stop_recording();
            current_state = STATE_ERROR;
            return;
        }
    } else {
        static uint8_t error_count = 0;
        error_count++;
        if (error_count >= 5) {
            stop_recording();
            current_state = STATE_ERROR;
            error_count = 0;
        }
    }
}