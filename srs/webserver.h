#ifndef WEBSERVER_H
#define WEBSERVER_H

#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include "config.h"

// Инициализация веб-сервера
void initWebServer();

// Настройка WiFi
bool setupWiFi();

// Настройка точки доступа
void setupAccessPoint();

// Настройка маршрутов веб-сервера
void setupWebRoutes();

// Настройка WebSocket сервера
void setupWebSocket();

// Обработчик WebSocket событий
void onWebSocketEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, 
                      AwsEventType type, void *arg, uint8_t *data, size_t len);

// Отправка данных о температурах через WebSocket
void sendTemperaturesWebSocket();

// Отправка данных о статусе системы через WebSocket
void sendStatusWebSocket();

// Отправка уведомления клиентам
void sendNotificationToClients(NotificationType type, const String& message);

// Обработка запроса на начало процесса
void handleStartProcess(AsyncWebServerRequest *request);

// Обработка запроса на остановку процесса
void handleStopProcess(AsyncWebServerRequest *request);

// Обработка запроса на изменение мощности
void handlePowerChange(AsyncWebServerRequest *request);

// Обработка запроса на настройку параметров ректификации
void handleRectificationSettings(AsyncWebServerRequest *request);

// Обработка запроса на настройку параметров дистилляции
void handleDistillationSettings(AsyncWebServerRequest *request);

// Обработка запроса на настройку параметров системы
void handleSystemSettings(AsyncWebServerRequest *request);

// Обработка загрузки файлов
void handleFileUpload(AsyncWebServerRequest *request, const String& filename, 
                      size_t index, uint8_t *data, size_t len, bool final);

// Настройка файловой системы SPIFFS
bool setupSPIFFS();

#endif // WEBSERVER_H