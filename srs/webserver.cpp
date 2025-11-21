#include "webserver.h"
#include <WiFi.h>
#include <SPIFFS.h>
#include <ArduinoJson.h>
#include <ESPAsyncWebServer.h>
#include <AsyncJson.h>
#include "utils.h"
#include "temp_sensors.h"
#include "power_control.h"

// Создаем экземпляр веб-сервера на порту 80
AsyncWebServer server(80);

// WebSocket для отправки данных в режиме реального времени
AsyncWebSocket ws("/ws");

// Время последней отправки данных через WebSocket
static unsigned long lastWebSocketUpdate = 0;

// Инициализация веб-сервера
void initWebServer() {
    Serial.println("Инициализация веб-сервера...");
    
    // Настраиваем WiFi
    if (!setupWiFi()) {
        // Если не удалось подключиться к WiFi, создаем точку доступа
        setupAccessPoint();
    }
    
    // Настраиваем маршруты
    setupWebRoutes();
    
    // Настраиваем WebSocket
    setupWebSocket();
    
    // Запускаем сервер
    server.begin();
    
    Serial.println("Веб-сервер запущен");
}

// Настройка WiFi
bool setupWiFi() {
    // Проверяем, сохранены ли настройки WiFi
    if (strlen(sysSettings.wifiSSID) == 0) {
        Serial.println("WiFi не настроен, создаем точку доступа");
        return false;
    }
    
    Serial.print("Подключение к WiFi сети: ");
    Serial.println(sysSettings.wifiSSID);
    
    // Запускаем WiFi в режиме клиента
    WiFi.mode(WIFI_STA);
    WiFi.begin(sysSettings.wifiSSID, sysSettings.wifiPassword);
    
    // Ждем подключения с таймаутом
    int connectionAttempts = 0;
    while (WiFi.status() != WL_CONNECTED && connectionAttempts < 20) {
        delay(500);
        Serial.print(".");
        connectionAttempts++;
    }
    
    if (WiFi.status() == WL_CONNECTED) {
        Serial.println();
        Serial.print("WiFi подключен, IP-адрес: ");
        Serial.println(WiFi.localIP());
        return true;
    } else {
        Serial.println();
        Serial.println("Не удалось подключиться к WiFi, создаем точку доступа");
        return false;
    }
}

// Настройка точки доступа
void setupAccessPoint() {
    Serial.println("Настройка точки доступа...");
    
    // Запускаем WiFi в режиме точки доступа
    WiFi.mode(WIFI_AP);
    
    // Генерируем имя точки доступа на основе MAC-адреса
    String mac = WiFi.macAddress();
    mac.replace(":", "");
    String apName = "Distiller_" + mac.substring(mac.length() - 4);
    
    // Запускаем точку доступа
    WiFi.softAP(apName.c_str(), "distiller123");
    
    Serial.print("Точка доступа создана, имя: ");
    Serial.println(apName);
    Serial.print("IP-адрес: ");
    Serial.println(WiFi.softAPIP());
}

// Настройка маршрутов веб-сервера
void setupWebRoutes() {
    // Обслуживание файлов из SPIFFS
    server.serveStatic("/", SPIFFS, "/").setDefaultFile("index.html");
    
    // Маршрут для получения текущих температур
    server.on("/api/temperatures", HTTP_GET, [](AsyncWebServerRequest *request) {
        AsyncResponseStream *response = request->beginResponseStream("application/json");
        DynamicJsonDocument doc(512);
        
        // Добавляем температуры в JSON
        JsonArray temps = doc.createNestedArray("temperatures");
        
        for (int i = 0; i < MAX_TEMP_SENSORS; i++) {
            JsonObject sensor = temps.createNestedObject();
            
            sensor["id"] = i;
            sensor["name"] = getTempSensorName(i);
            sensor["temperature"] = temperatures[i];
            sensor["connected"] = isSensorConnected(i);
        }
        
        serializeJson(doc, *response);
        request->send(response);
    });
    
    // Маршрут для получения текущего статуса системы
    server.on("/api/status", HTTP_GET, [](AsyncWebServerRequest *request) {
        AsyncResponseStream *response = request->beginResponseStream("application/json");
        DynamicJsonDocument doc(1024);
        
        // Общий статус системы
        doc["running"] = systemRunning;
        doc["paused"] = systemPaused;
        doc["mode"] = currentMode;
        doc["modeName"] = (currentMode == MODE_RECTIFICATION) ? "Ректификация" : "Дистилляция";
        
        // Время работы процесса
        if (systemRunning) {
            doc["uptime"] = (millis() - processStartTime) / 1000;
            doc["uptimeFormatted"] = getFormattedTime(millis() - processStartTime);
        } else {
            doc["uptime"] = 0;
            doc["uptimeFormatted"] = "00:00:00";
        }
        
        // Информация о мощности
        doc["power"] = {
            {"percent", getCurrentPowerPercent()},
            {"watts", getCurrentPowerWatts()},
            {"mode", sysSettings.powerControlMode},
            {"modeName", getPowerControlModeName(sysSettings.powerControlMode)}
        };
        
        // Информация о текущей фазе процесса
        if (systemRunning) {
            if (currentMode == MODE_RECTIFICATION) {
                doc["phase"] = {
                    {"id", rectPhase},
                    {"name", getPhaseNameRussian(rectPhase)},
                    {"headsCollected", headsCollected},
                    {"bodyCollected", bodyCollected},
                    {"tailsCollected", tailsCollected},
                    {"totalCollected", headsCollected + bodyCollected + tailsCollected}
                };
            } 
            else if (currentMode == MODE_DISTILLATION) {
                doc["phase"] = {
                    {"id", distPhase},
                    {"name", getDistPhaseNameRussian(distPhase)},
                    {"collected", distillationCollected}
                };
            }
        }
        
        // PZEM информация, если включен
        if (sysSettings.pzemEnabled) {
            doc["pzem"] = {
                {"power", getPzemPowerWatts()},
                {"voltage", getPzemVoltage()},
                {"current", getPzemCurrent()},
                {"energy", getPzemEnergy()}
            };
        }
        
        serializeJson(doc, *response);
        request->send(response);
    });
    
    // Маршрут для получения настроек
    server.on("/api/settings", HTTP_GET, [](AsyncWebServerRequest *request) {
        AsyncResponseStream *response = request->beginResponseStream("application/json");
        DynamicJsonDocument doc(2048);
        
        // Системные настройки
        doc["system"] = {
            {"maxHeaterPower", sysSettings.maxHeaterPowerWatts},
            {"powerControlMode", sysSettings.powerControlMode},
            {"pzemEnabled", sysSettings.pzemEnabled},
            {"soundEnabled", sysSettings.soundEnabled},
            {"soundVolume", sysSettings.soundVolume},
            {"displayEnabled", sysSettings.displaySettings.enabled},
            {"displayBrightness", sysSettings.displaySettings.brightness},
            {"tempUpdateInterval", sysSettings.tempUpdateInterval},
            {"wifiSSID", sysSettings.wifiSSID}
        };
        
        // Настройки PI-регулятора
        doc["pi"] = {
            {"kp", sysSettings.piSettings.kp},
            {"ki", sysSettings.piSettings.ki},
            {"outputMin", sysSettings.piSettings.outputMin},
            {"outputMax", sysSettings.piSettings.outputMax},
            {"integralLimit", sysSettings.piSettings.integralLimit}
        };
        
        // Настройки ректификации
        doc["rectification"] = {
            {"model", rectParams.model},
            {"modelName", (rectParams.model == MODEL_CLASSIC) ? "Классическая" : "Альтернативная"},
            {"maxCubeTemp", rectParams.maxCubeTemp},
            {"headsTemp", rectParams.headsTemp},
            {"bodyTemp", rectParams.bodyTemp},
            {"tailsTemp", rectParams.tailsTemp},
            {"endTemp", rectParams.endTemp},
            {"heatingPower", rectParams.heatingPower},
            {"heatingPowerWatts", rectParams.heatingPowerWatts},
            {"stabilizationPower", rectParams.stabilizationPower},
            {"stabilizationPowerWatts", rectParams.stabilizationPowerWatts},
            {"bodyPower", rectParams.bodyPower},
            {"bodyPowerWatts", rectParams.bodyPowerWatts},
            {"tailsPower", rectParams.tailsPower},
            {"tailsPowerWatts", rectParams.tailsPowerWatts},
            {"stabilizationTime", rectParams.stabilizationTime},
            {"headsVolume", rectParams.headsVolume},
            {"bodyVolume", rectParams.bodyVolume},
            {"refluxRatio", rectParams.refluxRatio},
            {"refluxPeriod", rectParams.refluxPeriod}
        };
        
        // Дополнительные параметры для альтернативной модели
        doc["rectification"]["alternative"] = {
            {"headsTargetTime", rectParams.headsTargetTimeMinutes},
            {"postHeadsStabilizationTime", rectParams.postHeadsStabilizationTime},
            {"bodyFlowRate", rectParams.bodyFlowRateMlPerHour},
            {"tempDeltaEndBody", rectParams.tempDeltaEndBody},
            {"tailsCubeTemp", rectParams.tailsCubeTemp},
            {"tailsFlowRate", rectParams.tailsFlowRateMlPerHour},
            {"useSameFlowForTails", rectParams.useSameFlowRateForTails}
        };
        
        // Настройки дистилляции
        doc["distillation"] = {
            {"maxCubeTemp", distParams.maxCubeTemp},
            {"startCollectingTemp", distParams.startCollectingTemp},
            {"endTemp", distParams.endTemp},
            {"heatingPower", distParams.heatingPower},
            {"heatingPowerWatts", distParams.heatingPowerWatts},
            {"distillationPower", distParams.distillationPower},
            {"distillationPowerWatts", distParams.distillationPowerWatts},
            {"flowRate", distParams.flowRate},
            {"separateHeads", distParams.separateHeads},
            {"headsVolume", distParams.headsVolume},
            {"headsFlowRate", distParams.headsFlowRate}
        };
        
        // Настройки насоса
        doc["pump"] = {
            {"calibrationFactor", pumpSettings.calibrationFactor},
            {"headsFlowRate", pumpSettings.headsFlowRate},
            {"bodyFlowRate", pumpSettings.bodyFlowRate},
            {"tailsFlowRate", pumpSettings.tailsFlowRate},
            {"minFlowRate", pumpSettings.minFlowRate},
            {"maxFlowRate", pumpSettings.maxFlowRate},
            {"pumpPeriodMs", pumpSettings.pumpPeriodMs}
        };
        
        // Настройки датчиков температуры
        JsonArray sensors = doc.createNestedArray("temperatureSensors");
        
        for (int i = 0; i < MAX_TEMP_SENSORS; i++) {
            JsonObject sensor = sensors.createNestedObject();
            
            sensor["id"] = i;
            sensor["name"] = getTempSensorName(i);
            sensor["enabled"] = sysSettings.tempSensorEnabled[i];
            sensor["calibration"] = sysSettings.tempSensorCalibration[i];
            
            // Добавляем адрес датчика в виде строки
            if (sysSettings.tempSensorEnabled[i]) {
                char addrStr[24] = {0};
                for (int j = 0; j < 8; j++) {
                    sprintf(&addrStr[j*3], "%02X", sysSettings.tempSensorAddresses[i][j]);
                    if (j < 7) addrStr[j*3+2] = ':';
                }
                sensor["address"] = addrStr;
            } else {
                sensor["address"] = "";
            }
        }
        
        serializeJson(doc, *response);
        request->send(response);
    });
    
    // Маршрут для запуска процесса
    server.on("/api/start", HTTP_POST, handleStartProcess);
    
    // Маршрут для остановки процесса
    server.on("/api/stop", HTTP_POST, handleStopProcess);
    
    // Маршрут для изменения мощности
    server.on("/api/power", HTTP_POST, handlePowerChange);
    
    // Маршрут для обновления настроек ректификации
    server.on("/api/settings/rectification", HTTP_POST, handleRectificationSettings);
    
    // Маршрут для обновления настроек дистилляции
    server.on("/api/settings/distillation", HTTP_POST, handleDistillationSettings);
    
    // Маршрут для обновления системных настроек
    server.on("/api/settings/system", HTTP_POST, handleSystemSettings);
    
    // Маршрут для калибровки датчиков температуры
    server.on("/api/calibrate/temperature", HTTP_POST, [](AsyncWebServerRequest *request) {
        AsyncJsonResponse *response = new AsyncJsonResponse();
        JsonVariant json = response->getRoot();
        
        bool success = false;
        String message = "Не удалось выполнить калибровку";
        
        if (request->hasParam("id", true) && request->hasParam("offset", true)) {
            int id = request->getParam("id", true)->value().toInt();
            float offset = request->getParam("offset", true)->value().toFloat();
            
            if (id >= 0 && id < MAX_TEMP_SENSORS) {
                calibrateTempSensor(id, offset);
                success = true;
                message = "Датчик успешно откалиброван";
            } else {
                message = "Неверный ID датчика";
            }
        } else {
            message = "Отсутствуют необходимые параметры";
        }
        
        json["success"] = success;
        json["message"] = message;
        
        response->setLength();
        request->send(response);
    });
    
    // Маршрут для калибровки насоса
    server.on("/api/calibrate/pump", HTTP_POST, [](AsyncWebServerRequest *request) {
        AsyncJsonResponse *response = new AsyncJsonResponse();
        JsonVariant json = response->getRoot();
        
        bool success = false;
        String message = "Не удалось выполнить калибровку насоса";
        
        if (request->hasParam("factor", true)) {
            float factor = request->getParam("factor", true)->value().toFloat();
            
            if (factor > 0.0) {
                calibratePump(factor);
                success = true;
                message = "Насос успешно откалиброван";
            } else {
                message = "Неверное значение коэффициента калибровки";
            }
        } else {
            message = "Отсутствуют необходимые параметры";
        }
        
        json["success"] = success;
        json["message"] = message;
        
        response->setLength();
        request->send(response);
    });
    
    // Маршрут для сканирования датчиков температуры
    server.on("/api/scan/sensors", HTTP_POST, [](AsyncWebServerRequest *request) {
        AsyncJsonResponse *response = new AsyncJsonResponse();
        JsonVariant json = response->getRoot();
        
        bool success = scanForTempSensors();
        String message = success ? "Датчики успешно отсканированы" : "Датчики не найдены";
        
        json["success"] = success;
        json["message"] = message;
        json["sensorsCount"] = getConnectedSensorsCount();
        
        response->setLength();
        request->send(response);
    });
    
    // Маршрут для сброса настроек к значениям по умолчанию
    server.on("/api/reset", HTTP_POST, [](AsyncWebServerRequest *request) {
        AsyncJsonResponse *response = new AsyncJsonResponse();
        JsonVariant json = response->getRoot();
        
        bool success = resetAllSettings();
        String message = success ? "Настройки сброшены к значениям по умолчанию" : "Не удалось сбросить настройки";
        
        json["success"] = success;
        json["message"] = message;
        
        response->setLength();
        request->send(response);
    });
    
    // Маршрут для загрузки файлов (обновлений интерфейса)
    server.on("/api/upload", HTTP_POST, 
        [](AsyncWebServerRequest *request) {
            request->send(200);
        },
        handleFileUpload
    );
    
    // Маршрут для перезагрузки устройства
    server.on("/api/reboot", HTTP_POST, [](AsyncWebServerRequest *request) {
        AsyncJsonResponse *response = new AsyncJsonResponse();
        JsonVariant json = response->getRoot();
        
        json["success"] = true;
        json["message"] = "Устройство перезагружается...";
        
        response->setLength();
        request->send(response);
        
        // Перезагрузка через 1 секунду после отправки ответа
        delay(1000);
        ESP.restart();
    });
    
    // Маршрут для паузы/возобновления процесса
    server.on("/api/pause", HTTP_POST, [](AsyncWebServerRequest *request) {
        AsyncJsonResponse *response = new AsyncJsonResponse();
        JsonVariant json = response->getRoot();
        
        bool success = false;
        String message = "Не удалось выполнить действие";
        
        if (systemRunning) {
            if (systemPaused) {
                resumeProcess();
                success = true;
                message = "Процесс возобновлен";
            } else {
                pauseProcess();
                success = true;
                message = "Процесс приостановлен";
            }
        } else {
            message = "Процесс не запущен";
        }
        
        json["success"] = success;
        json["message"] = message;
        json["paused"] = systemPaused;
        
        response->setLength();
        request->send(response);
    });
    
    // Обработка запроса не найденных страниц
    server.onNotFound([](AsyncWebServerRequest *request) {
        if (request->method() == HTTP_OPTIONS) {
            // Обработка CORS preflight запроса
            request->send(200);
        } else {
            // Перенаправляем на главную страницу
            request->redirect("/");
        }
    });
}

// Настройка WebSocket сервера
void setupWebSocket() {
    // Добавляем обработчик WebSocket событий
    ws.onEvent(onWebSocketEvent);
    
    // Добавляем WebSocket сервер к основному серверу
    server.addHandler(&ws);
    
    Serial.println("WebSocket сервер настроен");
}

// Обработчик WebSocket событий
void onWebSocketEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, 
                      AwsEventType type, void *arg, uint8_t *data, size_t len) {
    switch (type) {
        case WS_EVT_CONNECT:
            Serial.printf("WebSocket клиент #%u подключился с %s\n", 
                          client->id(), client->remoteIP().toString().c_str());
            
            // Отправляем текущий статус при подключении
            sendStatusWebSocket();
            sendTemperaturesWebSocket();
            break;
            
        case WS_EVT_DISCONNECT:
            Serial.printf("WebSocket клиент #%u отключился\n", client->id());
            break;
            
        case WS_EVT_DATA:
            // Обработка полученных данных
            handleWebSocketMessage(arg, data, len);
            break;
            
        case WS_EVT_PONG:
            break;
            
        case WS_EVT_ERROR:
            Serial.printf("WebSocket ошибка для клиента #%u\n", client->id());
            break;
    }
}

// Обработка сообщений WebSocket
void handleWebSocketMessage(void *arg, uint8_t *data, size_t len) {
    AwsFrameInfo *info = (AwsFrameInfo*)arg;
    
    if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
        // Добавляем нулевой символ для корректной обработки строк
        data[len] = 0;
        
        // Преобразуем данные в строку
        String message = String((char*)data);
        
        // Преобразуем JSON
        DynamicJsonDocument doc(1024);
        DeserializationError error = deserializeJson(doc, message);
        
        if (!error) {
            // Проверяем тип команды
            if (doc.containsKey("command")) {
                String command = doc["command"];
                
                if (command == "getPower") {
                    // Команда получения текущей мощности
                    DynamicJsonDocument responseDoc(256);
                    responseDoc["type"] = "power";
                    responseDoc["percent"] = getCurrentPowerPercent();
                    responseDoc["watts"] = getCurrentPowerWatts();
                    
                    String responseStr;
                    serializeJson(responseDoc, responseStr);
                    ws.textAll(responseStr);
                }
                else if (command == "setPower") {
                    // Команда установки мощности
                    if (doc.containsKey("percent")) {
                        int percent = doc["percent"];
                        setPowerPercent(percent);
                        
                        // Отправляем обновленный статус
                        sendStatusWebSocket();
                    }
                }
            }
        }
    }
}

// Отправка данных о температурах через WebSocket
void sendTemperaturesWebSocket() {
    DynamicJsonDocument doc(512);
    doc["type"] = "temperatures";
    
    JsonArray temps = doc.createNestedArray("values");
    
    for (int i = 0; i < MAX_TEMP_SENSORS; i++) {
        JsonObject sensor = temps.createNestedObject();
        
        sensor["id"] = i;
        sensor["name"] = getTempSensorName(i);
        sensor["temperature"] = temperatures[i];
        sensor["connected"] = isSensorConnected(i);
    }
    
    String output;
    serializeJson(doc, output);
    ws.textAll(output);
}

// Отправка данных о статусе системы через WebSocket
void sendStatusWebSocket() {
    DynamicJsonDocument doc(1024);
    doc["type"] = "status";
    
    // Общий статус системы
    doc["running"] = systemRunning;
    doc["paused"] = systemPaused;
    doc["mode"] = currentMode;
    doc["modeName"] = (currentMode == MODE_RECTIFICATION) ? "Ректификация" : "Дистилляция";
    
    // Время работы процесса
    if (systemRunning) {
        doc["uptime"] = (millis() - processStartTime) / 1000;
        doc["uptimeFormatted"] = getFormattedTime(millis() - processStartTime);
    } else {
        doc["uptime"] = 0;
        doc["uptimeFormatted"] = "00:00:00";
    }
    
    // Информация о мощности
    doc["power"] = {
        {"percent", getCurrentPowerPercent()},
        {"watts", getCurrentPowerWatts()},
        {"mode", sysSettings.powerControlMode},
        {"modeName", getPowerControlModeName(sysSettings.powerControlMode)}
    };
    
    // Информация о текущей фазе процесса
    if (systemRunning) {
        if (currentMode == MODE_RECTIFICATION) {
            doc["phase"] = {
                {"id", rectPhase},
                {"name", getPhaseNameRussian(rectPhase)},
                {"headsCollected", headsCollected},
                {"bodyCollected", bodyCollected},
                {"tailsCollected", tailsCollected},
                {"totalCollected", headsCollected + bodyCollected + tailsCollected}
            };
        } 
        else if (currentMode == MODE_DISTILLATION) {
            doc["phase"] = {
                {"id", distPhase},
                {"name", getDistPhaseNameRussian(distPhase)},
                {"collected", distillationCollected}
            };
        }
    }
    
    // PZEM информация, если включен
    if (sysSettings.pzemEnabled) {
        doc["pzem"] = {
            {"power", getPzemPowerWatts()},
            {"voltage", getPzemVoltage()},
            {"current", getPzemCurrent()},
            {"energy", getPzemEnergy()}
        };
    }
    
    String output;
    serializeJson(doc, output);
    ws.textAll(output);
}

// Отправка уведомления клиентам
void sendNotificationToClients(NotificationType type, const String& message) {
    DynamicJsonDocument doc(256);
    doc["type"] = "notification";
    
    switch (type) {
        case NOTIFY_INFO:
            doc["notifyType"] = "info";
            break;
        case NOTIFY_SUCCESS:
            doc["notifyType"] = "success";
            break;
        case NOTIFY_WARNING:
            doc["notifyType"] = "warning";
            break;
        case NOTIFY_ERROR:
            doc["notifyType"] = "error";
            break;
        default:
            doc["notifyType"] = "info";
            break;
    }
    
    doc["message"] = message;
    
    String output;
    serializeJson(doc, output);
    ws.textAll(output);
}

// Обработка запроса на начало процесса
void handleStartProcess(AsyncWebServerRequest *request) {
    AsyncJsonResponse *response = new AsyncJsonResponse();
    JsonVariant json = response->getRoot();
    
    bool success = false;
    String message = "Не удалось запустить процесс";
    
    if (systemRunning) {
        message = "Процесс уже запущен";
    } else {
        if (request->hasParam("mode", true)) {
            int mode = request->getParam("mode", true)->value().toInt();
            
            if (mode == MODE_RECTIFICATION || mode == MODE_DISTILLATION) {
                currentMode = (OperationMode)mode;
                
                // Проверяем, что все необходимые датчики подключены
                if (checkRequiredSensors()) {
                    startProcess();
                    success = true;
                    message = "Процесс запущен";
                } else {
                    message = "Не все необходимые датчики подключены";
                }
            } else {
                message = "Неверный режим работы";
            }
        } else {
            message = "Не указан режим работы";
        }
    }
    
    json["success"] = success;
    json["message"] = message;
    
    response->setLength();
    request->send(response);
}

// Обработка запроса на остановку процесса
void handleStopProcess(AsyncWebServerRequest *request) {
    AsyncJsonResponse *response = new AsyncJsonResponse();
    JsonVariant json = response->getRoot();
    
    bool success = false;
    String message = "Не удалось остановить процесс";
    
    if (systemRunning) {
        stopProcess();
        success = true;
        message = "Процесс остановлен";
    } else {
        message = "Процесс не запущен";
    }
    
    json["success"] = success;
    json["message"] = message;
    
    response->setLength();
    request->send(response);
}

// Обработка запроса на изменение мощности
void handlePowerChange(AsyncWebServerRequest *request) {
    AsyncJsonResponse *response = new AsyncJsonResponse();
    JsonVariant json = response->getRoot();
    
    bool success = false;
    String message = "Не удалось изменить мощность";
    
    if (request->hasParam("percent", true)) {
        int percent = request->getParam("percent", true)->value().toInt();
        
        if (percent >= 0 && percent <= 100) {
            setPowerPercent(percent);
            success = true;
            message = "Мощность установлена: " + String(percent) + "%";
        } else {
            message = "Неверное значение мощности. Допустимый диапазон: 0-100%";
        }
    } else if (request->hasParam("watts", true)) {
        int watts = request->getParam("watts", true)->value().toInt();
        
        if (watts >= 0 && watts <= sysSettings.maxHeaterPowerWatts) {
            setPowerWatts(watts);
            success = true;
            message = "Мощность установлена: " + String(watts) + "Вт";
        } else {
            message = "Неверное значение мощности. Допустимый диапазон: 0-" + 
                     String(sysSettings.maxHeaterPowerWatts) + "Вт";
        }
    } else {
        message = "Не указано значение мощности";
    }
    
    json["success"] = success;
    json["message"] = message;
    
    response->setLength();
    request->send(response);
}

// Обработка запроса на настройку параметров ректификации
void handleRectificationSettings(AsyncWebServerRequest *request) {
    AsyncJsonResponse *response = new AsyncJsonResponse();
    JsonVariant json = response->getRoot();
    
    bool success = true;
    String message = "Настройки ректификации обновлены";
    
    // Обновляем параметры из запроса
    if (request->hasParam("model", true)) {
        rectParams.model = (RectificationModel)request->getParam("model", true)->value().toInt();
    }
    
    if (request->hasParam("maxCubeTemp", true)) {
        rectParams.maxCubeTemp = request->getParam("maxCubeTemp", true)->value().toFloat();
    }
    
    if (request->hasParam("headsTemp", true)) {
        rectParams.headsTemp = request->getParam("headsTemp", true)->value().toFloat();
    }
    
    if (request->hasParam("bodyTemp", true)) {
        rectParams.bodyTemp = request->getParam("bodyTemp", true)->value().toFloat();
    }
    
    if (request->hasParam("tailsTemp", true)) {
        rectParams.tailsTemp = request->getParam("tailsTemp", true)->value().toFloat();
    }
    
    if (request->hasParam("endTemp", true)) {
        rectParams.endTemp = request->getParam("endTemp", true)->value().toFloat();
    }
    
    if (request->hasParam("heatingPowerWatts", true)) {
        rectParams.heatingPowerWatts = request->getParam("heatingPowerWatts", true)->value().toInt();
        rectParams.heatingPower = wattsToPercent(rectParams.heatingPowerWatts);
    } else if (request->hasParam("heatingPower", true)) {
        rectParams.heatingPower = request->getParam("heatingPower", true)->value().toInt();
        rectParams.heatingPowerWatts = percentToWatts(rectParams.heatingPower);
    }
    
    if (request->hasParam("stabilizationPowerWatts", true)) {
        rectParams.stabilizationPowerWatts = request->getParam("stabilizationPowerWatts", true)->value().toInt();
        rectParams.stabilizationPower = wattsToPercent(rectParams.stabilizationPowerWatts);
    } else if (request->hasParam("stabilizationPower", true)) {
        rectParams.stabilizationPower = request->getParam("stabilizationPower", true)->value().toInt();
        rectParams.stabilizationPowerWatts = percentToWatts(rectParams.stabilizationPower);
    }
    
    if (request->hasParam("bodyPowerWatts", true)) {
        rectParams.bodyPowerWatts = request->getParam("bodyPowerWatts", true)->value().toInt();
        rectParams.bodyPower = wattsToPercent(rectParams.bodyPowerWatts);
    } else if (request->hasParam("bodyPower", true)) {
        rectParams.bodyPower = request->getParam("bodyPower", true)->value().toInt();
        rectParams.bodyPowerWatts = percentToWatts(rectParams.bodyPower);
    }
    
    if (request->hasParam("tailsPowerWatts", true)) {
        rectParams.tailsPowerWatts = request->getParam("tailsPowerWatts", true)->value().toInt();
        rectParams.tailsPower = wattsToPercent(rectParams.tailsPowerWatts);
    } else if (request->hasParam("tailsPower", true)) {
        rectParams.tailsPower = request->getParam("tailsPower", true)->value().toInt();
        rectParams.tailsPowerWatts = percentToWatts(rectParams.tailsPower);
    }
    
    if (request->hasParam("stabilizationTime", true)) {
        rectParams.stabilizationTime = request->getParam("stabilizationTime", true)->value().toInt();
    }
    
    if (request->hasParam("headsVolume", true)) {
        rectParams.headsVolume = request->getParam("headsVolume", true)->value().toFloat();
    }
    
    if (request->hasParam("bodyVolume", true)) {
        rectParams.bodyVolume = request->getParam("bodyVolume", true)->value().toFloat();
    }
    
    if (request->hasParam("refluxRatio", true)) {
        rectParams.refluxRatio = request->getParam("refluxRatio", true)->value().toFloat();
    }
    
    if (request->hasParam("refluxPeriod", true)) {
        rectParams.refluxPeriod = request->getParam("refluxPeriod", true)->value().toInt();
    }
    
    // Параметры для альтернативной модели
    if (request->hasParam("headsTargetTime", true)) {
        rectParams.headsTargetTimeMinutes = request->getParam("headsTargetTime", true)->value().toInt();
    }
    
    if (request->hasParam("postHeadsStabilizationTime", true)) {
        rectParams.postHeadsStabilizationTime = request->getParam("postHeadsStabilizationTime", true)->value().toInt();
    }
    
    if (request->hasParam("bodyFlowRate", true)) {
        rectParams.bodyFlowRateMlPerHour = request->getParam("bodyFlowRate", true)->value().toFloat();
    }
    
    if (request->hasParam("tempDeltaEndBody", true)) {
        rectParams.tempDeltaEndBody = request->getParam("tempDeltaEndBody", true)->value().toFloat();
    }
    
    if (request->hasParam("tailsCubeTemp", true)) {
        rectParams.tailsCubeTemp = request->getParam("tailsCubeTemp", true)->value().toFloat();
    }
    
    if (request->hasParam("tailsFlowRate", true)) {
        rectParams.tailsFlowRateMlPerHour = request->getParam("tailsFlowRate", true)->value().toFloat();
    }
    
    if (request->hasParam("useSameFlowForTails", true)) {
        rectParams.useSameFlowRateForTails = (request->getParam("useSameFlowForTails", true)->value() == "true");
    }
    
    // Сохраняем параметры
    success = saveRectificationParams();
    if (!success) {
        message = "Не удалось сохранить настройки ректификации";
    }
    
    json["success"] = success;
    json["message"] = message;
    
    response->setLength();
    request->send(response);
}

// Обработка запроса на настройку параметров дистилляции
void handleDistillationSettings(AsyncWebServerRequest *request) {
    AsyncJsonResponse *response = new AsyncJsonResponse();
    JsonVariant json = response->getRoot();
    
    bool success = true;
    String message = "Настройки дистилляции обновлены";
    
    // Обновляем параметры из запроса
    if (request->hasParam("maxCubeTemp", true)) {
        distParams.maxCubeTemp = request->getParam("maxCubeTemp", true)->value().toFloat();
    }
    
    if (request->hasParam("startCollectingTemp", true)) {
        distParams.startCollectingTemp = request->getParam("startCollectingTemp", true)->value().toFloat();
    }
    
    if (request->hasParam("endTemp", true)) {
        distParams.endTemp = request->getParam("endTemp", true)->value().toFloat();
    }
    
    if (request->hasParam("heatingPowerWatts", true)) {
        distParams.heatingPowerWatts = request->getParam("heatingPowerWatts", true)->value().toInt();
        distParams.heatingPower = wattsToPercent(distParams.heatingPowerWatts);
    } else if (request->hasParam("heatingPower", true)) {
        distParams.heatingPower = request->getParam("heatingPower", true)->value().toInt();
        distParams.heatingPowerWatts = percentToWatts(distParams.heatingPower);
    }
    
    if (request->hasParam("distillationPowerWatts", true)) {
        distParams.distillationPowerWatts = request->getParam("distillationPowerWatts", true)->value().toInt();
        distParams.distillationPower = wattsToPercent(distParams.distillationPowerWatts);
    } else if (request->hasParam("distillationPower", true)) {
        distParams.distillationPower = request->getParam("distillationPower", true)->value().toInt();
        distParams.distillationPowerWatts = percentToWatts(distParams.distillationPower);
    }
    
    if (request->hasParam("flowRate", true)) {
        distParams.flowRate = request->getParam("flowRate", true)->value().toFloat();
    }
    
    if (request->hasParam("separateHeads", true)) {
        distParams.separateHeads = (request->getParam("separateHeads", true)->value() == "true");
    }
    
    if (request->hasParam("headsVolume", true)) {
        distParams.headsVolume = request->getParam("headsVolume", true)->value().toFloat();
    }
    
    if (request->hasParam("headsFlowRate", true)) {
        distParams.headsFlowRate = request->getParam("headsFlowRate", true)->value().toFloat();
    }
    
    // Сохраняем параметры
    success = saveDistillationParams();
    if (!success) {
        message = "Не удалось сохранить настройки дистилляции";
    }
    
    json["success"] = success;
    json["message"] = message;
    
    response->setLength();
    request->send(response);
}

// Обработка запроса на настройку системных параметров
void handleSystemSettings(AsyncWebServerRequest *request) {
    AsyncJsonResponse *response = new AsyncJsonResponse();
    JsonVariant json = response->getRoot();
    
    bool success = true;
    String message = "Системные настройки обновлены";
    
    // Обновляем параметры из запроса
    if (request->hasParam("maxHeaterPower", true)) {
        sysSettings.maxHeaterPowerWatts = request->getParam("maxHeaterPower", true)->value().toInt();
    }
    
    if (request->hasParam("powerControlMode", true)) {
        sysSettings.powerControlMode = (PowerControlMode)request->getParam("powerControlMode", true)->value().toInt();
    }
    
    if (request->hasParam("pzemEnabled", true)) {
        sysSettings.pzemEnabled = (request->getParam("pzemEnabled", true)->value() == "true");
    }
    
    if (request->hasParam("soundEnabled", true)) {
        sysSettings.soundEnabled = (request->getParam("soundEnabled", true)->value() == "true");
    }
    
    if (request->hasParam("soundVolume", true)) {
        sysSettings.soundVolume = request->getParam("soundVolume", true)->value().toInt();
    }
    
    // Настройки PI-регулятора
    if (request->hasParam("piKp", true)) {
        sysSettings.piSettings.kp = request->getParam("piKp", true)->value().toFloat();
    }
    
    if (request->hasParam("piKi", true)) {
        sysSettings.piSettings.ki = request->getParam("piKi", true)->value().toFloat();
    }
    
    if (request->hasParam("piOutputMin", true)) {
        sysSettings.piSettings.outputMin = request->getParam("piOutputMin", true)->value().toFloat();
    }
    
    if (request->hasParam("piOutputMax", true)) {
        sysSettings.piSettings.outputMax = request->getParam("piOutputMax", true)->value().toFloat();
    }
    
    if (request->hasParam("piIntegralLimit", true)) {
        sysSettings.piSettings.integralLimit = request->getParam("piIntegralLimit", true)->value().toFloat();
    }
    
    // Настройки дисплея
    if (request->hasParam("displayEnabled", true)) {
        sysSettings.displaySettings.enabled = (request->getParam("displayEnabled", true)->value() == "true");
    }
    
    if (request->hasParam("displayBrightness", true)) {
        sysSettings.displaySettings.brightness = request->getParam("displayBrightness", true)->value().toInt();
    }
    
    if (request->hasParam("displayRotation", true)) {
        sysSettings.displaySettings.rotation = request->getParam("displayRotation", true)->value().toInt();
    }
    
    if (request->hasParam("displayInvertColors", true)) {
        sysSettings.displaySettings.invertColors = (request->getParam("displayInvertColors", true)->value() == "true");
    }
    
    if (request->hasParam("displayContrast", true)) {
        sysSettings.displaySettings.contrast = request->getParam("displayContrast", true)->value().toInt();
    }
    
    if (request->hasParam("displayTimeout", true)) {
        sysSettings.displaySettings.timeout = request->getParam("displayTimeout", true)->value().toInt();
    }
    
    if (request->hasParam("displayShowLogo", true)) {
        sysSettings.displaySettings.showLogo = (request->getParam("displayShowLogo", true)->value() == "true");
    }
    
    // Настройки WiFi
    if (request->hasParam("wifiSSID", true) && request->hasParam("wifiPassword", true)) {
        String ssid = request->getParam("wifiSSID", true)->value();
        String password = request->getParam("wifiPassword", true)->value();
        
        // Проверяем корректность параметров
        if (ssid.length() > 0 && ssid.length() <= 32 && password.length() <= 64) {
            strncpy(sysSettings.wifiSSID, ssid.c_str(), sizeof(sysSettings.wifiSSID) - 1);
            strncpy(sysSettings.wifiPassword, password.c_str(), sizeof(sysSettings.wifiPassword) - 1);
            sysSettings.wifiSSID[sizeof(sysSettings.wifiSSID) - 1] = '\0';
            sysSettings.wifiPassword[sizeof(sysSettings.wifiPassword) - 1] = '\0';
        }
    }
    
    // Сохраняем параметры
    success = saveSystemSettings();
    if (!success) {
        message = "Не удалось сохранить системные настройки";
    }
    
    json["success"] = success;
    json["message"] = message;
    
    response->setLength();
    request->send(response);
}

// Обработка загрузки файлов
void handleFileUpload(AsyncWebServerRequest *request, const String& filename,
                      size_t index, uint8_t *data, size_t len, bool final) {
    if (!index) {
        Serial.printf("Загрузка файла: %s\n", filename.c_str());
        
        // Открываем файл для записи
        if (SPIFFS.exists("/" + filename)) {
            SPIFFS.remove("/" + filename);
        }
        
        File file = SPIFFS.open("/" + filename, FILE_WRITE);
        if (!file) {
            Serial.println("Не удалось открыть файл для записи");
            request->send(500, "text/plain", "Не удалось открыть файл для записи");
            return;
        }
        
        // Записываем первую часть данных
        file.write(data, len);
        file.close();
    } else {
        // Дописываем данные
        File file = SPIFFS.open("/" + filename, FILE_APPEND);
        if (!file) {
            Serial.println("Не удалось открыть файл для дописывания");
            request->send(500, "text/plain", "Не удалось открыть файл для дописывания");
            return;
        }
        
        file.write(data, len);
        file.close();
    }
    
    if (final) {
        Serial.printf("Файл %s загружен\n", filename.c_str());
        
        // Отправляем успешный ответ
        request->send(200, "text/plain", "Файл загружен");
    }
}

// Настройка файловой системы SPIFFS
bool setupSPIFFS() {
    if (!SPIFFS.begin(true)) {
        Serial.println("Ошибка монтирования SPIFFS");
        return false;
    }
    
    Serial.println("SPIFFS смонтирована");
    
    // Выводим список файлов
    File root = SPIFFS.open("/");
    File file = root.openNextFile();
    
    while (file) {
        Serial.print("Файл: ");
        Serial.print(file.name());
        Serial.print(" (");
        Serial.print(file.size());
        Serial.println(" байт)");
        file = root.openNextFile();
    }
    
    return true;
}

// Получение имени датчика температуры
String getTempSensorName(int index) {
    switch (index) {
        case TEMP_CUBE:
            return "Куб";
        case TEMP_REFLUX:
            return "Колонна";
        case TEMP_PRODUCT:
            return "Продукт";
        default:
            return "Датчик " + String(index);
    }
}

// Получение названия режима управления мощностью
String getPowerControlModeName(PowerControlMode mode) {
    switch (mode) {
        case POWER_CONTROL_MANUAL:
            return "Ручной";
        case POWER_CONTROL_PI:
            return "PI-регулятор";
        case POWER_CONTROL_PZEM:
            return "По PZEM";
        default:
            return "Неизвестно";
    }
}