#include "web.h"
#include "settings.h"
#include "temp_sensors.h"
#include "heater.h"
#include "pump.h"
#include "valve.h"
#include "rectification.h"
#include "distillation.h"
#include <Arduino.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <LittleFS.h>
#include <ArduinoJson.h>

// Объект веб-сервера на порту 80
AsyncWebServer server(80);

// Объекты для обработки WebSocket
AsyncWebSocket ws("/ws");
bool webSocketActive = false;
unsigned long lastWsUpdate = 0;
const int wsUpdateInterval = 1000; // Обновление по WebSocket каждую секунду

// Инициализация модуля веб-сервера
void initWebServer() {
    // Начинаем работу с файловой системой
    if (!LittleFS.begin(true)) {
        Serial.println("Ошибка монтирования файловой системы LittleFS");
        return;
    }
    
    // Настройка обработчика WebSocket
    ws.onEvent(onWebSocketEvent);
    server.addHandler(&ws);
    
    // Настройка маршрутов API
    setupApiRoutes();
    
    // Настройка маршрутов для статических файлов
    setupStaticRoutes();
    
    // Настройка обработчика для не найденных ресурсов (404)
    server.onNotFound([](AsyncWebServerRequest *request) {
        request->redirect("/");
    });
    
    // Запуск веб-сервера
    server.begin();
    
    Serial.println("Веб-сервер запущен");
}

// Обновление состояния WebSocket соединения
void updateWebSocket() {
    if (!webSocketActive) {
        return;
    }
    
    unsigned long currentTime = millis();
    if (currentTime - lastWsUpdate < wsUpdateInterval) {
        return;
    }
    
    lastWsUpdate = currentTime;
    
    // Создаем JSON объект для отправки статуса
    DynamicJsonDocument doc(1024);
    
    // Добавляем информацию о температуре
    JsonObject temps = doc.createNestedObject("temperatures");
    temps["cube"] = getTemperature(TEMP_CUBE);
    temps["column"] = getTemperature(TEMP_COLUMN);
    temps["reflux"] = getTemperature(TEMP_REFLUX);
    temps["tsa"] = getTemperature(TEMP_TSA);
    temps["waterOut"] = getTemperature(TEMP_WATER_OUT);
    
    // Добавляем информацию о нагревателе
    JsonObject heater = doc.createNestedObject("heater");
    heater["power"] = getHeaterPowerWatts();
    heater["percent"] = getHeaterPowerPercent();
    
    // Добавляем информацию о системе
    JsonObject system = doc.createNestedObject("system");
    system["uptime"] = millis() / 1000;
    
    // Информация о текущем процессе
    if (isRectificationRunning()) {
        JsonObject process = doc.createNestedObject("rectification");
        process["running"] = true;
        process["paused"] = isRectificationPaused();
        process["phase"] = getRectificationPhaseName();
        process["uptime"] = getRectificationUptime();
        process["phaseTime"] = getRectificationPhaseTime();
        process["headsVolume"] = getRectificationHeadsVolume();
        process["bodyVolume"] = getRectificationBodyVolume();
        process["tailsVolume"] = getRectificationTailsVolume();
        process["totalVolume"] = getRectificationTotalVolume();
        process["refluxStatus"] = getRectificationRefluxStatus();
    }
    else if (isDistillationRunning()) {
        JsonObject process = doc.createNestedObject("distillation");
        process["running"] = true;
        process["paused"] = isDistillationPaused();
        process["phase"] = getDistillationPhaseName();
        process["uptime"] = getDistillationUptime();
        process["phaseTime"] = getDistillationPhaseTime();
        process["productVolume"] = getDistillationProductVolume();
        process["headsVolume"] = getDistillationHeadsVolume();
        process["headsMode"] = isDistillationHeadsMode();
    }
    
    // Сериализуем JSON в строку и отправляем
    String jsonString;
    serializeJson(doc, jsonString);
    ws.textAll(jsonString);
}

// Настройка маршрутов API
void setupApiRoutes() {
    // Получение статуса системы
    server.on("/api/status", HTTP_GET, [](AsyncWebServerRequest *request) {
        DynamicJsonDocument doc(1024);
        
        // Информация о температуре
        JsonObject temps = doc.createNestedObject("temperatures");
        temps["cube"] = getTemperature(TEMP_CUBE);
        temps["column"] = getTemperature(TEMP_COLUMN);
        temps["reflux"] = getTemperature(TEMP_REFLUX);
        temps["tsa"] = getTemperature(TEMP_TSA);
        temps["waterOut"] = getTemperature(TEMP_WATER_OUT);
        
        // Информация о подключенных датчиках
        JsonObject sensors = doc.createNestedObject("sensors");
        sensors["cube"] = isSensorConnected(TEMP_CUBE);
        sensors["column"] = isSensorConnected(TEMP_COLUMN);
        sensors["reflux"] = isSensorConnected(TEMP_REFLUX);
        sensors["tsa"] = isSensorConnected(TEMP_TSA);
        sensors["waterOut"] = isSensorConnected(TEMP_WATER_OUT);
        
        // Информация о нагревателе
        JsonObject heater = doc.createNestedObject("heater");
        heater["power"] = getHeaterPowerWatts();
        heater["percent"] = getHeaterPowerPercent();
        
        // Информация о насосе
        JsonObject pump = doc.createNestedObject("pump");
        pump["running"] = isPumpRunning();
        pump["flowRate"] = getPumpFlowRate();
        
        // Информация о клапане
        JsonObject valve = doc.createNestedObject("valve");
        valve["open"] = isValveOpen();
        
        // Информация о текущем процессе
        if (isRectificationRunning()) {
            JsonObject process = doc.createNestedObject("rectification");
            process["running"] = true;
            process["paused"] = isRectificationPaused();
            process["phase"] = getRectificationPhaseName();
            process["uptime"] = getRectificationUptime();
            process["phaseTime"] = getRectificationPhaseTime();
            process["headsVolume"] = getRectificationHeadsVolume();
            process["bodyVolume"] = getRectificationBodyVolume();
            process["tailsVolume"] = getRectificationTailsVolume();
            process["totalVolume"] = getRectificationTotalVolume();
            process["refluxStatus"] = getRectificationRefluxStatus();
        }
        else if (isDistillationRunning()) {
            JsonObject process = doc.createNestedObject("distillation");
            process["running"] = true;
            process["paused"] = isDistillationPaused();
            process["phase"] = getDistillationPhaseName();
            process["uptime"] = getDistillationUptime();
            process["phaseTime"] = getDistillationPhaseTime();
            process["productVolume"] = getDistillationProductVolume();
            process["headsVolume"] = getDistillationHeadsVolume();
            process["headsMode"] = isDistillationHeadsMode();
        }
        else {
            doc["process"] = "idle";
        }
        
        // Отправляем ответ
        String response;
        serializeJson(doc, response);
        request->send(200, "application/json", response);
    });
    
    // Получение настроек
    server.on("/api/settings", HTTP_GET, [](AsyncWebServerRequest *request) {
        DynamicJsonDocument doc(2048);
        
        // Настройки нагревателя
        JsonObject heater = doc.createNestedObject("heater");
        heater["maxPowerWatts"] = sysSettings.heaterSettings.maxPowerWatts;
        
        // Настройки датчиков
        JsonObject sensors = doc.createNestedObject("sensors");
        
        for (int i = 0; i < MAX_TEMP_SENSORS; i++) {
            JsonObject sensor = sensors.createNestedObject(String(i));
            sensor["name"] = getTempSensorName(i);
            sensor["enabled"] = sysSettings.tempSensorEnabled[i];
            sensor["calibration"] = sysSettings.tempSensorCalibration[i];
            
            String address = "";
            for (int j = 0; j < 8; j++) {
                char hex[3];
                sprintf(hex, "%02X", sysSettings.tempSensorAddresses[i][j]);
                address += hex;
                if (j < 7) address += ":";
            }
            sensor["address"] = address;
        }
        
        // Настройки насоса
        JsonObject pump = doc.createNestedObject("pump");
        pump["headsFlowRate"] = sysSettings.pumpSettings.headsFlowRate;
        pump["bodyFlowRate"] = sysSettings.pumpSettings.bodyFlowRate;
        pump["tailsFlowRate"] = sysSettings.pumpSettings.tailsFlowRate;
        
        // Настройки ректификации
        JsonObject rect = doc.createNestedObject("rectification");
        rect["model"] = sysSettings.rectificationSettings.model;
        rect["heatingPowerWatts"] = sysSettings.rectificationSettings.heatingPowerWatts;
        rect["stabilizationPowerWatts"] = sysSettings.rectificationSettings.stabilizationPowerWatts;
        rect["bodyPowerWatts"] = sysSettings.rectificationSettings.bodyPowerWatts;
        rect["tailsPowerWatts"] = sysSettings.rectificationSettings.tailsPowerWatts;
        rect["headsTemp"] = sysSettings.rectificationSettings.headsTemp;
        rect["bodyTemp"] = sysSettings.rectificationSettings.bodyTemp;
        rect["tailsTemp"] = sysSettings.rectificationSettings.tailsTemp;
        rect["endTemp"] = sysSettings.rectificationSettings.endTemp;
        rect["maxCubeTemp"] = sysSettings.rectificationSettings.maxCubeTemp;
        rect["stabilizationTime"] = sysSettings.rectificationSettings.stabilizationTime;
        rect["postHeadsStabilizationTime"] = sysSettings.rectificationSettings.postHeadsStabilizationTime;
        rect["headsVolume"] = sysSettings.rectificationSettings.headsVolume;
        rect["bodyVolume"] = sysSettings.rectificationSettings.bodyVolume;
        rect["refluxRatio"] = sysSettings.rectificationSettings.refluxRatio;
        rect["refluxPeriod"] = sysSettings.rectificationSettings.refluxPeriod;
        
        // Настройки дистилляции
        JsonObject dist = doc.createNestedObject("distillation");
        dist["heatingPowerWatts"] = sysSettings.distillationSettings.heatingPowerWatts;
        dist["distillationPowerWatts"] = sysSettings.distillationSettings.distillationPowerWatts;
        dist["startCollectingTemp"] = sysSettings.distillationSettings.startCollectingTemp;
        dist["endTemp"] = sysSettings.distillationSettings.endTemp;
        dist["maxCubeTemp"] = sysSettings.distillationSettings.maxCubeTemp;
        dist["separateHeads"] = sysSettings.distillationSettings.separateHeads;
        dist["headsVolume"] = sysSettings.distillationSettings.headsVolume;
        dist["flowRate"] = sysSettings.distillationSettings.flowRate;
        dist["headsFlowRate"] = sysSettings.distillationSettings.headsFlowRate;
        
        // Отправляем ответ
        String response;
        serializeJson(doc, response);
        request->send(200, "application/json", response);
    });
    
    // Обновление настроек
    server.on("/api/settings", HTTP_POST, [](AsyncWebServerRequest *request) {
        request->send(200);
    }, NULL, [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
        DynamicJsonDocument doc(2048);
        deserializeJson(doc, data, len);
        
        // Обновляем настройки, сохраняем и отправляем ответ
        if (doc.containsKey("heater")) {
            JsonObject heater = doc["heater"];
            if (heater.containsKey("maxPowerWatts")) {
                sysSettings.heaterSettings.maxPowerWatts = heater["maxPowerWatts"];
            }
        }
        
        // Обновляем настройки насоса
        if (doc.containsKey("pump")) {
            JsonObject pump = doc["pump"];
            if (pump.containsKey("headsFlowRate")) {
                sysSettings.pumpSettings.headsFlowRate = pump["headsFlowRate"];
            }
            if (pump.containsKey("bodyFlowRate")) {
                sysSettings.pumpSettings.bodyFlowRate = pump["bodyFlowRate"];
            }
            if (pump.containsKey("tailsFlowRate")) {
                sysSettings.pumpSettings.tailsFlowRate = pump["tailsFlowRate"];
            }
        }
        
        // Обновляем настройки датчиков
        if (doc.containsKey("sensors")) {
            JsonObject sensors = doc["sensors"];
            for (JsonPair kv : sensors) {
                int sensorIndex = atoi(kv.key().c_str());
                if (sensorIndex >= 0 && sensorIndex < MAX_TEMP_SENSORS) {
                    JsonObject sensor = kv.value().as<JsonObject>();
                    
                    if (sensor.containsKey("calibration")) {
                        float calibration = sensor["calibration"];
                        calibrateTempSensor(sensorIndex, calibration);
                    }
                }
            }
        }
        
        // Обновляем настройки ректификации
        if (doc.containsKey("rectification")) {
            JsonObject rect = doc["rectification"];
            
            if (rect.containsKey("model")) {
                sysSettings.rectificationSettings.model = rect["model"];
            }
            if (rect.containsKey("heatingPowerWatts")) {
                sysSettings.rectificationSettings.heatingPowerWatts = rect["heatingPowerWatts"];
            }
            if (rect.containsKey("stabilizationPowerWatts")) {
                sysSettings.rectificationSettings.stabilizationPowerWatts = rect["stabilizationPowerWatts"];
            }
            if (rect.containsKey("bodyPowerWatts")) {
                sysSettings.rectificationSettings.bodyPowerWatts = rect["bodyPowerWatts"];
            }
            if (rect.containsKey("tailsPowerWatts")) {
                sysSettings.rectificationSettings.tailsPowerWatts = rect["tailsPowerWatts"];
            }
            if (rect.containsKey("headsTemp")) {
                sysSettings.rectificationSettings.headsTemp = rect["headsTemp"];
            }
            if (rect.containsKey("bodyTemp")) {
                sysSettings.rectificationSettings.bodyTemp = rect["bodyTemp"];
            }
            if (rect.containsKey("tailsTemp")) {
                sysSettings.rectificationSettings.tailsTemp = rect["tailsTemp"];
            }
            if (rect.containsKey("endTemp")) {
                sysSettings.rectificationSettings.endTemp = rect["endTemp"];
            }
            if (rect.containsKey("maxCubeTemp")) {
                sysSettings.rectificationSettings.maxCubeTemp = rect["maxCubeTemp"];
            }
            if (rect.containsKey("stabilizationTime")) {
                sysSettings.rectificationSettings.stabilizationTime = rect["stabilizationTime"];
            }
            if (rect.containsKey("postHeadsStabilizationTime")) {
                sysSettings.rectificationSettings.postHeadsStabilizationTime = rect["postHeadsStabilizationTime"];
            }
            if (rect.containsKey("headsVolume")) {
                sysSettings.rectificationSettings.headsVolume = rect["headsVolume"];
            }
            if (rect.containsKey("bodyVolume")) {
                sysSettings.rectificationSettings.bodyVolume = rect["bodyVolume"];
            }
            if (rect.containsKey("refluxRatio")) {
                sysSettings.rectificationSettings.refluxRatio = rect["refluxRatio"];
            }
            if (rect.containsKey("refluxPeriod")) {
                sysSettings.rectificationSettings.refluxPeriod = rect["refluxPeriod"];
            }
        }
        
        // Обновляем настройки дистилляции
        if (doc.containsKey("distillation")) {
            JsonObject dist = doc["distillation"];
            
            if (dist.containsKey("heatingPowerWatts")) {
                sysSettings.distillationSettings.heatingPowerWatts = dist["heatingPowerWatts"];
            }
            if (dist.containsKey("distillationPowerWatts")) {
                sysSettings.distillationSettings.distillationPowerWatts = dist["distillationPowerWatts"];
            }
            if (dist.containsKey("startCollectingTemp")) {
                sysSettings.distillationSettings.startCollectingTemp = dist["startCollectingTemp"];
            }
            if (dist.containsKey("endTemp")) {
                sysSettings.distillationSettings.endTemp = dist["endTemp"];
            }
            if (dist.containsKey("maxCubeTemp")) {
                sysSettings.distillationSettings.maxCubeTemp = dist["maxCubeTemp"];
            }
            if (dist.containsKey("separateHeads")) {
                sysSettings.distillationSettings.separateHeads = dist["separateHeads"];
            }
            if (dist.containsKey("headsVolume")) {
                sysSettings.distillationSettings.headsVolume = dist["headsVolume"];
            }
            if (dist.containsKey("flowRate")) {
                sysSettings.distillationSettings.flowRate = dist["flowRate"];
            }
            if (dist.containsKey("headsFlowRate")) {
                sysSettings.distillationSettings.headsFlowRate = dist["headsFlowRate"];
            }
        }
        
        // Сохраняем изменённые настройки
        saveSystemSettings();
        
        request->send(200, "application/json", "{\"status\":\"ok\"}");
    });
    
    // API для запуска процесса ректификации
    server.on("/api/rectification/start", HTTP_POST, [](AsyncWebServerRequest *request) {
        if (isDistillationRunning()) {
            request->send(409, "application/json", "{\"error\":\"Процесс дистилляции уже запущен\"}");
            return;
        }
        
        bool started = startRectification();
        if (started) {
            request->send(200, "application/json", "{\"status\":\"ok\"}");
        } else {
            request->send(500, "application/json", "{\"error\":\"Не удалось запустить ректификацию\"}");
        }
    });
    
    // API для остановки процесса ректификации
    server.on("/api/rectification/stop", HTTP_POST, [](AsyncWebServerRequest *request) {
        if (!isRectificationRunning()) {
            request->send(400, "application/json", "{\"error\":\"Процесс не запущен\"}");
            return;
        }
        
        stopRectification();
        request->send(200, "application/json", "{\"status\":\"ok\"}");
    });
    
    // API для паузы процесса ректификации
    server.on("/api/rectification/pause", HTTP_POST, [](AsyncWebServerRequest *request) {
        if (!isRectificationRunning() || isRectificationPaused()) {
            request->send(400, "application/json", "{\"error\":\"Процесс не запущен или уже на паузе\"}");
            return;
        }
        
        pauseRectification();
        request->send(200, "application/json", "{\"status\":\"ok\"}");
    });
    
    // API для возобновления процесса ректификации
    server.on("/api/rectification/resume", HTTP_POST, [](AsyncWebServerRequest *request) {
        if (!isRectificationRunning() || !isRectificationPaused()) {
            request->send(400, "application/json", "{\"error\":\"Процесс не запущен или не на паузе\"}");
            return;
        }
        
        resumeRectification();
        request->send(200, "application/json", "{\"status\":\"ok\"}");
    });
    
    // API для запуска процесса дистилляции
    server.on("/api/distillation/start", HTTP_POST, [](AsyncWebServerRequest *request) {
        if (isRectificationRunning()) {
            request->send(409, "application/json", "{\"error\":\"Процесс ректификации уже запущен\"}");
            return;
        }
        
        bool started = startDistillation();
        if (started) {
            request->send(200, "application/json", "{\"status\":\"ok\"}");
        } else {
            request->send(500, "application/json", "{\"error\":\"Не удалось запустить дистилляцию\"}");
        }
    });
    
    // API для остановки процесса дистилляции
    server.on("/api/distillation/stop", HTTP_POST, [](AsyncWebServerRequest *request) {
        if (!isDistillationRunning()) {
            request->send(400, "application/json", "{\"error\":\"Процесс не запущен\"}");
            return;
        }
        
        stopDistillation();
        request->send(200, "application/json", "{\"status\":\"ok\"}");
    });
    
    // API для паузы процесса дистилляции
    server.on("/api/distillation/pause", HTTP_POST, [](AsyncWebServerRequest *request) {
        if (!isDistillationRunning() || isDistillationPaused()) {
            request->send(400, "application/json", "{\"error\":\"Процесс не запущен или уже на паузе\"}");
            return;
        }
        
        pauseDistillation();
        request->send(200, "application/json", "{\"status\":\"ok\"}");
    });
    
    // API для возобновления процесса дистилляции
    server.on("/api/distillation/resume", HTTP_POST, [](AsyncWebServerRequest *request) {
        if (!isDistillationRunning() || !isDistillationPaused()) {
            request->send(400, "application/json", "{\"error\":\"Процесс не запущен или не на паузе\"}");
            return;
        }
        
        resumeDistillation();
        request->send(200, "application/json", "{\"status\":\"ok\"}");
    });
    
    // API для ручного управления нагревателем
    server.on("/api/heater/set", HTTP_POST, [](AsyncWebServerRequest *request) {
        if (isRectificationRunning() || isDistillationRunning()) {
            request->send(409, "application/json", "{\"error\":\"Процесс уже запущен, ручное управление недоступно\"}");
            return;
        }
        
        int powerWatts = 0;
        if (request->hasParam("power", true)) {
            powerWatts = request->getParam("power", true)->value().toInt();
        }
        
        setHeaterPower(powerWatts);
        request->send(200, "application/json", "{\"status\":\"ok\", \"power\":" + String(powerWatts) + "}");
    });
    
    // API для ручного управления насосом
    server.on("/api/pump/set", HTTP_POST, [](AsyncWebServerRequest *request) {
        if (isRectificationRunning() || isDistillationRunning()) {
            request->send(409, "application/json", "{\"error\":\"Процесс уже запущен, ручное управление недоступно\"}");
            return;
        }
        
        if (!request->hasParam("flowRate", true)) {
            request->send(400, "application/json", "{\"error\":\"Параметр flowRate обязателен\"}");
            return;
        }
        
        float flowRate = request->getParam("flowRate", true)->value().toFloat();
        
        if (flowRate > 0) {
            pumpStart(flowRate);
        } else {
            pumpStop();
        }
        
        request->send(200, "application/json", "{\"status\":\"ok\", \"flowRate\":" + String(flowRate) + "}");
    });
    
    // API для ручного управления клапаном
    server.on("/api/valve/set", HTTP_POST, [](AsyncWebServerRequest *request) {
        if (isRectificationRunning() || isDistillationRunning()) {
            request->send(409, "application/json", "{\"error\":\"Процесс уже запущен, ручное управление недоступно\"}");
            return;
        }
        
        if (!request->hasParam("open", true)) {
            request->send(400, "application/json", "{\"error\":\"Параметр open обязателен\"}");
            return;
        }
        
        bool open = (request->getParam("open", true)->value() == "true");
        
        if (open) {
            valveOpen();
        } else {
            valveClose();
        }
        
        request->send(200, "application/json", "{\"status\":\"ok\", \"open\":" + String(open ? "true" : "false") + "}");
    });
    
    // API для калибровки датчиков
    server.on("/api/sensor/calibrate", HTTP_POST, [](AsyncWebServerRequest *request) {
        if (isRectificationRunning() || isDistillationRunning()) {
            request->send(409, "application/json", "{\"error\":\"Процесс уже запущен, калибровка недоступна\"}");
            return;
        }
        
        if (!request->hasParam("sensor", true) || !request->hasParam("offset", true)) {
            request->send(400, "application/json", "{\"error\":\"Параметры sensor и offset обязательны\"}");
            return;
        }
        
        int sensorIndex = request->getParam("sensor", true)->value().toInt();
        float offset = request->getParam("offset", true)->value().toFloat();
        
        if (sensorIndex < 0 || sensorIndex >= MAX_TEMP_SENSORS) {
            request->send(400, "application/json", "{\"error\":\"Некорректный индекс датчика\"}");
            return;
        }
        
        calibrateTempSensor(sensorIndex, offset);
        
        request->send(200, "application/json", "{\"status\":\"ok\", \"sensor\":" + String(sensorIndex) + ", \"offset\":" + String(offset) + "}");
    });
    
    // API для сканирования датчиков
    server.on("/api/sensors/scan", HTTP_POST, [](AsyncWebServerRequest *request) {
        if (isRectificationRunning() || isDistillationRunning()) {
            request->send(409, "application/json", "{\"error\":\"Процесс уже запущен, сканирование недоступно\"}");
            return;
        }
        
        bool success = scanForTempSensors();
        int count = getConnectedSensorsCount();
        
        if (success) {
            request->send(200, "application/json", "{\"status\":\"ok\", \"count\":" + String(count) + "}");
        } else {
            request->send(500, "application/json", "{\"error\":\"Не удалось найти датчики\"}");
        }
    });
    
    // API для сброса настроек к значениям по умолчанию
    server.on("/api/settings/reset", HTTP_POST, [](AsyncWebServerRequest *request) {
        if (isRectificationRunning() || isDistillationRunning()) {
            request->send(409, "application/json", "{\"error\":\"Процесс уже запущен, сброс настроек недоступен\"}");
            return;
        }
        
        resetSystemSettings();
        saveSystemSettings();
        
        request->send(200, "application/json", "{\"status\":\"ok\"}");
    });
}

// Настройка маршрутов для статических файлов
void setupStaticRoutes() {
    // Обработка корневого маршрута
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(LittleFS, "/index.html", "text/html");
    });
    
    // Маршруты для статических файлов
    server.on("/styles.css", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(LittleFS, "/styles.css", "text/css");
    });
    
    server.on("/main.js", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(LittleFS, "/main.js", "application/javascript");
    });
    
    server.on("/manifest.json", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(LittleFS, "/manifest.json", "application/json");
    });
    
    server.on("/favicon.ico", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(LittleFS, "/favicon.ico", "image/x-icon");
    });
    
    // Маршрут для иконок
    server.on("/icons/icon-192.png", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(LittleFS, "/icons/icon-192.png", "image/png");
    });
    
    server.on("/icons/icon-512.png", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(LittleFS, "/icons/icon-512.png", "image/png");
    });
}

// Обработчик событий WebSocket
void onWebSocketEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, 
                      AwsEventType type, void *arg, uint8_t *data, size_t len) {
    switch (type) {
        case WS_EVT_CONNECT:
            Serial.printf("WebSocket клиент #%u подключен от %s\n", client->id(), client->remoteIP().toString().c_str());
            webSocketActive = true;
            break;
        case WS_EVT_DISCONNECT:
            Serial.printf("WebSocket клиент #%u отключен\n", client->id());
            webSocketActive = (ws.count() > 0);
            break;
        case WS_EVT_DATA:
            // Обработка входящих данных WebSocket
            handleWebSocketMessage(arg, data, len);
            break;
        case WS_EVT_ERROR:
            Serial.printf("WebSocket ошибка #%u: %u\n", client->id(), *((uint16_t*)arg));
            break;
    }
}

// Обработка сообщений WebSocket
void handleWebSocketMessage(void *arg, uint8_t *data, size_t len) {
    AwsFrameInfo *info = (AwsFrameInfo*)arg;
    if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
        // Обеспечиваем завершение нулем для строки
        data[len] = 0;
        
        // Разбираем JSON
        DynamicJsonDocument doc(1024);
        DeserializationError error = deserializeJson(doc, (char*)data);
        
        if (error) {
            Serial.println("Ошибка разбора JSON сообщения WebSocket");
            return;
        }
        
        // Обрабатываем команды
        if (doc.containsKey("cmd")) {
            String command = doc["cmd"];
            
            if (command == "getStatus") {
                // Принудительно обновляем статус WebSocket
                lastWsUpdate = 0;
                updateWebSocket();
            }
            // Можно добавить другие команды, если потребуется
        }
    }
}