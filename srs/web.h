/**
 * @file web.h
 * @brief Веб-интерфейс для управления системой
 * 
 * Этот модуль отвечает за веб-сервер, API и WebSocket коммуникацию.
 */

#ifndef WEB_H
#define WEB_H

#include <Arduino.h>
#include <AsyncWebSocket.h>

/**
 * @brief Инициализация модуля веб-сервера
 */
void initWebServer();

/**
 * @brief Обновление состояния WebSocket соединения
 * 
 * Периодически отправляет текущие данные о состоянии системы подключенным клиентам.
 */
void updateWebSocket();

/**
 * @brief Настройка маршрутов API
 * 
 * Настраивает все конечные точки API для взаимодействия с системой.
 */
void setupApiRoutes();

/**
 * @brief Настройка маршрутов для статических файлов
 * 
 * Настраивает маршруты для обслуживания HTML, CSS, JavaScript и других статических файлов.
 */
void setupStaticRoutes();

/**
 * @brief Обработчик событий WebSocket
 * 
 * @param server Указатель на объект AsyncWebSocket
 * @param client Указатель на клиента
 * @param type Тип события
 * @param arg Аргументы события
 * @param data Данные сообщения
 * @param len Длина данных
 */
void onWebSocketEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, 
                      AwsEventType type, void *arg, uint8_t *data, size_t len);

/**
 * @brief Обработка сообщений WebSocket
 * 
 * @param arg Аргументы события
 * @param data Данные сообщения
 * @param len Длина данных
 */
void handleWebSocketMessage(void *arg, uint8_t *data, size_t len);

#endif // WEB_H