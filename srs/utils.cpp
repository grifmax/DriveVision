#include "utils.h"
#include "webserver.h"
#include "display.h"

// Воспроизведение звукового сигнала
void playSound(SoundType type) {
    // Проверяем, включен ли звук
    if (!sysSettings.soundEnabled) {
        return;
    }
    
    // Определяем параметры звука
    int frequency = 0;
    int duration = 0;
    
    switch (type) {
        case SOUND_START:
            // Звук запуска системы - восходящая мелодия
            for (int i = 0; i < 3; i++) {
                frequency = 1000 + i * 500;
                duration = 100;
                tone(PIN_BUZZER, frequency, duration);
                delay(duration + 50);
            }
            break;
            
        case SOUND_STOP:
            // Звук остановки - нисходящая мелодия
            for (int i = 0; i < 3; i++) {
                frequency = 2000 - i * 500;
                duration = 100;
                tone(PIN_BUZZER, frequency, duration);
                delay(duration + 50);
            }
            break;
            
        case SOUND_PHASE_CHANGE:
            // Звук смены фазы - два коротких сигнала
            frequency = 1500;
            duration = 100;
            tone(PIN_BUZZER, frequency, duration);
            delay(duration + 50);
            tone(PIN_BUZZER, frequency, duration);
            break;
            
        case SOUND_ALARM:
            // Звук тревоги - серия быстрых сигналов
            frequency = 2000;
            for (int i = 0; i < 5; i++) {
                tone(PIN_BUZZER, frequency, 100);
                delay(150);
            }
            break;
            
        case SOUND_PROCESS_COMPLETE:
            // Звук завершения процесса - мелодия
            int notes[] = {1000, 1500, 2000, 1500, 2000, 2500};
            int durations[] = {100, 100, 200, 100, 100, 300};
            
            for (int i = 0; i < 6; i++) {
                tone(PIN_BUZZER, notes[i], durations[i]);
                delay(durations[i] + 50);
            }
            break;
            
        case SOUND_BUTTON_PRESS:
            // Звук нажатия кнопки - короткий сигнал
            frequency = 1000;
            duration = 30;
            tone(PIN_BUZZER, frequency, duration);
            break;
            
        case SOUND_BUTTON_MENU:
            // Звук меню - чуть более длинный сигнал
            frequency = 1200;
            duration = 50;
            tone(PIN_BUZZER, frequency, duration);
            break;
            
        default:
            break;
    }
}

// Логирование события
void logEvent(const String& message) {
    // Вывод в последовательный порт
    Serial.print("[");
    Serial.print(getFormattedUptime());
    Serial.print("] ");
    Serial.println(message);
    
    // Здесь можно добавить запись в файл лога, если нужно
}

// Отправка уведомления в веб-интерфейс
void sendWebNotification(NotificationType type, const String& message) {
    // Отправляем уведомление через веб-сервер
    sendNotificationToClients(type, message);
    
    // Выводим уведомление на дисплей
    #ifdef DISPLAY_ENABLED
        showNotification(message.c_str(), type, 3000);
    #endif
    
    // Логируем событие
    logEvent(message);
    
    // Воспроизводим соответствующий звук в зависимости от типа уведомления
    switch (type) {
        case NOTIFY_ERROR:
            playSound(SOUND_ALARM);
            break;
        case NOTIFY_WARNING:
            playSound(SOUND_PHASE_CHANGE);
            break;
        case NOTIFY_SUCCESS:
            playSound(SOUND_PROCESS_COMPLETE);
            break;
        default:
            break;
    }
}

// Запуск процесса
void startProcess() {
    if (systemRunning) {
        sendWebNotification(NOTIFY_WARNING, "Процесс уже запущен");
        return;
    }
    
    // Проверяем, что все необходимые датчики подключены
    if (!checkRequiredSensors()) {
        sendWebNotification(NOTIFY_ERROR, "Не все датчики подключены");
        return;
    }
    
    systemRunning = true;
    systemPaused = false;
    processStartTime = millis();
    
    // Инициализация переменных в зависимости от режима
    if (currentMode == MODE_RECTIFICATION) {
        // Сбрасываем фазу ректификации
        rectPhase = PHASE_HEATING;
        
        // Устанавливаем мощность для нагрева
        if (sysSettings.powerControlMode == POWER_CONTROL_MANUAL) {
            setPowerPercent(rectParams.heatingPower);
        } else {
            setPowerWatts(rectParams.heatingPowerWatts);
        }
        
        // Сбрасываем счетчики отбора
        headsCollected = 0;
        bodyCollected = 0;
        tailsCollected = 0;
        
        // Инициализация переменных для альтернативной модели
        if (rectParams.model == MODEL_ALTERNATIVE) {
            temperatureStabilized = false;
        }
        
        sendWebNotification(NOTIFY_INFO, "Запущен процесс ректификации");
        logEvent("Начало процесса ректификации");
    } 
    else if (currentMode == MODE_DISTILLATION) {
        // Сбрасываем фазу дистилляции
        distPhase = DIST_PHASE_HEATING;
        
        // Устанавливаем мощность для нагрева
        if (sysSettings.powerControlMode == POWER_CONTROL_MANUAL) {
            setPowerPercent(distParams.heatingPower);
        } else {
            setPowerWatts(distParams.heatingPowerWatts);
        }
        
        // Сбрасываем счетчики отбора
        distillationCollected = 0;
        
        sendWebNotification(NOTIFY_INFO, "Запущен процесс дистилляции");
        logEvent("Начало процесса дистилляции");
    }
    
    playSound(SOUND_START);
    
    #ifdef DISPLAY_ENABLED
        goToScreen(SCREEN_PROCESS);
    #endif
    
    // Отправляем статус клиентам
    sendStatusToClients();
}

// Остановка процесса
void stopProcess() {
    if (!systemRunning) {
        return;
    }
    
    systemRunning = false;
    systemPaused = false;
    
    // Выключаем нагрев
    setPowerPercent(0);
    
    // Выключаем насос
    disablePump();
    
    // Выключаем клапан орошения
    disableValve();
    
    // Логируем и отправляем уведомление
    if (currentMode == MODE_RECTIFICATION) {
        sendWebNotification(NOTIFY_SUCCESS, "Процесс ректификации остановлен");
        logEvent("Конец процесса ректификации");
    } else if (currentMode == MODE_DISTILLATION) {
        sendWebNotification(NOTIFY_SUCCESS, "Процесс дистилляции остановлен");
        logEvent("Конец процесса дистилляции");
    }
    
    playSound(SOUND_STOP);
    
    #ifdef DISPLAY_ENABLED
        goToScreen(MENU_MAIN);
    #endif
    
    // Отправляем статус клиентам
    sendStatusToClients();
}

// Пауза процесса
void pauseProcess() {
    if (!systemRunning || systemPaused) {
        return;
    }
    
    systemPaused = true;
    
    // Запоминаем текущую мощность и устанавливаем минимальную
    pausedPower = getCurrentPowerPercent();
    setPowerPercent(10);  // Минимальная поддерживающая мощность
    
    // Останавливаем насос
    disablePump();
    
    // Закрываем клапан орошения
    disableValve();
    
    sendWebNotification(NOTIFY_WARNING, "Процесс приостановлен");
    logEvent("Процесс приостановлен");
    
    // Отправляем статус клиентам
    sendStatusToClients();
}

// Возобновление процесса
void resumeProcess() {
    if (!systemRunning || !systemPaused) {
        return;
    }
    
    systemPaused = false;
    
    // Восстанавливаем мощность
    setPowerPercent(pausedPower);
    
    sendWebNotification(NOTIFY_INFO, "Процесс возобновлен");
    logEvent("Процесс возобновлен");
    
    // Отправляем статус клиентам
    sendStatusToClients();
}

// Преобразование процентов мощности в ватты
int percentToWatts(int percent) {
    return (int)((float)percent * sysSettings.maxHeaterPowerWatts / 100.0f);
}

// Преобразование ватт в проценты мощности
int wattsToPercent(int watts) {
    return (int)((float)watts * 100.0f / sysSettings.maxHeaterPowerWatts);
}

// Получение имени фазы ректификации на русском
String getPhaseNameRussian(RectificationPhase phase) {
    switch (phase) {
        case PHASE_NONE:
            return "Не начат";
        case PHASE_HEATING:
            return "Нагрев";
        case PHASE_STABILIZATION:
            return "Стабилизация";
        case PHASE_HEADS:
            return "Отбор голов";
        case PHASE_POST_HEADS_STABILIZATION:
            return "Стабилизация после голов";
        case PHASE_BODY:
            return "Отбор тела";
        case PHASE_TAILS:
            return "Отбор хвостов";
        case PHASE_COMPLETED:
            return "Завершен";
        default:
            return "Неизвестно";
    }
}

// Получение имени фазы дистилляции на русском
String getDistPhaseNameRussian(DistillationPhase phase) {
    switch (phase) {
        case DIST_PHASE_NONE:
            return "Не начат";
        case DIST_PHASE_HEATING:
            return "Нагрев";
        case DIST_PHASE_DISTILLATION:
            return "Отбор";
        case DIST_PHASE_COMPLETED:
            return "Завершен";
        default:
            return "Неизвестно";
    }
}

// Получение строки с форматированным временем (ч:м:с)
String getFormattedTime(unsigned long timeInMs) {
    unsigned long seconds = timeInMs / 1000;
    unsigned long minutes = seconds / 60;
    unsigned long hours = minutes / 60;
    
    minutes %= 60;
    seconds %= 60;
    
    char buffer[9]; // формат "00:00:00"
    sprintf(buffer, "%02lu:%02lu:%02lu", hours, minutes, seconds);
    return String(buffer);
}

// Получение строки с форматированным временем работы
String getFormattedUptime() {
    return getFormattedTime(millis());
}

// Проверка подключения необходимых датчиков
bool checkRequiredSensors() {
    if (currentMode == MODE_RECTIFICATION) {
        // Для ректификации нужны датчики куба и колонны
        if (!tempSensors.isConnected(TEMP_CUBE) || !tempSensors.isConnected(TEMP_REFLUX)) {
            return false;
        }
    } else if (currentMode == MODE_DISTILLATION) {
        // Для дистилляции нужен хотя бы датчик куба
        if (!tempSensors.isConnected(TEMP_CUBE)) {
            return false;
        }
    }
    
    return true;
}