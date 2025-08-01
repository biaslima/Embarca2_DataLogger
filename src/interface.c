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
static uint32_t last_button_a_time = 0;
static uint32_t last_button_b_time = 0;
static const uint32_t DEBOUNCE_TIME_MS = 200;

// Estados do sistema para LEDs
static system_state_t current_led_state = STATE_INITIALIZING;
static bool is_sd_accessing = false;
static bool is_system_recording = false;
static bool sd_is_mounted = false;

// Controle de piscadas
static uint32_t last_blink_time = 0;
static bool blink_state = false;

//Inicializa os pinos de LEDs e interrupções.
void interface_init(void) {
    gpio_init(LED_RED_PIN);
    gpio_set_dir(LED_RED_PIN, GPIO_OUT);
    gpio_put(LED_RED_PIN, false);
    
    gpio_init(LED_GREEN_PIN);
    gpio_set_dir(LED_GREEN_PIN, GPIO_OUT);
    gpio_put(LED_GREEN_PIN, false);
    
    gpio_init(LED_BLUE_PIN);
    gpio_set_dir(LED_BLUE_PIN, GPIO_OUT);
    gpio_put(LED_BLUE_PIN, false);
    
    gpio_init(BUTTON_A_PIN);
    gpio_set_dir(BUTTON_A_PIN, GPIO_IN);
    gpio_pull_up(BUTTON_A_PIN);
    
    gpio_init(BUTTON_B_PIN);
    gpio_set_dir(BUTTON_B_PIN, GPIO_IN);
    gpio_pull_up(BUTTON_B_PIN);
    
    gpio_set_irq_enabled_with_callback(BUTTON_A_PIN, GPIO_IRQ_EDGE_FALL, true, &button_irq_handler);
    gpio_set_irq_enabled_with_callback(BUTTON_B_PIN, GPIO_IRQ_EDGE_FALL, true, &button_irq_handler);
    
    buzzer_init();
}

//Inicializa o buzzer 
void buzzer_init(void) {
    gpio_set_function(BUZZER_PIN, GPIO_FUNC_PWM);
    buzzer_slice = pwm_gpio_to_slice_num(BUZZER_PIN);
    buzzer_channel = pwm_gpio_to_channel(BUZZER_PIN);
    
    pwm_set_clkdiv(buzzer_slice, 125.0f);
    uint32_t wrap = 1000;
    pwm_set_wrap(buzzer_slice, wrap);
    pwm_set_chan_level(buzzer_slice, buzzer_channel, wrap / 2);
    pwm_set_enabled(buzzer_slice, false);
}

//Gera um beep no buzzer
void buzzer_beep(uint16_t frequency, uint16_t duration_ms) {
    if (frequency == 0) return;
    
    uint32_t wrap = 1000000 / frequency;
    pwm_set_wrap(buzzer_slice, wrap);
    pwm_set_chan_level(buzzer_slice, buzzer_channel, wrap / 2);
    pwm_set_enabled(buzzer_slice, true);
    
    sleep_ms(duration_ms);
    
    pwm_set_enabled(buzzer_slice, false);
}

//Toca uma sequência de bips predefinida
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
            buzzer_beep(200, 300);
            sleep_ms(100);
            buzzer_beep(200, 300);
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

//Desliga todos os LEDs 
static void rgb_led_turn_off_all(void) {
    gpio_put(LED_RED_PIN, false);
    gpio_put(LED_GREEN_PIN, false);
    gpio_put(LED_BLUE_PIN, false);
}

//Define o estado individual dos LEDs 
static void rgb_led_set_individual(bool red, bool green, bool blue) {
    gpio_put(LED_RED_PIN, red);
    gpio_put(LED_GREEN_PIN, green);
    gpio_put(LED_BLUE_PIN, blue);
}

//Manipulador de interrupção para os botões.
void button_irq_handler(uint gpio, uint32_t events) {
    uint32_t current_time = to_ms_since_boot(get_absolute_time());
    
    if (gpio == BUTTON_A_PIN && (events & GPIO_IRQ_EDGE_FALL)) {
        if (current_time - last_button_a_time >= DEBOUNCE_TIME_MS) {
            button_a_pressed = true;
            last_button_a_time = current_time;
        }
    } else if (gpio == BUTTON_B_PIN && (events & GPIO_IRQ_EDGE_FALL)) {
        if (current_time - last_button_b_time >= DEBOUNCE_TIME_MS) {
            button_b_pressed = true;
            last_button_b_time = current_time;
        }
    }
}

//Verifica se o botão A foi pressionado 
bool button_a_get_pressed(void) {
    if (button_a_pressed) {
        button_a_pressed = false;
        return true;
    }
    return false;
}

//Verifica se o botão B foi pressionado 
bool button_b_get_pressed(void) {
    if (button_b_pressed) {
        button_b_pressed = false;
        return true;
    }
    return false;
}

//Define o estado de acesso ao SD card 
void interface_sd_access_indication(bool accessing) {
    is_sd_accessing = accessing;
}

//Atualiza o estado dos LEDs RGB com base no estado atual do sistema.
void interface_update_state(system_state_t state, bool sd_mounted, bool recording_active) {
    current_led_state = state;
    is_system_recording = recording_active;
    sd_is_mounted = sd_mounted;
    uint32_t current_time = to_ms_since_boot(get_absolute_time());
    
    rgb_led_turn_off_all();

    if (current_led_state == STATE_ERROR) {
        if (current_time - last_blink_time >= 300) {
            blink_state = !blink_state;
            rgb_led_set_individual(blink_state, false, blink_state);
            last_blink_time = current_time;
        }
    } else if (is_system_recording) {
        if (current_time - last_blink_time >= 300) {
            blink_state = !blink_state;
            if (blink_state) {
                rgb_led_set_individual(true, false, false);
            } else {
                rgb_led_set_individual(false, false, true);
            }
            last_blink_time = current_time;
        }
    } else if (current_led_state == STATE_MOUNTING_SD || current_led_state == STATE_UNMOUNTING_SD) {
        rgb_led_set_individual(true, true, false);
        blink_state = false;
    } else if (current_led_state == STATE_INITIALIZING) {
        rgb_led_set_individual(true, true, false);
        blink_state = false;
    } else if (current_led_state == STATE_READY) {
        if (sd_is_mounted) {
            rgb_led_set_individual(false, true, false);
            blink_state = false;
        } else {
            rgb_led_set_individual(true, false, true);
            blink_state = false;
        }
    } else if (is_sd_accessing) {
        if (current_time - last_blink_time >= 100) {
            blink_state = !blink_state;
            gpio_put(LED_BLUE_PIN, blink_state);
            last_blink_time = current_time;
        }
    } else {
        rgb_led_turn_off_all();
        blink_state = false;
    }
}