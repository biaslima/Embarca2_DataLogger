#ifndef INTERFACE_H
#define INTERFACE_H

#include "pico/stdlib.h"

// ... (seus defines para pinos, etc.)
#define LED_RED_PIN 13
#define LED_GREEN_PIN 11
#define LED_BLUE_PIN 12
#define BUTTON_A_PIN 5
#define BUTTON_B_PIN 6
#define BUZZER_PIN 21

// Adicionar novos estados para montagem/desmontagem
typedef enum {
    STATE_INITIALIZING,
    STATE_READY,
    STATE_RECORDING,
    STATE_ERROR,
    STATE_MOUNTING_SD,   // Novo estado para montagem do SD
    STATE_UNMOUNTING_SD  // Novo estado para desmontagem do SD
} system_state_t;

// Tipos de cor RGB (ainda que a função set_color não seja mais usada diretamente, pode ser útil manter)
typedef enum {
    RGB_OFF,
    RGB_RED,
    RGB_GREEN,
    RGB_BLUE,
    RGB_YELLOW,
    RGB_PURPLE
} rgb_color_t;

// Sequências de buzzer
typedef enum {
    BUZZER_INIT,
    BUZZER_START_RECORDING,
    BUZZER_STOP_RECORDING,
    BUZZER_ERROR,
    BUZZER_SD_MOUNT,
    BUZZER_SD_UNMOUNT
} buzzer_sequence_t;

// Funções da interface
void interface_init(void);

// Botões
bool button_a_get_pressed(void);
bool button_b_get_pressed(void);
void button_irq_handler(uint gpio, uint32_t events);

// Buzzer
void buzzer_init(void);
void buzzer_beep(uint16_t frequency, uint16_t duration_ms);
void buzzer_play_sequence(buzzer_sequence_t sequence);

// Indicação de acesso ao SD (para o LED azul)
void interface_sd_access_indication(bool accessing);

// Atualização do estado geral da interface (LEDs)
void interface_update_state(system_state_t state, bool sd_mounted, bool recording_active);

#endif // INTERFACE_H