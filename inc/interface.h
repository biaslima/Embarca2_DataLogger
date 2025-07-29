#ifndef INTERFACE_H
#define INTERFACE_H

#include "pico/stdlib.h"
#include <stdint.h>
#include <stdbool.h>

// ============== DEFINIÇÕES DE PINOS ==============

// LEDs RGB
#define LED_RED_PIN     13
#define LED_GREEN_PIN   11
#define LED_BLUE_PIN    12
// Botões
#define BUTTON_A_PIN    5
#define BUTTON_B_PIN    6
// Buzzer
#define BUZZER_PIN      21


// ============== ENUMERAÇÕES ==============

// Estados do sistema
typedef enum {
    STATE_INITIALIZING,
    STATE_READY,
    STATE_RECORDING,
    STATE_ERROR
} system_state_t;

// Cores do LED RGB
typedef enum {
    RGB_OFF,
    RGB_RED,
    RGB_GREEN,
    RGB_BLUE,
    RGB_YELLOW,
    RGB_PURPLE,
    RGB_CYAN,
    RGB_WHITE
} rgb_color_t;

// Sequências do buzzer
typedef enum {
    BUZZER_INIT,
    BUZZER_START_RECORDING,
    BUZZER_STOP_RECORDING,
    BUZZER_ERROR,
    BUZZER_SD_MOUNT,
    BUZZER_SD_UNMOUNT
} buzzer_sequence_t;

// ============== FUNÇÕES PRINCIPAIS ==============

void interface_init(void);
void interface_update_state(system_state_t state, bool sd_mounted, uint32_t sample_count);
void interface_sd_access_indication(bool accessing);
void buzzer_init(void);
void buzzer_beep(uint16_t frequency, uint16_t duration_ms);
void buzzer_play_sequence(buzzer_sequence_t sequence);

// ============== FUNÇÕES DOS LEDs RGB ==============

void rgb_led_set_color(rgb_color_t color);
void rgb_led_update_state(system_state_t state);
void rgb_led_blink(rgb_color_t color, uint16_t blink_count, uint16_t interval_ms);

// ============== FUNÇÕES DOS BOTÕES ==============

void button_irq_handler(uint gpio, uint32_t events);
bool button_a_get_pressed(void);
bool button_b_get_pressed(void);

#endif // INTERFACE_H