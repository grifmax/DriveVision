/**
 * @file distillation.h
 * @brief Управление процессом дистилляции
 * 
 * Этот модуль управляет процессом дистилляции (перегонки) спирта-сырца,
 * включая фазы: нагрев, отбор (с возможностью отделения голов).
 */

#ifndef DISTILLATION_H
#define DISTILLATION_H

#include <Arduino.h>

// Фазы дистилляции
enum DistillationPhase {
    DIST_PHASE_IDLE = 0,         // Процесс не запущен
    DIST_PHASE_HEATING,          // Нагрев до рабочей температуры
    DIST_PHASE_DISTILLATION,     // Отбор продукта
    DIST_PHASE_COMPLETED,        // Процесс завершен
    DIST_PHASE_ERROR             // Ошибка в процессе
};

/**
 * @brief Инициализация подсистемы дистилляции
 */
void initDistillation();

/**
 * @brief Запуск процесса дистилляции
 * 
 * @return true если процесс успешно запущен
 */
bool startDistillation();

/**
 * @brief Остановка процесса дистилляции
 */
void stopDistillation();

/**
 * @brief Пауза процесса дистилляции
 */
void pauseDistillation();

/**
 * @brief Возобновление процесса дистилляции после паузы
 */
void resumeDistillation();

/**
 * @brief Обработка процесса дистилляции, вызывается в основном цикле
 */
void processDistillation();

/**
 * @brief Получение текущей фазы дистилляции
 * 
 * @return Текущая фаза процесса
 */
DistillationPhase getDistillationPhase();

/**
 * @brief Получение имени текущей фазы дистилляции
 * 
 * @return Имя фазы как строка
 */
const char* getDistillationPhaseName();

/**
 * @brief Проверка, запущен ли процесс дистилляции
 * 
 * @return true если процесс запущен
 */
bool isDistillationRunning();

/**
 * @brief Проверка, на паузе ли процесс дистилляции
 * 
 * @return true если процесс на паузе
 */
bool isDistillationPaused();

/**
 * @brief Получение объема собранного продукта
 * 
 * @return Объем продукта в миллилитрах
 */
int getDistillationProductVolume();

/**
 * @brief Получение объема собранных голов (если включено отделение голов)
 * 
 * @return Объем голов в миллилитрах
 */
int getDistillationHeadsVolume();

/**
 * @brief Проверка, находится ли процесс в режиме отбора голов
 * 
 * @return true если идет отбор голов
 */
bool isDistillationHeadsMode();

/**
 * @brief Получение общего времени работы процесса
 * 
 * @return Время работы в секундах
 */
unsigned long getDistillationUptime();

/**
 * @brief Получение времени работы текущей фазы
 * 
 * @return Время работы фазы в секундах
 */
unsigned long getDistillationPhaseTime();

/**
 * @brief Получение текущей температуры куба
 * 
 * @return Температура в градусах Цельсия
 */
float getDistillationCubeTemp();

/**
 * @brief Получение текущей температуры колонны
 * 
 * @return Температура в градусах Цельсия
 */
float getDistillationColumnTemp();

/**
 * @brief Получение текущей температуры продукта
 * 
 * @return Температура в градусах Цельсия
 */
float getDistillationProductTemp();

// Приватные функции (не включаются в заголовочный файл для внешнего использования)
void processDistHeatingPhase();
void processDistillationPhase();
bool checkDistillationSafety();
void setDistillationPhase(DistillationPhase phase);

#endif // DISTILLATION_H