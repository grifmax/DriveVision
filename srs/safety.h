/**
 * @file safety.h
 * @brief Система безопасности контроллера
 * 
 * Модуль обеспечивает комплексную систему безопасности,
 * включая контроль температуры, давления, времени работы
 * и другие критические параметры.
 */

#ifndef SAFETY_H
#define SAFETY_H

#include <Arduino.h>

// Коды ошибок безопасности
enum SafetyErrorCode {
    SAFETY_OK = 0,                      // Нет ошибок
    SAFETY_ERROR_TEMPERATURE_HIGH,      // Превышение максимальной температуры
    SAFETY_ERROR_TEMPERATURE_RISE,      // Слишком быстрый рост температуры
    SAFETY_ERROR_SENSOR_DISCONNECT,     // Отключение датчика температуры
    SAFETY_ERROR_WATER_FLOW_LOW,        // Низкий поток охлаждающей воды
    SAFETY_ERROR_MAX_RUNTIME_EXCEEDED,  // Превышено максимальное время работы
    SAFETY_ERROR_POWER_ISSUE,           // Проблемы с питанием
    SAFETY_ERROR_EMERGENCY_STOP,        // Аварийная остановка
    SAFETY_ERROR_WATCHDOG_TIMEOUT,      // Срабатывание сторожевого таймера
    SAFETY_ERROR_PRESSURE_HIGH          // Высокое давление
};

// Структура для хранения состояния безопасности
struct SafetyStatus {
    bool isSystemSafe;                  // Общее состояние безопасности
    SafetyErrorCode errorCode;          // Код последней ошибки
    unsigned long errorTime;            // Время возникновения ошибки
    String errorDescription;            // Описание ошибки
    bool isSensorError;                 // Флаг ошибки датчика
    bool isTemperatureError;            // Флаг ошибки температуры
    bool isWaterFlowError;              // Флаг ошибки потока воды
    bool isPressureError;               // Флаг ошибки давления
    bool isRuntimeError;                // Флаг ошибки времени работы
    bool isEmergencyStop;               // Флаг аварийной остановки
    bool isWatchdogReset;               // Флаг сброса по сторожевому таймеру
};

/**
 * @brief Инициализация системы безопасности
 * 
 * @return true если инициализация прошла успешно
 */
bool initSafety();

/**
 * @brief Проверка общего состояния безопасности
 * 
 * Проверяет все параметры безопасности и возвращает общее состояние.
 * 
 * @return true если система в безопасном состоянии
 */
bool isSafetyOK();

/**
 * @brief Получение статуса безопасности
 * 
 * @return Структура с текущим состоянием безопасности
 */
SafetyStatus getSafetyStatus();

/**
 * @brief Сброс ошибок безопасности
 * 
 * Сбрасывает все ошибки безопасности, если это возможно.
 * 
 * @return true если все ошибки успешно сброшены
 */
bool resetSafetyErrors();

/**
 * @brief Проверка безопасности для процесса дистилляции
 * 
 * @param cubeTemp Текущая температура куба
 * @param waterOutTemp Текущая температура выхода воды
 * @return SafetyErrorCode: SAFETY_OK если все в порядке, иначе код ошибки
 */
SafetyErrorCode checkDistillationSafety(float cubeTemp, float waterOutTemp);

/**
 * @brief Проверка безопасности для процесса ректификации
 * 
 * @param cubeTemp Текущая температура куба
 * @param columnTemp Текущая температура колонны
 * @param refluxTemp Текущая температура в узле отбора
 * @param waterOutTemp Текущая температура выхода воды
 * @param tsaTemp Текущая температура ТСА (если используется)
 * @return SafetyErrorCode: SAFETY_OK если все в порядке, иначе код ошибки
 */
SafetyErrorCode checkRectificationSafety(float cubeTemp, float columnTemp, float refluxTemp, float waterOutTemp, float tsaTemp);

/**
 * @brief Обновление системы безопасности
 * 
 * Выполняет периодические проверки безопасности и обновляет состояние.
 * Вызывается в основном цикле программы.
 */
void updateSafety();

/**
 * @brief Аварийная остановка
 * 
 * Немедленно останавливает все процессы и выключает все нагревательные элементы.
 * 
 * @param reason Причина аварийной остановки
 */
void emergencyStop(const String &reason);

/**
 * @brief Получение описания ошибки безопасности
 * 
 * @param errorCode Код ошибки
 * @return Текстовое описание ошибки
 */
String getSafetyErrorDescription(SafetyErrorCode errorCode);

/**
 * @brief Установка максимального времени непрерывной работы (в часах)
 * 
 * @param hours Максимальное время работы в часах
 */
void setSafetyMaxRuntime(int hours);

/**
 * @brief Установка максимальной температуры куба
 * 
 * @param maxTemp Максимальная температура в градусах Цельсия
 */
void setSafetyMaxCubeTemp(float maxTemp);

/**
 * @brief Установка максимальной скорости изменения температуры
 * 
 * @param maxRatePerMinute Максимальная скорость изменения в градусах в минуту
 */
void setSafetyMaxTempRiseRate(float maxRatePerMinute);

/**
 * @brief Установка минимальной температуры выхода воды для охлаждения
 * 
 * @param minTemp Минимальная температура в градусах Цельсия
 */
void setSafetyMinWaterOutTemp(float minTemp);

/**
 * @brief Установка максимальной температуры выхода воды для охлаждения
 * 
 * @param maxTemp Максимальная температура в градусах Цельсия
 */
void setSafetyMaxWaterOutTemp(float maxTemp);

/**
 * @brief Запуск сторожевого таймера
 * 
 * @param timeoutSeconds Таймаут срабатывания в секундах
 */
void startSafetyWatchdog(int timeoutSeconds);

/**
 * @brief Сброс сторожевого таймера
 */
void resetSafetyWatchdog();

#endif // SAFETY_H