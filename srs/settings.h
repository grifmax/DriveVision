/**
 * @file settings.h
 * @brief Управление настройками системы
 * 
 * Этот модуль отвечает за хранение, загрузку и сохранение настроек системы.
 */

#ifndef SETTINGS_H
#define SETTINGS_H

#include <Arduino.h>

// Максимальное количество датчиков температуры
#define MAX_TEMP_SENSORS 5

// Настройки нагревателя
struct HeaterSettings {
    int maxPowerWatts;              // Максимальная мощность нагревателя в ваттах
    int volts;                      // Напряжение питания нагревателя
};

// Настройки насоса
struct PumpSettings {
    float headsFlowRate;            // Скорость отбора голов (мл/мин)
    float bodyFlowRate;             // Скорость отбора тела (мл/мин)
    float tailsFlowRate;            // Скорость отбора хвостов (мл/мин)
    float calibrationFactor;        // Калибровочный коэффициент насоса
};

// Настройки ректификации
struct RectificationSettings {
    int model;                      // Модель процесса ректификации (0 - классическая, 1 - альтернативная)
    int heatingPowerWatts;          // Мощность нагрева в фазе нагрева
    int stabilizationPowerWatts;    // Мощность нагрева в фазе стабилизации
    int bodyPowerWatts;             // Мощность нагрева в фазе отбора тела
    int tailsPowerWatts;            // Мощность нагрева в фазе отбора хвостов
    float headsTemp;                // Температура начала отбора голов
    float bodyTemp;                 // Температура начала отбора тела
    float tailsTemp;                // Температура начала отбора хвостов
    float endTemp;                  // Температура окончания процесса
    float maxCubeTemp;              // Максимальная температура куба
    float tailsCubeTemp;            // Температура куба для перехода к хвостам
    float tempDeltaEndBody;         // Дельта температуры для окончания отбора тела (для альт. модели)
    int stabilizationTime;          // Время стабилизации колонны (минуты)
    int postHeadsStabilizationTime; // Время стабилизации после отбора голов (минуты)
    int headsVolume;                // Объем голов (мл)
    int bodyVolume;                 // Объем тела (мл)
    float refluxRatio;              // Соотношение орошения (R/D)
    int refluxPeriod;               // Период цикла орошения (секунды)
    bool useSameFlowForTails;       // Использовать ту же скорость отбора для хвостов
};

// Настройки дистилляции
struct DistillationSettings {
    int heatingPowerWatts;          // Мощность нагрева в фазе нагрева
    int distillationPowerWatts;     // Мощность нагрева в фазе дистилляции
    float startCollectingTemp;      // Температура начала отбора
    float endTemp;                  // Температура окончания процесса
    float maxCubeTemp;              // Максимальная температура куба
    bool separateHeads;             // Отделять головы
    int headsVolume;                // Объем голов (мл)
    float flowRate;                 // Скорость отбора (мл/мин)
    float headsFlowRate;            // Скорость отбора голов (мл/мин)
};

// Настройки безопасности
struct SafetySettings {
    int maxRuntimeHours;          // Максимальное время непрерывной работы в часах
    float maxCubeTemp;            // Максимальная температура куба
    float maxTempRiseRate;        // Максимальная скорость изменения температуры
    float minWaterOutTemp;        // Минимальная температура выхода воды
    float maxWaterOutTemp;        // Максимальная температура выхода воды
    bool emergencyStopEnabled;    // Включен ли аварийный останов
    bool watchdogEnabled;         // Включен ли сторожевой таймер
};

// Основная структура настроек системы
struct SystemSettings {
    // Версия настроек для совместимости при обновлениях
    uint32_t settingsVersion;
    
    // Информация о датчиках температуры
    byte tempSensorAddresses[MAX_TEMP_SENSORS][8];  // Адреса датчиков
    bool tempSensorEnabled[MAX_TEMP_SENSORS];       // Статус датчиков (включен/выключен)
    float tempSensorCalibration[MAX_TEMP_SENSORS];  // Калибровочное значение для датчиков
    
    // Настройки нагревателя
    HeaterSettings heaterSettings;
    
    // Настройки насоса
    PumpSettings pumpSettings;
    
    // Настройки ректификации
    RectificationSettings rectificationSettings;
    
    // Настройки дистилляции
    DistillationSettings distillationSettings;
    
    // Настройки безопасности
    SafetySettings safetySettings;
    
    // Настройки сети
    char wifiSsid[32];      // SSID WiFi сети
    char wifiPassword[64];  // Пароль WiFi сети
    bool useAccessPoint;    // Использовать режим точки доступа
};

// Глобальный экземпляр настроек
extern SystemSettings sysSettings;

/**
 * @brief Инициализация системы настроек
 * 
 * @return true если инициализация прошла успешно
 */
bool initSettings();

/**
 * @brief Загрузка настроек из энергонезависимой памяти
 * 
 * @return true если загрузка прошла успешно
 */
bool loadSystemSettings();

/**
 * @brief Сохранение настроек в энергонезависимую память
 * 
 * @return true если сохранение прошло успешно
 */
bool saveSystemSettings();

/**
 * @brief Сброс настроек к значениям по умолчанию
 */
void resetSystemSettings();

/**
 * @brief Вывод текущих настроек в последовательный порт
 */
void printSystemSettings();

#endif // SETTINGS_H