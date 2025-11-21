/**
 * @file config.h
 * @brief Основной файл конфигурации системы
 */

#ifndef CONFIG_H
#define CONFIG_H

// Версия прошивки
#define FIRMWARE_VERSION "1.0.0"

// Пины микроконтроллера
#define PIN_TEMP_SENSORS 4      // Пин шины 1-Wire для датчиков температуры
#define PIN_HEATER 16           // Пин управления нагревателем (ШИМ)
#define PIN_VALVE 17            // Пин управления клапаном
#define PIN_PUMP 5              // Пин управления насосом (ШИМ)
#define PIN_I2C_SDA 21          // Пин SDA для I2C (дисплей)
#define PIN_I2C_SCL 22          // Пин SCL для I2C (дисплей)
#define PIN_EMERGENCY_STOP 27   // Пин кнопки аварийной остановки

// Параметры дисплея
#define DISPLAY_WIDTH 128       // Ширина дисплея в пикселях
#define DISPLAY_HEIGHT 64       // Высота дисплея в пикселях
#define DISPLAY_ADDRESS 0x3C    // Адрес I2C дисплея
#define DISPLAY_REFRESH_MS 500  // Интервал обновления дисплея в миллисекундах

// Параметры ШИМ для нагревателя
#define HEATER_PWM_FREQ 500     // Частота ШИМ для нагревателя (Гц)
#define HEATER_PWM_CHANNEL 0    // Канал ШИМ для нагревателя
#define HEATER_PWM_RESOLUTION 8 // Разрешение ШИМ для нагревателя (биты)

// Параметры ШИМ для насоса
#define PUMP_PWM_FREQ 1000      // Частота ШИМ для насоса (Гц)
#define PUMP_PWM_CHANNEL 1      // Канал ШИМ для насоса
#define PUMP_PWM_RESOLUTION 8   // Разрешение ШИМ для насоса (биты)

// Параметры датчиков температуры
#define TEMP_SENSOR_RESOLUTION 12   // Разрешение датчика температуры (9-12 бит)
#define TEMP_UPDATE_INTERVAL 1000   // Интервал обновления температуры в мс

// Настройки Wi-Fi
#define WIFI_AP_SSID "Distiller"    // Имя точки доступа Wi-Fi
#define WIFI_AP_PASSWORD "password" // Пароль точки доступа Wi-Fi

// Настройки безопасности
#define SAFETY_MAX_RUNTIME_HOURS_DEFAULT 12     // Макс. время работы по умолчанию (часы)
#define SAFETY_MAX_CUBE_TEMP_DEFAULT 105.0f     // Макс. температура куба по умолчанию
#define SAFETY_MAX_TEMP_RISE_RATE_DEFAULT 5.0f  // Макс. скорость изменения температуры (градусов в минуту)
#define SAFETY_MIN_WATER_OUT_TEMP_DEFAULT 5.0f  // Мин. температура выхода воды
#define SAFETY_MAX_WATER_OUT_TEMP_DEFAULT 50.0f // Макс. температура выхода воды
#define SAFETY_CHECK_INTERVAL 1000              // Интервал проверки безопасности (мс)

// Другие константы
#define SERIAL_BAUD_RATE 115200    // Скорость последовательного порта
#define MAX_STRING_LENGTH 64       // Максимальная длина строк

#endif // CONFIG_H