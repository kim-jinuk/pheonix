#pragma once

// PWM 초기화: export + 초기 duty 설정
void pwm_init();

// PWM 제어: x, y 각도 (0~180)
void pwm(int angle_x, int angle_y);
