#include "../inc/interface.h"
#include "../inc/ssd1306.h"
#include "../inc/font.h"
#include "pico/stdlib.h"
#include "hardware/pwm.h"
#include <stdio.h>

// Variáveis globais para controle dos periféricos
static uint buzzer_slice, buzzer_channel;

static volatile bool button_a_pressed = false;
static volatile bool button_b_pressed = false;
static uint32_t last_button_time = 0;
static const uint32_t DEBOUNCE_TIME_MS = 200;

// Estados do sistema para LEDs
static system_state_t current_led_state = STATE_INITIALIZING;

// INICIALIZAÇÃO 
void interface_init(void) {
    // Inicializa LEDs RGB
    gpio_init(LED_RED_PIN);
    gpio_set_dir(LED_RED_PIN, GPIO_OUT);
    gpio_put(LED_RED_PIN, false);
    
    gpio_init(LED_GREEN_PIN);
    gpio_set_dir(LED_GREEN_PIN, GPIO_OUT);
    gpio_put(LED_GREEN_PIN, false);
    
    gpio_init(LED_BLUE_PIN);
    gpio_set_dir(LED_BLUE_PIN, GPIO_OUT);
    gpio_put(LED_BLUE_PIN, false);
    
    // Inicializa botões
    gpio_init(BUTTON_A_PIN);
    gpio_set_dir(BUTTON_A_PIN, GPIO_IN);
    gpio_pull_up(BUTTON_A_PIN);
    
    gpio_init(BUTTON_B_PIN);
    gpio_set_dir(BUTTON_B_PIN, GPIO_IN);
    gpio_pull_up(BUTTON_B_PIN);
    
    // Configura interrupções dos botões
    gpio_set_irq_enabled_with_callback(BUTTON_A_PIN, GPIO_IRQ_EDGE_FALL, true, &button_irq_handler);
    gpio_set_irq_enabled_with_callback(BUTTON_B_PIN, GPIO_IRQ_EDGE_FALL, true, &button_irq_handler);
    
    // Inicializa buzzer
    buzzer_init();
    printf("Interface inicializada com sucesso!\n");
}

// ============== BUZZER ==============
void buzzer_init(void) {
    gpio_set_function(BUZZER_PIN, GPIO_FUNC_PWM);
    buzzer_slice = pwm_gpio_to_slice_num(BUZZER_PIN);
    buzzer_channel = pwm_gpio_to_channel(BUZZER_PIN);
    
    pwm_set_clkdiv(buzzer_slice, 125.0f);
    pwm_set_wrap(buzzer_slice, 1000);
    pwm_set_enabled(buzzer_slice, false);
}

void buzzer_beep(uint16_t frequency, uint16_t duration_ms) {
    if (frequency == 0) return;
    
    uint32_t wrap = 1000000 / frequency;
    pwm_set_wrap(buzzer_slice, wrap);
    pwm_set_chan_level(buzzer_slice, buzzer_channel, wrap / 2);
    pwm_set_enabled(buzzer_slice, true);
    
    sleep_ms(duration_ms);
    
    pwm_set_enabled(buzzer_slice, false);
}

void buzzer_play_sequence(buzzer_sequence_t sequence) {
    switch (sequence) {
        case BUZZER_INIT:
            buzzer_beep(440, 100);
            break;
            
        case BUZZER_START_RECORDING:
            buzzer_beep(800, 150);
            break;
            
        case BUZZER_STOP_RECORDING:
            buzzer_beep(600, 100);
            sleep_ms(50);
            buzzer_beep(600, 100);
            break;
            
        case BUZZER_ERROR:
            buzzer_beep(200, 200);
            sleep_ms(100);
            buzzer_beep(200, 200);
            sleep_ms(100);
            buzzer_beep(200, 200);
            break;
            
        case BUZZER_SD_MOUNT:
            buzzer_beep(1000, 80);
            sleep_ms(30);
            buzzer_beep(1200, 80);
            break;
            
        case BUZZER_SD_UNMOUNT:
            buzzer_beep(800, 100);
            break;
    }
}

// ============== LEDs RGB ==============
void rgb_led_set_color(rgb_color_t color) {
    gpio_put(LED_RED_PIN, false);
    gpio_put(LED_GREEN_PIN, false);
    gpio_put(LED_BLUE_PIN, false);
    
    switch (color) {
        case RGB_OFF:
            break;
            
        case RGB_RED:
            gpio_put(LED_RED_PIN, true);
            break;
            
        case RGB_GREEN:
            gpio_put(LED_GREEN_PIN, true);
            break;
            
        case RGB_BLUE:
            gpio_put(LED_BLUE_PIN, true);
            break;
            
        case RGB_YELLOW:
            gpio_put(LED_RED_PIN, true);
            gpio_put(LED_GREEN_PIN, true);
            break;
            
        case RGB_PURPLE:
            gpio_put(LED_RED_PIN, true);
            gpio_put(LED_BLUE_PIN, true);
            break;
    }
}

void rgb_led_update_state(system_state_t state) {
    current_led_state = state;
    
    switch (state) {
        case STATE_INITIALIZING:
            rgb_led_set_color(RGB_YELLOW);
            break;
            
        case STATE_READY:
            rgb_led_set_color(RGB_GREEN);
            break;
            
        case STATE_RECORDING:
            rgb_led_set_color(RGB_RED);
            break;
            
        case STATE_ERROR:
            rgb_led_set_color(RGB_PURPLE);
            break;
    }
}

void rgb_led_blink(rgb_color_t color, uint16_t blink_count, uint16_t interval_ms) {
    for (uint16_t i = 0; i < blink_count; i++) {
        rgb_led_set_color(color);
        sleep_ms(interval_ms);
        rgb_led_set_color(RGB_OFF);
        sleep_ms(interval_ms);
    }
    
    // Retorna ao estado atual
    rgb_led_update_state(current_led_state);
}
// ============== BOTÕES ==============
void button_irq_handler(uint gpio, uint32_t events) {
    uint32_t current_time = to_ms_since_boot(get_absolute_time());
    
    // Debounce
    if (current_time - last_button_time < DEBOUNCE_TIME_MS) {
        return;
    }
    last_button_time = current_time;
    
    if (gpio == BUTTON_A_PIN && (events & GPIO_IRQ_EDGE_FALL)) {
        button_a_pressed = true;
    } else if (gpio == BUTTON_B_PIN && (events & GPIO_IRQ_EDGE_FALL)) {
        button_b_pressed = true;
    }
}

bool button_a_get_pressed(void) {
    if (button_a_pressed) {
        button_a_pressed = false;
        return true;
    }
    return false;
}

bool button_b_get_pressed(void) {
    if (button_b_pressed) {
        button_b_pressed = false;
        return true;
    }
    return false;
}

// ============== ATUALIZAÇÃO GERAL ==============
void interface_update_state(system_state_t state, bool sd_mounted, uint32_t sample_count) {
    if (state == STATE_READY && !sd_mounted) {
        rgb_led_blink(RGB_PURPLE, 1, 200);
    } else {
        rgb_led_update_state(state);
    }
}
void interface_sd_access_indication(bool accessing) {
    if (accessing) {
        rgb_led_blink(RGB_BLUE, 1, 100);
    }
}