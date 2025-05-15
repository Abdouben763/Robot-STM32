#ifndef ROBOT_H
#define ROBOT_H

#include "main.h"
#include <stdarg.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ==========================
// Définitions et constantes
// ==========================
#define TICKS_PAR_TOUR     20
#define TICKS_PAR_ROTATION 40
#define D_ROUE             0.065f       // Diamètre en m
#define D_roue_cent        6.5f         // Distance entre les roues en cm
#define PI                 3.1416f

// ============================
// Définition de l'état du robot
// ============================
typedef enum {
    IDLE,
    AVANCER,
    ARRIERE,
    TOURNER_GAUCHE,
    TOURNER_DROITE,
    AUTO
} RobotState;

// =========================
// Variables globales externes
// =========================

// --- Périphériques ---
extern ADC_HandleTypeDef hadc;
extern TIM_HandleTypeDef htim2;
extern TIM_HandleTypeDef htim3;
extern TIM_HandleTypeDef htim4;
extern TIM_HandleTypeDef htim6;
extern UART_HandleTypeDef huart2;
extern UART_HandleTypeDef huart3;

// --- État et commandes ---
extern RobotState current_state;
extern int target_ticks;
extern float target_angle;
extern char last_command;
extern uint8_t auto_enabled;

// --- Encodeurs ---
extern uint32_t encodeur1_cnt;
extern uint32_t encodeur2_cnt;
extern uint32_t encodeur_L;
extern uint32_t encodeur_R;

// --- Ultrasons ---
extern uint32_t IC_Val1;
extern uint32_t IC_Val2;
extern uint32_t Difference;
extern uint8_t Is_First_Captured;
extern uint8_t Distance_u;

// --- UART ---
extern uint8_t rx_data;
extern char rx_buffer[64];
extern uint8_t rx_index;
extern volatile uint8_t RX_busy;
extern volatile uint8_t TX_busy;
extern volatile uint8_t RX_ready;
extern char uart_buffer[50];
extern int uart_buffer_len;

// --- Mesures ---
extern int battery_level;
extern int speed;
extern float distance_G;
extern float distance_D;
extern float distance;
extern float Angle;
extern float distancetotale;

// --- Flags ---
extern volatile int robot_tourne;

// ==========================
// Fonctions
// ==========================

// --- Déplacement moteurs ---
void move_m1(GPIO_PinState dir, uint8_t duty_cycle);
void move_m2(GPIO_PinState dir, uint8_t duty_cycle);
void stop_robot(void);

// --- Déplacements haut niveau ---
void Avancer(float distance);
void Arriere(float distance);
void Tourner(float Target);
void move_forward_robot(void);
void move_backward_robot(void);
void left_rotation_robot(void);
void right_rotation_robot(void);

// --- Mesures capteurs ---
uint32_t Measure_battery(void);
void delay_us(uint16_t delay);
void HAL_TIM_IC_CaptureCallback(TIM_HandleTypeDef *htim);
void Trig_read(void);

// --- Mode automatique ---
void auto_mode(void);

// --- Communication UART ---
void Rx_commandes(void);
void start_uart_receive(void);
void update_and_transmit_data(void);

// --- Callback GPIO (interruptions) ---
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin);

#endif /* ROBOT_H */
