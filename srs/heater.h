#ifndef HEATER_H
#define HEATER_H

#include <Arduino.h>
#include "config.h"

// Инициализация нагревателя
void initHeater();

// Включение нагревателя
void enableHeater();

// Выключение нагревателя
void disableHeater();

// Установка мощности нагрева в процентах (0-100%)
void setHeaterPower(int powerPercent);

// Установка мощности нагрева в ваттах
void setHeaterPowerWatts(int powerWatts);

// Получение текущей мощности нагрева в процентах
int getHeaterPowerPercent();

// Получение текущей мощности нагрева в ваттах
int getHeaterPowerWatts();

// Обновление состояния нагревателя
void updateHeater();

// Проверка, включен ли нагреватель
bool isHeaterEnabled();

// Аварийное выключение нагревателя
void emergencyHeaterShutdown(const char* reason);

#endif // HEATER_H