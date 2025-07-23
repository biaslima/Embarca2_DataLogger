#include "../inc/sdlogger.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hardware/adc.h"
#include "hardware/rtc.h"
#include "pico/stdlib.h"

#include "diskio.h"
#include "f_util.h"
#include "hw_config.h"
#include "my_debug.h"
#include "rtc.h"

// Variáveis estáticas para gerenciamento do arquivo de log
static FIL log_file;
static bool logging_active = false;
static char current_log_filename[FF_LFN_BUF];

// Funções de ajuda
sd_card_t *sd_get_by_name(const char *const name) {
    for (size_t i = 0; i < sd_get_num(); ++i) {
        if (0 == strcmp(sd_get_by_num(i)->pcName, name)) {
            return sd_get_by_num(i);
        }
    }
    DBG_PRINTF("%s: unknown name %s\n", __func__, name);
    return NULL;
}

FATFS *sd_get_fs_by_name(const char *name) {
    for (size_t i = 0; i < sd_get_num(); ++i) {
        if (0 == strcmp(sd_get_by_num(i)->pcName, name)) {
            return &sd_get_by_num(i)->fatfs;
        }
    }
    DBG_PRINTF("%s: unknown name %s\n", __func__, name);
    return NULL;
}

// Funções de comando (para interação com o SD Card)
void run_setrtc() {
    const char *dateStr = strtok(NULL, " ");
    if (!dateStr) {
        printf("Missing argument\n");
        return;
    }
    int date = atoi(dateStr);

    const char *monthStr = strtok(NULL, " ");
    if (!monthStr) {
        printf("Missing argument\n");
        return;
    }
    int month = atoi(monthStr);

    const char *yearStr = strtok(NULL, " ");
    if (!yearStr) {
        printf("Missing argument\n");
        return;
    }
    int year = atoi(yearStr) + 2000;

    const char *hourStr = strtok(NULL, " ");
    if (!hourStr) {
        printf("Missing argument\n");
        return;
    }
    int hour = atoi(hourStr);

    const char *minStr = strtok(NULL, " ");
    if (!minStr) {
        printf("Missing argument\n");
        return;
    }
    int min = atoi(minStr);

    const char *secStr = strtok(NULL, " ");
    if (!secStr) {
        printf("Missing argument\n");
        return;
    }
    int sec = atoi(secStr);

    datetime_t t = {
        .year = (int16_t)year,
        .month = (int8_t)month,
        .day = (int8_t)date,
        .dotw = 0, // 0 is Sunday
        .hour = (int8_t)hour,
        .min = (int8_t)min,
        .sec = (int8_t)sec
    };
    rtc_set_datetime(&t);
}

void run_format() {
    const char *arg1 = strtok(NULL, " ");
    if (!arg1) {
        arg1 = sd_get_by_num(0)->pcName;
    }
    FATFS *p_fs = sd_get_fs_by_name(arg1);
    if (!p_fs) {
        printf("Unknown logical drive number: \"%s\"\n", arg1);
        return;
    }
    /* Format the drive with default parameters */
    FRESULT fr = f_mkfs(arg1, 0, 0, FF_MAX_SS * 2);
    if (FR_OK != fr) {
        printf("f_mkfs error: %s (%d)\n", FRESULT_str(fr), fr);
    }
}

void run_mount() {
    const char *arg1 = strtok(NULL, " ");
    if (!arg1) {
        arg1 = sd_get_by_num(0)->pcName;
    }
    FATFS *p_fs = sd_get_fs_by_name(arg1);
    if (!p_fs) {
        printf("Unknown logical drive number: \"%s\"\n", arg1);
        return;
    }
    FRESULT fr = f_mount(p_fs, arg1, 1);
    if (FR_OK != fr) {
        printf("f_mount error: %s (%d)\n", FRESULT_str(fr), fr);
        return;
    }
    sd_card_t *pSD = sd_get_by_name(arg1);
    myASSERT(pSD);
    pSD->mounted = true;
    printf("Processo de montagem do SD ( %s ) concluído\n", pSD->pcName);
}

void run_unmount() {
    const char *arg1 = strtok(NULL, " ");
    if (!arg1) {
        arg1 = sd_get_by_num(0)->pcName;
    }
    FATFS *p_fs = sd_get_fs_by_name(arg1);
    if (!p_fs) {
        printf("Unknown logical drive number: \"%s\"\n", arg1);
        return;
    }
    FRESULT fr = f_unmount(arg1);
    if (FR_OK != fr) {
        printf("f_unmount error: %s (%d)\n", FRESULT_str(fr), fr);
        return;
    }
    sd_card_t *pSD = sd_get_by_name(arg1);
    myASSERT(pSD);
    pSD->mounted = false;
    pSD->m_Status |= STA_NOINIT; // in case medium is removed
    printf("SD ( %s ) desmontado\n", pSD->pcName);
}

void run_getfree() {
    const char *arg1 = strtok(NULL, " ");
    if (!arg1) {
        arg1 = sd_get_by_num(0)->pcName;
    }
    DWORD fre_clust, fre_sect, tot_sect;
    FATFS *p_fs = sd_get_fs_by_name(arg1);
    if (!p_fs) {
        printf("Unknown logical drive number: \"%s\"\n", arg1);
        return;
    }
    FRESULT fr = f_getfree(arg1, &fre_clust, &p_fs);
    if (FR_OK != fr) {
        printf("f_getfree error: %s (%d)\n", FRESULT_str(fr), fr);
        return;
    }
    tot_sect = (p_fs->n_fatent - 2) * p_fs->csize;
    fre_sect = fre_clust * p_fs->csize;
    printf("%10lu KiB total drive space.\n%10lu KiB available.\n", tot_sect / 2, fre_sect / 2);
}

void run_ls() {
    const char *arg1 = strtok(NULL, " ");
    if (!arg1) {
        arg1 = "";
    }
    char cwdbuf[FF_LFN_BUF] = {0};
    FRESULT fr;
    char const *p_dir;
    if (arg1[0]) {
        p_dir = arg1;
    } else {
        fr = f_getcwd(cwdbuf, sizeof cwdbuf);
        if (FR_OK != fr) {
            printf("f_getcwd error: %s (%d)\n", FRESULT_str(fr), fr);
            return;
        }
        p_dir = cwdbuf;
    }
    printf("Directory Listing: %s\n", p_dir);
    DIR dj;
    FILINFO fno;
    memset(&dj, 0, sizeof dj);
    memset(&fno, 0, sizeof fno);
    fr = f_findfirst(&dj, &fno, p_dir, "*");
    if (FR_OK != fr) {
        printf("f_findfirst error: %s (%d)\n", FRESULT_str(fr), fr);
        return;
    }
    while (fr == FR_OK && fno.fname[0]) {
        const char *pcWritableFile = "writable file",
                   *pcReadOnlyFile = "read only file",
                   *pcDirectory = "directory";
        const char *pcAttrib;
        if (fno.fattrib & AM_DIR) {
            pcAttrib = pcDirectory;
        } else if (fno.fattrib & AM_RDO) {
            pcAttrib = pcReadOnlyFile;
        } else {
            pcAttrib = pcWritableFile;
        }
        printf("%s [%s] [size=%llu]\n", fno.fname, pcAttrib, fno.fsize);

        fr = f_findnext(&dj, &fno);
    }
    f_closedir(&dj);
}

void run_cat() {
    char *arg1 = strtok(NULL, " ");
    if (!arg1) {
        printf("Missing argument\n");
        return;
    }
    FIL fil;
    FRESULT fr = f_open(&fil, arg1, FA_READ);
    if (FR_OK != fr) {
        printf("f_open error: %s (%d)\n", FRESULT_str(fr), fr);
        return;
    }
    char buf[256];
    while (f_gets(buf, sizeof buf, &fil)) {
        printf("%s", buf);
    }
    fr = f_close(&fil);
    if (FR_OK != fr) {
        printf("f_open error: %s (%d)\n", FRESULT_str(fr), fr);
    }
}
void capture_adc_data_and_save() {
 printf("\nCapturando dados do ADC. Aguarde finalização...\n");
    FIL file;
    // Tenta abrir o arquivo com o nome "adc_data1.csv"
    // Use 'filename' global aqui, se ainda for para um propósito geral de ADC
    FRESULT res = f_open(&file, "adc_data1.csv", FA_WRITE | FA_CREATE_ALWAYS);
    if (res != FR_OK) {
        printf("\n[ERRO] Não foi possível abrir o arquivo para escrita. Monte o Cartao.\n");
        return;
    }

    // Escreve o cabeçalho CSV na primeira linha do arquivo
    f_printf(&file, "sample_number,adc_value\n");

    for (int i = 0; i < 128; i++) {
        adc_select_input(0);
        uint16_t adc_value = adc_read();
        char buffer[50];
        // Formata os dados com vírgula como separador para CSV
        sprintf(buffer, "%d,%d\n", i + 1, adc_value);
        UINT bw;
        res = f_write(&file, buffer, strlen(buffer), &bw);
        if (res != FR_OK) {
            printf("[ERRO] Não foi possível escrever no arquivo. Monte o Cartao.\n");
            f_close(&file);
            return;
        }
        sleep_ms(100);
    }
    f_close(&file);
    printf("\nDados do ADC salvos no arquivo adc_data1.csv.\n\n"); // Atualizar mensagem
}


void read_file(const char *filename_to_read) { // Renomear parâmetro para evitar conflito com global
    FIL file;
    FRESULT res = f_open(&file, filename_to_read, FA_READ);
    if (res != FR_OK) {
        printf("[ERRO] Não foi possível abrir o arquivo para leitura. Verifique se o Cartão está montado ou se o arquivo existe.\n");
        return;
    }
    char buffer[128];
    UINT br;
    printf("Conteúdo do arquivo %s:\n", filename_to_read);
    while (f_read(&file, buffer, sizeof(buffer) - 1, &br) == FR_OK && br > 0) {
        buffer[br] = '\0';
        printf("%s", buffer);
    }
    f_close(&file);
    printf("\nLeitura do arquivo %s concluída.\n\n", filename_to_read);
}


//Inicia a sessão de log do IMU
bool sdlogger_start(const char *log_filename) {
    if (logging_active) {
        printf("[AVISO] O logger já está ativo. Pare o log atual antes de iniciar um novo.\n");
        return false;
    }

    // Copia o nome do arquivo para a variável estática
    strncpy(current_log_filename, log_filename, sizeof(current_log_filename) - 1);
    current_log_filename[sizeof(current_log_filename) - 1] = '\0'; // Garante null termination

    FRESULT res = f_open(&log_file, current_log_filename, FA_WRITE | FA_CREATE_ALWAYS); // Abre ou cria o arquivo 
    if (res != FR_OK) {
        printf("[ERRO] Falha ao abrir o arquivo de log '%s': %s (%d)\n", current_log_filename, FRESULT_str(res), res);
        return false;
    }

    // Escreve o cabeçalho CSV conforme o enunciado 
    f_printf(&log_file, "numero_amostra,accel_x,accel_y,accel_z,giro_x,giro_y,giro_z\n");
    logging_active = true;
    printf("Log iniciado em '%s'\n", current_log_filename);
    return true;
}

//Loga uma amostra do IMU no arquivo CSV
bool sdlogger_log_sample(uint32_t sample_num, int16_t accel[3], int16_t gyro[3]) { 
    if (!logging_active) {
        printf("[AVISO] O logger não está ativo. Inicie o log antes de gravar amostras.\n");
        return false;
    }

    // Formata a linha de dados conforme o enunciado [cite: 26, 47]
    FRESULT res = f_printf(&log_file, "%lu,%d,%d,%d,%d,%d,%d\n",
                           sample_num,
                           accel[0], accel[1], accel[2],
                           gyro[0], gyro[1], gyro[2]); 

    if (res < 0) { // f_printf retorna o número de bytes escritos ou um erro negativo
        printf("[ERRO] Falha ao escrever no arquivo de log: %s (%d)\n", FRESULT_str((FRESULT)res), (FRESULT)res);
        return false;
    }
    return true;
}

//Para a sessão de log
void sdlogger_stop() {
    if (logging_active) {
        f_close(&log_file); // Fecha o arquivo
        logging_active = false;
        printf("Log encerrado para '%s'.\n", current_log_filename);
    } else {
        printf("[AVISO] O logger não estava ativo para ser parado.\n");
    }
}

// Função de ajuda do CLI (mantida como está)
void run_help() {
    printf("\nComandos disponíveis:\n\n");
    printf("Digite 'a' para montar o cartão SD\n");
    printf("Digite 'b' para desmontar o cartão SD\n");
    printf("Digite 'c' para listar arquivos\n");
    printf("Digite 'd' para mostrar conteúdo do arquivo\n");
    printf("Digite 'e' para obter espaço livre no cartão SD\n");
    printf("Digite 'f' para capturar dados do ADC e salvar no arquivo\n");
    printf("Digite 'g' para formatar o cartão SD\n");
    printf("Digite 'h' para exibir os comandos disponíveis\n");
    printf("\nEscolha o comando:  ");
}