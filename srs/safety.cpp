/**
 * @file safety.cpp
 * @brief Реализация функций системы безопасности
 */

#include "safety.h"
#include "temp_sensors.h"
#include "heater.h"
#include "pump.h"
#include "valve.h"
#include "config.h"
#include "display.h"
#include "utils.h"
#include <Arduino.h>

#ifdef ESP32
#include <esp_task_wdt.h>
#endif

// Настройки безопасности по умолчанию
#define DEFAULT_MAX_RUNTIME_HOURS 12        // Максимальное время непрерывной работы в часах
#define DEFAULT_MAX_CUBE_TEMP 105.0f        // Максимальная температура куба в градусах
#define DEFAULT_MAX_TEMP_RISE_RATE 5.0f     // Максимальная скорость роста температуры (градусов в минуту)
#define DEFAULT_MIN_WATER_OUT_TEMP 5.0f     // Минимальная температура выхода воды
#define DEFAULT_MAX_WATER_OUT_TEMP 50.0f    // Максимальная температура выхода воды
#define WATCHDOG_TIMEOUT_SECONDS 30         // Таймаут сторожевого таймера в секундах
#define SAFETY_CHECK_INTERVAL 1000          // Интервал проверки безопасности (мс)

// Параметры безопасности
static float maxCubeTemp = DEFAULT_MAX_CUBE_TEMP;
static float maxTempRiseRate = DEFAULT_MAX_TEMP_RISE_RATE;
static float minWaterOutTemp = DEFAULT_MIN_WATER_OUT_TEMP;
static float maxWaterOutTemp = DEFAULT_MAX_WATER_OUT_TEMP;
static int maxRuntimeHours = DEFAULT_MAX_RUNTIME_HOURS;

// Время последней проверки безопасности
static unsigned long lastSafetyCheck = 0;

// Время начала процесса
static unsigned long processStartTime = 0;
static bool processRunning = false;

// Состояние сторожевого таймера
static bool watchdogEnabled = false;

// Текущее состояние безопасности
static SafetyStatus currentStatus = {
    true,                      // isSystemSafe
    SAFETY_OK,                 // errorCode
    0,                         // errorTime
    "",                        // errorDescription
    false,                     // isSensorError
    false,                     // isTemperatureError
    false,                     // isWaterFlowError
    false,                     // isPressureError
    false,                     // isRuntimeError
    false,                     // isEmergencyStop
    false                      // isWatchdogReset
};

// История предыдущих показаний температуры для определения скорости изменения
#define TEMP_HISTORY_SIZE 10
static float tempHistory[MAX_TEMP_SENSORS][TEMP_HISTORY_SIZE];
static unsigned long tempHistoryTime[TEMP_HISTORY_SIZE];
static int tempHistoryIndex = 0;

// Инициализация системы безопасности
bool initSafety() {
    // Сброс истории температур
    for (int i = 0; i < MAX_TEMP_SENSORS; i++) {
        for (int j = 0; j < TEMP_HISTORY_SIZE; j++) {
            tempHistory[i][j] = 0.0f;
        }
    }
    
    // Сброс времени истории
    for (int i = 0; i < TEMP_HISTORY_SIZE; i++) {
        tempHistoryTime[i] = 0;
    }
    
    tempHistoryIndex = 0;
    
    // Сброс состояния безопасности
    currentStatus.isSystemSafe = true;
    currentStatus.errorCode = SAFETY_OK;
    currentStatus.errorTime = 0;
    currentStatus.errorDescription = "";
    currentStatus.isSensorError = false;
    currentStatus.isTemperatureError = false;
    currentStatus.isWaterFlowError = false;
    currentStatus.isPressureError = false;
    currentStatus.isRuntimeError = false;
    currentStatus.isEmergencyStop = false;
    currentStatus.isWatchdogReset = false;
    
    // Проверяем, был ли перезапуск по сторожевому таймеру
    #ifdef ESP32
    if (esp_reset_reason() == ESP_RST_TASK_WDT) {
        currentStatus.isWatchdogReset = true;
        Serial.println("Внимание: система перезагружена по сторожевому таймеру!");
    }
    #endif
    
    // Запуск сторожевого таймера
    startSafetyWatchdog(WATCHDOG_TIMEOUT_SECONDS);
    
    Serial.println("Система безопасности инициализирована");
    return true;
}

// Проверка общего состояния безопасности
bool isSafetyOK() {
    // Обновляем состояние безопасности
    updateSafety();
    
    return currentStatus.isSystemSafe;
}

// Получение статуса безопасности
SafetyStatus getSafetyStatus() {
    return currentStatus;
}

// Сброс ошибок безопасности
bool resetSafetyErrors() {
    // Сбрасываем ошибки только если это не аварийная остановка и не ошибка датчика
    if (currentStatus.isEmergencyStop || currentStatus.isSensorError) {
        // Для этих ошибок требуется вмешательство пользователя
        return false;
    }
    
    currentStatus.isSystemSafe = true;
    currentStatus.errorCode = SAFETY_OK;
    currentStatus.errorDescription = "";
    currentStatus.isTemperatureError = false;
    currentStatus.isWaterFlowError = false;
    currentStatus.isPressureError = false;
    currentStatus.isRuntimeError = false;
    
    Serial.println("Ошибки безопасности сброшены");
    return true;
}

// Проверка безопасности для процесса дистилляции
SafetyErrorCode checkDistillationSafety(float cubeTemp, float waterOutTemp) {
    // Проверка подключения необходимых датчиков
    if (!isSensorConnected(TEMP_CUBE)) {
        return SAFETY_ERROR_SENSOR_DISCONNECT;
    }
    
    // Проверка температуры куба
    if (cubeTemp > maxCubeTemp) {
        return SAFETY_ERROR_TEMPERATURE_HIGH;
    }
    
    // Проверка температуры охлаждающей воды
    if (isSensorConnected(TEMP_WATER_OUT)) {
        if (waterOutTemp > maxWaterOutTemp) {
            return SAFETY_ERROR_WATER_FLOW_LOW;
        }
    }
    
    // Проверка скорости изменения температуры
    float tempRiseRate = calculateTempRiseRate(TEMP_CUBE);
    if (tempRiseRate > maxTempRiseRate) {
        return SAFETY_ERROR_TEMPERATURE_RISE;
    }
    
    // Проверка максимального времени работы
    if (processRunning && (millis() - processStartTime) > (maxRuntimeHours * 3600000UL)) {
        return SAFETY_ERROR_MAX_RUNTIME_EXCEEDED;
    }
    
    return SAFETY_OK;
}

// Проверка безопасности для процесса ректификации
SafetyErrorCode checkRectificationSafety(float cubeTemp, float columnTemp, float refluxTemp, float waterOutTemp, float tsaTemp) {
    // Проверка подключения необходимых датчиков
    if (!isSensorConnected(TEMP_CUBE) || !isSensorConnected(TEMP_REFLUX)) {
        return SAFETY_ERROR_SENSOR_DISCONNECT;
    }
    
    // Проверка температуры куба
    if (cubeTemp > maxCubeTemp) {
        return SAFETY_ERROR_TEMPERATURE_HIGH;
    }
    
    // Проверка температуры охлаждающей воды
    if (isSensorConnected(TEMP_WATER_OUT)) {
        if (waterOutTemp > maxWaterOutTemp) {
            return SAFETY_ERROR_WATER_FLOW_LOW;
        }
    }
    
    // Проверка скорости изменения температуры
    float tempRiseRate = calculateTempRiseRate(TEMP_CUBE);
    if (tempRiseRate > maxTempRiseRate) {
        return SAFETY_ERROR_TEMPERATURE_RISE;
    }
    
    // Проверка максимального времени работы
    if (processRunning && (millis() - processStartTime) > (maxRuntimeHours * 3600000UL)) {
        return SAFETY_ERROR_MAX_RUNTIME_EXCEEDED;
    }
    
    return SAFETY_OK;
}

// Обновление системы безопасности
void updateSafety() {
    unsigned long currentTime = millis();
    
    // Проверяем безопасность только через определенные интервалы времени
    if (currentTime - lastSafetyCheck < SAFETY_CHECK_INTERVAL) {
        return;
    }
    
    lastSafetyCheck = currentTime;
    
    // Сброс сторожевого таймера
    resetSafetyWatchdog();
    
    // Обновление истории температур
    updateTemperatureHistory();
    
    // Проверка безопасности в зависимости от текущего процесса
    if (processRunning) {
        // Получаем все текущие температуры
        float cubeTemp = getTemperature(TEMP_CUBE);
        float columnTemp = getTemperature(TEMP_COLUMN);
        float refluxTemp = getTemperature(TEMP_REFLUX);
        float waterOutTemp = getTemperature(TEMP_WATER_OUT);
        float tsaTemp = getTemperature(TEMP_TSA);
        
        SafetyErrorCode errorCode;
        
        // Проверяем безопасность для текущего процесса
        // Здесь будет код, который определяет, какой процесс запущен и вызывает соответствующую проверку
        // Для примера используем проверку для ректификации
        errorCode = checkRectificationSafety(cubeTemp, columnTemp, refluxTemp, waterOutTemp, tsaTemp);
        
        // Если обнаружена ошибка безопасности
        if (errorCode != SAFETY_OK) {
            // Обновляем статус безопасности
            currentStatus.isSystemSafe = false;
            currentStatus.errorCode = errorCode;
            currentStatus.errorTime = currentTime;
            currentStatus.errorDescription = getSafetyErrorDescription(errorCode);
            
            // Устанавливаем соответствующий флаг ошибки
            switch (errorCode) {
                case SAFETY_ERROR_SENSOR_DISCONNECT:
                    currentStatus.isSensorError = true;
                    break;
                case SAFETY_ERROR_TEMPERATURE_HIGH:
                case SAFETY_ERROR_TEMPERATURE_RISE:
                    currentStatus.isTemperatureError = true;
                    break;
                case SAFETY_ERROR_WATER_FLOW_LOW:
                    currentStatus.isWaterFlowError = true;
                    break;
                case SAFETY_ERROR_MAX_RUNTIME_EXCEEDED:
                    currentStatus.isRuntimeError = true;
                    break;
                case SAFETY_ERROR_PRESSURE_HIGH:
                    currentStatus.isPressureError = true;
                    break;
                default:
                    break;
            }
            
            // Выполняем аварийную остановку при серьезных ошибках
            if (errorCode == SAFETY_ERROR_TEMPERATURE_HIGH || 
                errorCode == SAFETY_ERROR_PRESSURE_HIGH || 
                errorCode == SAFETY_ERROR_WATER_FLOW_LOW) {
                emergencyStop(currentStatus.errorDescription);
            }
        }
    }
}

// Аварийная остановка
void emergencyStop(const String &reason) {
    // Установка флагов аварийной остановки
    currentStatus.isSystemSafe = false;
    currentStatus.isEmergencyStop = true;
    currentStatus.errorDescription = "АВАРИЙНАЯ ОСТАНОВКА: " + reason;
    
    // Остановка процесса
    processRunning = false;
    
    // Выключение нагревателя
    setHeaterPower(0);
    
    // Остановка насоса
    pumpStop();
    
    // Закрытие клапана
    valveClose();
    
    // Вывод сообщения
    Serial.println(currentStatus.errorDescription);
    
    // Обновление дисплея с сообщением об ошибке
    displayShowError(currentStatus.errorDescription);
}

// Получение описания ошибки безопасности
String getSafetyErrorDescription(SafetyErrorCode errorCode) {
    switch (errorCode) {
        case SAFETY_OK:
            return "Система в норме";
        case SAFETY_ERROR_TEMPERATURE_HIGH:
            return "Превышение максимальной температуры";
        case SAFETY_ERROR_TEMPERATURE_RISE:
            return "Слишком быстрый рост температуры";
        case SAFETY_ERROR_SENSOR_DISCONNECT:
            return "Отключение датчика температуры";
        case SAFETY_ERROR_WATER_FLOW_LOW:
            return "Низкий поток охлаждающей воды";
        case SAFETY_ERROR_MAX_RUNTIME_EXCEEDED:
            return "Превышено максимальное время работы";
        case SAFETY_ERROR_POWER_ISSUE:
            return "Проблемы с питанием";
        case SAFETY_ERROR_EMERGENCY_STOP:
            return "Аварийная остановка";
        case SAFETY_ERROR_WATCHDOG_TIMEOUT:
            return "Срабатывание сторожевого таймера";
        case SAFETY_ERROR_PRESSURE_HIGH:
            return "Высокое давление";
        default:
            return "Неизвестная ошибка";
    }
}

// Установка максимального времени непрерывной работы
void setSafetyMaxRuntime(int hours) {
    if (hours > 0) {
        maxRuntimeHours = hours;
    }
}

// Установка максимальной температуры куба
void setSafetyMaxCubeTemp(float maxTemp) {
    if (maxTemp > 0) {
        maxCubeTemp = maxTemp;
    }
}

// Установка максимальной скорости изменения температуры
void setSafetyMaxTempRiseRate(float maxRatePerMinute) {
    if (maxRatePerMinute > 0) {
        maxTempRiseRate = maxRatePerMinute;
    }
}

// Установка минимальной температуры выхода воды для охлаждения
void setSafetyMinWaterOutTemp(float minTemp) {
    minWaterOutTemp = minTemp;
}

// Установка максимальной температуры выхода воды для охлаждения
void setSafetyMaxWaterOutTemp(float maxTemp) {
    if (maxTemp > 0) {
        maxWaterOutTemp = maxTemp;
    }
}

// Запуск сторожевого таймера
void startSafetyWatchdog(int timeoutSeconds) {
    if (timeoutSeconds <= 0) {
        return;
    }
    
    #ifdef ESP32
    // Настройка сторожевого таймера для ESP32
    esp_task_wdt_init(timeoutSeconds, true);  // Включаем сторожевой таймер с перезагрузкой
    esp_task_wdt_add(NULL);  // Добавляем текущую задачу в мониторинг
    watchdogEnabled = true;
    Serial.println("Сторожевой таймер запущен с таймаутом " + String(timeoutSeconds) + " секунд");
    #endif
}

// Сброс сторожевого таймера
void resetSafetyWatchdog() {
    if (!watchdogEnabled) {
        return;
    }
    
    #ifdef ESP32
    esp_task_wdt_reset();
    #endif
}

// Обновление истории температур для отслеживания скорости изменения
void updateTemperatureHistory() {
    // Получаем текущее время
    unsigned long currentTime = millis();
    
    // Обновляем индекс истории
    tempHistoryIndex = (tempHistoryIndex + 1) % TEMP_HISTORY_SIZE;
    
    // Записываем текущее время
    tempHistoryTime[tempHistoryIndex] = currentTime;
    
    // Сохраняем текущие показания датчиков
    for (int i = 0; i < MAX_TEMP_SENSORS; i++) {
        if (isSensorConnected(i)) {
            tempHistory[i][tempHistoryIndex] = getTemperature(i);
        } else {
            tempHistory[i][tempHistoryIndex] = -127.0;  // Значение, означающее отсутствие данных
        }
    }
}

// Расчет скорости изменения температуры в градусах в минуту
float calculateTempRiseRate(int sensorIndex) {
    if (!isSensorConnected(sensorIndex)) {
        return 0.0f;
    }
    
    // Получаем текущий индекс истории
    int currentIdx = tempHistoryIndex;
    
    // Находим самую старую запись с валидными данными
    int oldestIdx = currentIdx;
    unsigned long oldestTime = tempHistoryTime[oldestIdx];
    
    for (int i = 0; i < TEMP_HISTORY_SIZE; i++) {
        int idx = (currentIdx - i + TEMP_HISTORY_SIZE) % TEMP_HISTORY_SIZE;
        
        // Проверяем, что данные валидны и не слишком старые (не более 5 минут)
        if (tempHistoryTime[idx] > 0 && 
            tempHistory[sensorIndex][idx] > -100.0 &&
            (tempHistoryTime[currentIdx] - tempHistoryTime[idx]) <= 300000UL) {
            
            if (tempHistoryTime[idx] < oldestTime || oldestTime == tempHistoryTime[oldestIdx]) {
                oldestIdx = idx;
                oldestTime = tempHistoryTime[idx];
            }
        }
    }
    
    // Если нет исторических данных или они слишком свежие
    if (oldestIdx == currentIdx || 
        (tempHistoryTime[currentIdx] - tempHistoryTime[oldestIdx]) < 30000UL) {
        return 0.0f;  // Недостаточно данных для расчета
    }
    
    // Рассчитываем скорость изменения
    float tempDelta = tempHistory[sensorIndex][currentIdx] - tempHistory[sensorIndex][oldestIdx];
    float timeDeltaMinutes = (float)(tempHistoryTime[currentIdx] - tempHistoryTime[oldestIdx]) / 60000.0f;
    
    return tempDelta / timeDeltaMinutes;
}

// Регистрация начала процесса для контроля времени работы
void registerProcessStart() {
    processStartTime = millis();
    processRunning = true;
    
    // Сбрасываем флаги ошибок
    resetSafetyErrors();
    
    Serial.println("Зарегистрирован запуск процесса. Таймер безопасности активирован.");
}

// Регистрация окончания процесса
void registerProcessEnd() {
    processRunning = false;
    Serial.println("Зарегистрировано завершение процесса. Таймер безопасности деактивирован.");
}