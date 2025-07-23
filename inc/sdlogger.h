#ifndef SDLOGGER_H
#define SDLOGGER_H

#include <stdbool.h>
#include <stdint.h>
#include "ff.h" 
#include "sd_card.h"


#ifdef __cplusplus
extern "C" {
#endif

// Funções de ajuda
sd_card_t *sd_get_by_name(const char *const name);
FATFS *sd_get_fs_by_name(const char *name);

// Funções de comando (para interação com o SD Card)
void run_setrtc();
void run_format();
void run_mount();
void run_unmount();
void run_getfree();
void run_ls();
void run_cat();

// Funções de aplicação
void capture_adc_data_and_save();
void read_file(const char *filename);
bool sdlogger_start(const char *log_filename); 
bool sdlogger_log_sample(uint32_t sample_num, int16_t accel[3], int16_t gyro[3]); 
void sdlogger_stop();

// Função de ajuda do CLI
void run_help();

#ifdef __cplusplus
}
#endif

#endif // SDLOGGER_H