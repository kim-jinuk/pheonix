
#ifndef CONTROLMOTOR_HPP
#define CONTROLMOTOR_HPP

#include <string>
#include <mutex>

extern std::mutex motor_mutex;


void InitMotor();
void UpdateMotor(int dx, int dy);
int GetMotorX();
int GetMotorY();
int AngleToDuty(int angle);
void SetDuty(int ch, int duty_ns);

#endif
