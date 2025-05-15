/* robot.c
 *
 *  Created on: May 7, 2025
 *      Author: numidia
 */

#include "main.h"
#include "robot.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* -------------------- DEFINES -------------------- */

#define TICKS_PAR_TOUR 20
#define TICKS_PAR_ROTATION 40
#define D_ROUE 0.065
#define D_roue_cent 6.5
#define PI 3.1416

/* -------------------- VARIABLES GLOBALES -------------------- */

// Ultrasons
uint32_t IC_Val1 = 0;
uint32_t IC_Val2 = 0;
uint32_t Difference = 0;
uint8_t Is_First_Captured = 0;
uint8_t Distance_u = 0;

// États du robot
RobotState current_state = IDLE;
int target_ticks = 0;
float target_angle = 0.0f;
uint8_t auto_enabled = 0;

// Encodeurs
uint32_t encodeur1_cnt = 0;
uint32_t encodeur2_cnt = 0;
uint32_t encodeur_L = 0;
uint32_t encodeur_R = 0;

// UART
uint8_t rx_data;
char rx_buffer[64];
uint8_t rx_index = 0;
char uart_buffer[50];
int uart_buffer_len = 0;
char last_command = '\0';
volatile uint8_t RX_busy = 0;
volatile uint8_t TX_busy = 0;
volatile uint8_t RX_ready = 0;

// Mouvement
int speed = 50;
uint8_t duty = 10;
volatile int robot_tourne = 0;

// Distance & batterie
int battery_level;
float distance_G = 0.0f;
float distance_D = 0.0f;
float distance = 0.0f;
float distancetotale = 0.0f;
float Angle = 0.0f;

/* -------------------- FONCTIONS UTILITAIRES -------------------- */

uint32_t Measure_battery(void) {
    HAL_ADC_Start(&hadc);
    HAL_ADC_PollForConversion(&hadc, 20);
    uint32_t adc_value = HAL_ADC_GetValue(&hadc);
    float V_shunt = (adc_value * 3.3f) / 4096.0f;
    float gain_diviseur = (10000.0f + 4700.0f) / 4700.0f;
    float V_batterie = gain_diviseur * V_shunt;
    return (int)((V_batterie * 100.0f) / 8.4f);
}



void update_and_transmit_data(void) {
    battery_level = Measure_battery();

    uart_buffer_len = snprintf(uart_buffer, sizeof(uart_buffer), "%.2f;%d\r\n", distancetotale, battery_level);

    if (uart_buffer_len > 0 && uart_buffer_len < sizeof(uart_buffer)) {
        if (TX_busy == 0) {
            TX_busy = 1;
            HAL_UART_Transmit_IT(&huart3, (uint8_t *)uart_buffer, uart_buffer_len);
        }
    }
}

/* -------------------- CONTROLE DES MOTEURS -------------------- */

void move_m1(GPIO_PinState dir, uint8_t duty_cycle) {
    if (duty_cycle > 80) return;
    HAL_GPIO_WritePin(DIR1_GPIO_Port, DIR1_Pin, dir);
    TIM3->CCR2 = duty_cycle;
}

void move_m2(GPIO_PinState dir, uint8_t duty_cycle) {
    if (duty_cycle > 80) return;
    HAL_GPIO_WritePin(DIR2_GPIO_Port, DIR2_Pin, dir);
    TIM4->CCR1 = duty_cycle;
}

void stop_robot(void) {
    Angle = 0;
    encodeur1_cnt = encodeur2_cnt = encodeur_L = encodeur_R = 0;
    TIM3->CCR2 = 0;
    TIM4->CCR1 = 0;
}

/* -------------------- FONCTIONS DE DEPLACEMENT -------------------- */

void move_forward_robot(void) {
    Angle = 0;
    encodeur1_cnt = encodeur2_cnt = encodeur_L = encodeur_R = 0;
    move_m1(1, duty);
    move_m2(1, duty);
}

void move_backward_robot(void) {
    Angle = 0;
    encodeur1_cnt = encodeur2_cnt = encodeur_L = encodeur_R = 0;
    move_m1(0, duty);
    move_m2(0, duty);
}

void left_rotation_robot(void) {
    encodeur1_cnt = encodeur2_cnt = encodeur_L = encodeur_R = 0;
    move_m1(1, duty);
    move_m2(0, duty);
}

void right_rotation_robot(void) {
    encodeur1_cnt = encodeur2_cnt = encodeur_L = encodeur_R = 0;
    move_m1(0, duty);
    move_m2(1, duty);
}

void Avancer(float distance) {
    robot_tourne = 0;
    encodeur_L = encodeur_R = 0;
    target_ticks = (int)((distance / (PI * D_ROUE)) * TICKS_PAR_TOUR);
    move_m1(1, duty);
    move_m2(1, duty);
    current_state = AVANCER;
}

void Arriere(float distance) {
    robot_tourne = 0;
    encodeur_L = encodeur_R = 0;
    target_ticks = (int)((distance / (PI * D_ROUE)) * TICKS_PAR_TOUR);
    move_m1(0, duty);
    move_m2(0, duty);
    current_state = ARRIERE;
}

void Tourner(float angle) {
    robot_tourne = 1;
    encodeur1_cnt = encodeur2_cnt = 0;
    target_angle = angle;
    if (angle < 0) {
        move_m1(1, duty);
        move_m2(0, duty);
        current_state = TOURNER_GAUCHE;
    } else {
        move_m1(0, duty);
        move_m2(1, duty);
        current_state = TOURNER_DROITE;
    }
}

/* -------------------- MODE AUTOMATIQUE -------------------- */

void auto_mode(void) {
    uint32_t somme = 0;
    for (int i = 0; i < 5; i++) {
        Trig_read();
        delay_us(30000);
        somme += Distance_u;
    }

    uint32_t moyenne = somme / 5;
    if (moyenne < 10) {
        Tourner(90.0);
    } else {
        Avancer(0.1);
    }
}

/* -------------------- COMMANDES UART -------------------- */

void Rx_commandes(void) {
    if (current_state != IDLE || rx_buffer[0] == 0) return;

    if (rx_buffer[0] == 'V') {
        duty = (uint8_t)atoi(&rx_buffer[1]);
        return;
    }

    if (rx_buffer[0] == 'A') {
        auto_enabled = 1;
        return;
    }
    auto_enabled = 0;

    switch (rx_buffer[0]) {
        case 'F': robot_tourne = 0; move_forward_robot(); break;
        case 'L': robot_tourne = 1; left_rotation_robot(); break;
        case 'R': robot_tourne = 1; right_rotation_robot(); break;
        case 'S': robot_tourne = 0;  stop_robot(); break;
        case 'U': robot_tourne = 0; move_backward_robot(); break;
        case 'O': robot_tourne = 0; Avancer(0.1); break;
        case 'K': robot_tourne = 1; Tourner(-90.0); break;
        case 'M': robot_tourne = 1; Tourner(90.0); break;
        case 'B': robot_tourne = 0; Arriere(0.1); break;
        default: break;
    }

    rx_buffer[0] = 0;
}

void start_uart_receive(void) {
    HAL_UART_Receive_IT(&huart3, &rx_data, 1);
}

/* -------------------- INTERRUPTIONS & CALLBACKS -------------------- */

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin) {
    float delta = (PI * D_ROUE) / TICKS_PAR_TOUR;

    if (!robot_tourne) {
        if (GPIO_Pin == ENC1_Pin) {
            encodeur_L++;
            distance_G = delta * encodeur_L;
            distancetotale += delta / 2.0f;
        }
        if (GPIO_Pin == ENC2_Pin) {
            encodeur_R++;
            distance_D = delta * encodeur_R;
            distancetotale += delta / 2.0f;
        }
        distance = (distance_D + distance_G) / 2.0f;
    } else {
        if (GPIO_Pin == ENC1_Pin && HAL_GPIO_ReadPin(DIR1_GPIO_Port, DIR1_Pin) == GPIO_PIN_RESET) {
            encodeur1_cnt++;
            Angle = ((float)encodeur1_cnt / TICKS_PAR_ROTATION) * 360.0f;
        }
        if (GPIO_Pin == ENC2_Pin && HAL_GPIO_ReadPin(DIR2_GPIO_Port, DIR2_Pin) == GPIO_PIN_RESET) {
            encodeur2_cnt++;
            Angle = -((float)encodeur2_cnt / TICKS_PAR_ROTATION) * 360.0f;
        }
    }
}

/* -------------------- CAPTEUR ULTRASONS -------------------- */

void delay_us(uint16_t delay) {
    __HAL_TIM_SET_COUNTER(&htim2, 0);
    while (__HAL_TIM_GET_COUNTER(&htim2) < delay);
}

void HAL_TIM_IC_CaptureCallback(TIM_HandleTypeDef *htim) {
    if (htim->Channel == HAL_TIM_ACTIVE_CHANNEL_1) {
        if (!Is_First_Captured) {
            IC_Val1 = HAL_TIM_ReadCapturedValue(htim, TIM_CHANNEL_1);
            Is_First_Captured = 1;
            __HAL_TIM_SET_CAPTUREPOLARITY(htim, TIM_CHANNEL_1, TIM_INPUTCHANNELPOLARITY_FALLING);
        } else {
            IC_Val2 = HAL_TIM_ReadCapturedValue(htim, TIM_CHANNEL_1);
            __HAL_TIM_SET_COUNTER(htim, 0);

            Difference = (IC_Val2 > IC_Val1) ? (IC_Val2 - IC_Val1) : ((0xFFFF - IC_Val1) + IC_Val2);
            Distance_u = Difference * 0.034 / 2;

            Is_First_Captured = 0;
            __HAL_TIM_SET_CAPTUREPOLARITY(htim, TIM_CHANNEL_1, TIM_INPUTCHANNELPOLARITY_RISING);
            __HAL_TIM_DISABLE_IT(htim, TIM_IT_CC1);
        }
    }
}

void Trig_read(void) {
    HAL_GPIO_WritePin(GPIOA, Trig_Pin, GPIO_PIN_SET);
    delay_us(10);
    HAL_GPIO_WritePin(GPIOA, Trig_Pin, GPIO_PIN_RESET);
    __HAL_TIM_ENABLE_IT(&htim2, TIM_IT_CC1);
}
