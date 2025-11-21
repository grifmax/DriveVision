/**
 * @file settings.cpp
 * @brief Реализация управления настройками системы
 */

#include "settings.h"
#include "config.h"
#include <EEPROM.h>
#include <Arduino.h>

// Адрес начала хранения настроек в EEPROM
#define SETTINGS_EEPROM_ADDRESS 0

// Текущая версия структуры настроек
#define SETTINGS_VERSION 1

// Размер EEPROM для хранения настроек
#define EEPROM_SIZE 2048

// Глобальный экземпляр настроек
SystemSettings sysSettings;

// Инициализация системы настроек
bool initSettings() {
    Serial.println("Инициализация системы настроек...");
    
    // Инициализация EEPROM
    if (!EEPROM.begin(EEPROM_SIZE)) {
        Serial.println("Ошибка инициализации EEPROM!");
        return false;
    }
    
    // Загружаем сохраненные настройки
    if (!loadSystemSettings()) {
        Serial.println("Настройки не загружены, устанавливаем значения по умолчанию.");
        resetSystemSettings();
        saveSystemSettings();
    }
    
    Serial.println("Система настроек инициализирована");
    return true;
}

// Загрузка настроек из энергонезависимой памяти
bool loadSystemSettings() {
    // Считываем структуру настроек из EEPROM
    EEPROM.get(SETTINGS_EEPROM_ADDRESS, sysSettings);
    
    // Проверяем версию настроек
    if (sysSettings.settingsVersion != SETTINGS_VERSION) {
        Serial.println("Версия настроек не соответствует текущей версии!");
        return false;
    }
    
    Serial.println("Настройки успешно загружены");
    printSystemSettings();
    return true;
}

// Сохранение настроек в энергонезависимую память
bool saveSystemSettings() {
    // Устанавливаем текущую версию настроек
    sysSettings.settingsVersion = SETTINGS_VERSION;
    
    // Сохраняем структуру настроек в EEPROM
    EEPROM.put(SETTINGS_EEPROM_ADDRESS, sysSettings);
    
    // Фиксируем изменения
    if (!EEPROM.commit()) {
        Serial.println("Ошибка сохранения настроек в EEPROM!");
        return false;
    }
    
    Serial.println("Настройки успешно сохранены");
    return true;
}

// Сброс настроек к значениям по умолчанию
void resetSystemSettings() {
    Serial.println("Сброс настроек к значениям по умолчанию...");
    
    // Обнуляем всю структуру
    memset(&sysSettings, 0, sizeof(SystemSettings));
    
    // Устанавливаем версию настроек
    sysSettings.settingsVersion = SETTINGS_VERSION;
    
    // Сброс настроек датчиков температуры
    for (int i = 0; i < MAX_TEMP_SENSORS; i++) {
        sysSettings.tempSensorEnabled[i] = false;
        sysSettings.tempSensorCalibration[i] = 0.0f;
        memset(sysSettings.tempSensorAddresses[i], 0, 8);
    }
    
    // Настройки нагревателя по умолчанию
    sysSettings.heaterSettings.maxPowerWatts = 2000;
    sysSettings.heaterSettings.volts = 220;
    
    // Настройки насоса по умолчанию
    sysSettings.pumpSettings.headsFlowRate = 50.0f;
    sysSettings.pumpSettings.bodyFlowRate = 250.0f;
    sysSettings.pumpSettings.tailsFlowRate = 350.0f;
    sysSettings.pumpSettings.calibrationFactor = 1.0f;
    
    // Настройки ректификации по умолчанию
    sysSettings.rectificationSettings.model = 0;
    sysSettings.rectificationSettings.heatingPowerWatts = 1800;
    sysSettings.rectificationSettings.stabilizationPowerWatts = 1200;
    sysSettings.rectificationSettings.bodyPowerWatts = 1000;
    sysSettings.rectificationSettings.tailsPowerWatts = 1200;
    sysSettings.rectificationSettings.headsTemp = 78.0f;
    sysSettings.rectificationSettings.bodyTemp = 78.3f;
    sysSettings.rectificationSettings.tailsTemp = 92.0f;
    sysSettings.rectificationSettings.endTemp = 97.0f;
    sysSettings.rectificationSettings.maxCubeTemp = 101.0f;
    sysSettings.rectificationSettings.tailsCubeTemp = 95.0f;
    sysSettings.rectificationSettings.tempDeltaEndBody = 0.5f;
    sysSettings.rectificationSettings.stabilizationTime = 30;
    sysSettings.rectificationSettings.postHeadsStabilizationTime = 10;
    sysSettings.rectificationSettings.headsVolume = 150;
    sysSettings.rectificationSettings.bodyVolume = 2000;
    sysSettings.rectificationSettings.refluxRatio = 3.0f;
    sysSettings.rectificationSettings.refluxPeriod = 60;
    sysSettings.rectificationSettings.useSameFlowForTails = true;
    
    // Настройки дистилляции по умолчанию
    sysSettings.distillationSettings.heatingPowerWatts = 2000;
    sysSettings.distillationSettings.distillationPowerWatts = 1500;
    sysSettings.distillationSettings.startCollectingTemp = 70.0f;
    sysSettings.distillationSettings.endTemp = 97.0f;
    sysSettings.distillationSettings.maxCubeTemp = 101.0f;
    sysSettings.distillationSettings.separateHeads = true;
    sysSettings.distillationSettings.headsVolume = 200;
    sysSettings.distillationSettings.flowRate = 800.0f;
    sysSettings.distillationSettings.headsFlowRate = 200.0f;
    
    // Настройки безопасности
    sysSettings.safetySettings.maxRuntimeHours = SAFETY_MAX_RUNTIME_HOURS_DEFAULT;
    sysSettings.safetySettings.maxCubeTemp = SAFETY_MAX_CUBE_TEMP_DEFAULT;
    sysSettings.safetySettings.maxTempRiseRate = SAFETY_MAX_TEMP_RISE_RATE_DEFAULT;
    sysSettings.safetySettings.minWaterOutTemp = SAFETY_MIN_WATER_OUT_TEMP_DEFAULT;
    sysSettings.safetySettings.maxWaterOutTemp = SAFETY_MAX_WATER_OUT_TEMP_DEFAULT;
    sysSettings.safetySettings.emergencyStopEnabled = true;
    sysSettings.safetySettings.watchdogEnabled = true;
    
    // Настройки WiFi
    strncpy(sysSettings.wifiSsid, WIFI_AP_SSID, sizeof(sysSettings.wifiSsid) - 1);
    strncpy(sysSettings.wifiPassword, WIFI_AP_PASSWORD, sizeof(sysSettings.wifiPassword) - 1);
    sysSettings.useAccessPoint = true;
    
    Serial.println("Настройки сброшены к значениям по умолчанию");
}

// Вывод текущих настроек в последовательный порт
void printSystemSettings() {
    Serial.println("Текущие настройки системы:");
    Serial.println("----------------------------");
    
    Serial.println("Настройки нагревателя:");
    Serial.print("  Максимальная мощность: ");
    Serial.print(sysSettings.heaterSettings.maxPowerWatts);
    Serial.println(" Вт");
    
    Serial.println("Настройки насоса:");
    Serial.print("  Скорость отбора голов: ");
    Serial.print(sysSettings.pumpSettings.headsFlowRate);
    Serial.println(" мл/мин");
    Serial.print("  Скорость отбора тела: ");
    Serial.print(sysSettings.pumpSettings.bodyFlowRate);
    Serial.println(" мл/мин");
    
    Serial.println("Настройки ректификации:");
    Serial.print("  Модель: ");
    Serial.println(sysSettings.rectificationSettings.model);
    Serial.print("  Температура голов: ");
    Serial.print(sysSettings.rectificationSettings.headsTemp);
    Serial.println(" °C");
    
    Serial.println("Настройки дистилляции:");
    Serial.print("  Температура начала отбора: ");
    Serial.print(sysSettings.distillationSettings.startCollectingTemp);
    Serial.println(" °C");
    
    Serial.println("Настройки безопасности:");
    Serial.print("  Макс. время работы: ");
    Serial.print(sysSettings.safetySettings.maxRuntimeHours);
    Serial.println(" ч");
    Serial.print("  Макс. температура куба: ");
    Serial.print(sysSettings.safetySettings.maxCubeTemp);
    Serial.println(" °C");
    
    Serial.println("----------------------------");
}/**
 * @file settings.cpp
 * @brief Реализация управления настройками системы
 */

#include "settings.h"
#include "config.h"
#include <EEPROM.h>
#include <Arduino.h>

// Адрес начала хранения настроек в EEPROM
#define SETTINGS_EEPROM_ADDRESS 0

// Текущая версия структуры настроек
#define SETTINGS_VERSION 1

// Размер EEPROM для хранения настроек
#define EEPROM_SIZE 2048

// Глобальный экземпляр настроек
SystemSettings sysSettings;

// Инициализация системы настроек
bool initSettings() {
    Serial.println("Инициализация системы настроек...");
    
    // Инициализация EEPROM
    if (!EEPROM.begin(EEPROM_SIZE)) {
        Serial.println("Ошибка инициализации EEPROM!");
        return false;
    }
    
    // Загружаем сохраненные настройки
    if (!loadSystemSettings()) {
        Serial.println("Настройки не загружены, устанавливаем значения по умолчанию.");
        resetSystemSettings();
        saveSystemSettings();
    }
    
    Serial.println("Система настроек инициализирована");
    return true;
}

// Загрузка настроек из энергонезависимой памяти
bool loadSystemSettings() {
    // Считываем структуру настроек из EEPROM
    EEPROM.get(SETTINGS_EEPROM_ADDRESS, sysSettings);
    
    // Проверяем версию настроек
    if (sysSettings.settingsVersion != SETTINGS_VERSION) {
        Serial.println("Версия настроек не соответствует текущей версии!");
        return false;
    }
    
    Serial.println("Настройки успешно загружены");
    printSystemSettings();
    return true;
}

// Сохранение настроек в энергонезависимую память
bool saveSystemSettings() {
    // Устанавливаем текущую версию настроек
    sysSettings.settingsVersion = SETTINGS_VERSION;
    
    // Сохраняем структуру настроек в EEPROM
    EEPROM.put(SETTINGS_EEPROM_ADDRESS, sysSettings);
    
    // Фиксируем изменения
    if (!EEPROM.commit()) {
        Serial.println("Ошибка сохранения настроек в EEPROM!");
        return false;
    }
    
    Serial.println("Настройки успешно сохранены");
    return true;
}

// Сброс настроек к значениям по умолчанию
void resetSystemSettings() {
    Serial.println("Сброс настроек к значениям по умолчанию...");
    
    // Обнуляем всю структуру
    memset(&sysSettings, 0, sizeof(SystemSettings));
    
    // Устанавливаем версию настроек
    sysSettings.settingsVersion = SETTINGS_VERSION;
    
    // Сброс настроек датчиков температуры
    for (int i = 0; i < MAX_TEMP_SENSORS; i++) {
        sysSettings.tempSensorEnabled[i] = false;
        sysSettings.tempSensorCalibration[i] = 0.0f;
        memset(sysSettings.tempSensorAddresses[i], 0, 8);
    }
    
    // Настройки нагревателя по умолчанию
    sysSettings.heaterSettings.maxPowerWatts = 2000;
    sysSettings.heaterSettings.volts = 220;
    
    // Настройки насоса по умолчанию
    sysSettings.pumpSettings.headsFlowRate = 50.0f;
    sysSettings.pumpSettings.bodyFlowRate = 250.0f;
    sysSettings.pumpSettings.tailsFlowRate = 350.0f;
    sysSettings.pumpSettings.calibrationFactor = 1.0f;
    
    // Настройки ректификации по умолчанию
    sysSettings.rectificationSettings.model = 0;
    sysSettings.rectificationSettings.heatingPowerWatts = 1800;
    sysSettings.rectificationSettings.stabilizationPowerWatts = 1200;
    sysSettings.rectificationSettings.bodyPowerWatts = 1000;
    sysSettings.rectificationSettings.tailsPowerWatts = 1200;
    sysSettings.rectificationSettings.headsTemp = 78.0f;
    sysSettings.rectificationSettings.bodyTemp = 78.3f;
    sysSettings.rectificationSettings.tailsTemp = 92.0f;
    sysSettings.rectificationSettings.endTemp = 97.0f;
    sysSettings.rectificationSettings.maxCubeTemp = 101.0f;
    sysSettings.rectificationSettings.tailsCubeTemp = 95.0f;
    sysSettings.rectificationSettings.tempDeltaEndBody = 0.5f;
    sysSettings.rectificationSettings.stabilizationTime = 30;
    sysSettings.rectificationSettings.postHeadsStabilizationTime = 10;
    sysSettings.rectificationSettings.headsVolume = 150;
    sysSettings.rectificationSettings.bodyVolume = 2000;
    sysSettings.rectificationSettings.refluxRatio = 3.0f;
    sysSettings.rectificationSettings.refluxPeriod = 60;
    sysSettings.rectificationSettings.useSameFlowForTails = true;
    
    // Настройки дистилляции по умолчанию
    sysSettings.distillationSettings.heatingPowerWatts = 2000;
    sysSettings.distillationSettings.distillationPowerWatts = 1500;
    sysSettings.distillationSettings.startCollectingTemp = 70.0f;
    sysSettings.distillationSettings.endTemp = 97.0f;
    sysSettings.distillationSettings.maxCubeTemp = 101.0f;
    sysSettings.distillationSettings.separateHeads = true;
    sysSettings.distillationSettings.headsVolume = 200;
    sysSettings.distillationSettings.flowRate = 800.0f;
    sysSettings.distillationSettings.headsFlowRate = 200.0f;
    
    // Настройки безопасности
    sysSettings.safetySettings.maxRuntimeHours = SAFETY_MAX_RUNTIME_HOURS_DEFAULT;
    sysSettings.safetySettings.maxCubeTemp = SAFETY_MAX_CUBE_TEMP_DEFAULT;
    sysSettings.safetySettings.maxTempRiseRate = SAFETY_MAX_TEMP_RISE_RATE_DEFAULT;
    sysSettings.safetySettings.minWaterOutTemp = SAFETY_MIN_WATER_OUT_TEMP_DEFAULT;
    sysSettings.safetySettings.maxWaterOutTemp = SAFETY_MAX_WATER_OUT_TEMP_DEFAULT;
    sysSettings.safetySettings.emergencyStopEnabled = true;
    sysSettings.safetySettings.watchdogEnabled = true;
    
    // Настройки WiFi
    strncpy(sysSettings.wifiSsid, WIFI_AP_SSID, sizeof(sysSettings.wifiSsid) - 1);
    strncpy(sysSettings.wifiPassword, WIFI_AP_PASSWORD, sizeof(sysSettings.wifiPassword) - 1);
    sysSettings.useAccessPoint = true;
    
    Serial.println("Настройки сброшены к значениям по умолчанию");
}

// Вывод текущих настроек в последовательный порт
void printSystemSettings() {
    Serial.println("Текущие настройки системы:");
    Serial.println("----------------------------");
    
    Serial.println("Настройки нагревателя:");
    Serial.print("  Максимальная мощность: ");
    Serial.print(sysSettings.heaterSettings.maxPowerWatts);
    Serial.println(" Вт");
    
    Serial.println("Настройки насоса:");
    Serial.print("  Скорость отбора голов: ");
    Serial.print(sysSettings.pumpSettings.headsFlowRate);
    Serial.println(" мл/мин");
    Serial.print("  Скорость отбора тела: ");
    Serial.print(sysSettings.pumpSettings.bodyFlowRate);
    Serial.println(" мл/мин");
    
    Serial.println("Настройки ректификации:");
    Serial.print("  Модель: ");
    Serial.println(sysSettings.rectificationSettings.model);
    Serial.print("  Температура голов: ");
    Serial.print(sysSettings.rectificationSettings.headsTemp);
    Serial.println(" °C");
    
    Serial.println("Настройки дистилляции:");
    Serial.print("  Температура начала отбора: ");
    Serial.print(sysSettings.distillationSettings.startCollectingTemp);
    Serial.println(" °C");
    
    Serial.println("Настройки безопасности:");
    Serial.print("  Макс. время работы: ");
    Serial.print(sysSettings.safetySettings.maxRuntimeHours);
    Serial.println(" ч");
    Serial.print("  Макс. температура куба: ");
    Serial.print(sysSettings.safetySettings.maxCubeTemp);
    Serial.println(" °C");
    
    Serial.println("----------------------------");
}