/**
 * @file display.cpp
 * @brief Реализация функций для работы с дисплеем
 */

#include "display.h"
#include "config.h"
#include "temp_sensors.h"
#include "heater.h"
#include "pump.h"
#include "valve.h"
#include "rectification.h"
#include "distillation.h"
#include "safety.h"
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Arduino.h>

// Создаем объект дисплея
Adafruit_SSD1306 display(DISPLAY_WIDTH, DISPLAY_HEIGHT, &Wire, -1);

// Текущая страница дисплея
DisplayPage currentDisplayPage = DISPLAY_PAGE_MAIN;

// Время последнего обновления страницы
unsigned long lastPageChangeTime = 0;

// Счетчик для авто-переключения страниц
int autoPageChangeCounter = 0;

// Иконки для отображения статуса
static const unsigned char PROGMEM icon_heater[] = {
  0x00, 0x00, 0x0F, 0xF0, 0x1F, 0xF8, 0x1F, 0xF8, 0x1F, 0xF8, 0x1F, 0xF8,
  0x1F, 0xF8, 0x1F, 0xF8, 0x1F, 0xF8, 0x1F, 0xF8, 0x1F, 0xF8, 0x1F, 0xF8,
  0x0F, 0xF0, 0x07, 0xE0, 0x03, 0xC0, 0x01, 0x80
};

static const unsigned char PROGMEM icon_pump[] = {
  0x00, 0x00, 0x03, 0xC0, 0x0F, 0xF0, 0x1F, 0xF8, 0x1F, 0xF8, 0x1F, 0xF8,
  0x1F, 0xF8, 0x1F, 0xF8, 0x0F, 0xF0, 0x07, 0xE0, 0x03, 0xC0, 0x03, 0xC0,
  0x03, 0xC0, 0x03, 0xC0, 0x03, 0xC0, 0x03, 0xC0
};

static const unsigned char PROGMEM icon_valve[] = {
  0x00, 0x00, 0x03, 0xC0, 0x03, 0xC0, 0x03, 0xC0, 0x03, 0xC0, 0x03, 0xC0,
  0x03, 0xC0, 0x3F, 0xFC, 0x3F, 0xFC, 0x3F, 0xFC, 0x3F, 0xFC, 0x3F, 0xFC,
  0x03, 0xC0, 0x03, 0xC0, 0x03, 0xC0, 0x00, 0x00
};

// Инициализация дисплея
bool initDisplay() {
    Serial.println("Инициализация дисплея...");

    // Инициализация I2C
    Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);

    // Инициализация дисплея
    if (!display.begin(SSD1306_SWITCHCAPVCC, DISPLAY_ADDRESS)) {
        Serial.println("Не удалось инициализировать дисплей SSD1306");
        return false;
    }

    // Очистка дисплея
    display.clearDisplay();
    display.setTextColor(WHITE);

    // Отображение заставки
    displaySplashScreen();

    Serial.println("Дисплей успешно инициализирован");
    return true;
}

// Отображение заставки
void displaySplashScreen() {
    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(20, 10);
    display.println("САМОГОННЫЙ");
    display.setCursor(25, 25);
    display.println("КОНТРОЛЛЕР");
    display.setCursor(25, 40);
    display.print("v");
    display.println(FIRMWARE_VERSION);
    display.display();
    delay(2000);  // Показываем заставку 2 секунды
}

// Переключение на следующую страницу
void switchToNextPage() {
    currentDisplayPage = (DisplayPage)((int)currentDisplayPage + 1);
    
    // Проверка границ и переход на первую страницу, если достигли последней
    if (currentDisplayPage >= DISPLAY_PAGE_COUNT) {
        currentDisplayPage = DISPLAY_PAGE_MAIN;
    }
    
    lastPageChangeTime = millis();
    updateDisplay();
}

// Переключение на предыдущую страницу
void switchToPreviousPage() {
    if (currentDisplayPage == DISPLAY_PAGE_MAIN) {
        currentDisplayPage = (DisplayPage)(DISPLAY_PAGE_COUNT - 1);
    } else {
        currentDisplayPage = (DisplayPage)((int)currentDisplayPage - 1);
    }
    
    lastPageChangeTime = millis();
    updateDisplay();
}

// Установка конкретной страницы
void setDisplayPage(DisplayPage page) {
    if (page < DISPLAY_PAGE_COUNT) {
        currentDisplayPage = page;
        lastPageChangeTime = millis();
        updateDisplay();
    }
}

// Отображение ошибки
void displayShowError(const String &errorMessage) {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(WHITE);
    
    display.setCursor(0, 0);
    display.println("ОШИБКА:");
    
    display.setCursor(0, 16);
    // Ограничиваем длину сообщения, чтобы оно поместилось на экране
    String line1 = errorMessage.length() > 20 ? errorMessage.substring(0, 20) : errorMessage;
    display.println(line1);
    
    if (errorMessage.length() > 20) {
        display.setCursor(0, 26);
        String line2 = errorMessage.length() > 40 ? errorMessage.substring(20, 40) : errorMessage.substring(20);
        display.println(line2);
    }
    
    display.display();
}

// Обновление содержимого дисплея
void updateDisplay() {
    // Проверяем статус безопасности
    SafetyStatus safetyStatus = getSafetyStatus();
    if (!safetyStatus.isSystemSafe) {
        displayShowError(safetyStatus.errorDescription);
        return; // Выходим, чтобы не перезаписать сообщение об ошибке
    }
    
    // Обработка различных страниц дисплея
    switch (currentDisplayPage) {
        case DISPLAY_PAGE_MAIN:
            updateMainPage();
            break;
        case DISPLAY_PAGE_TEMPERATURES:
            updateTemperaturesPage();
            break;
        case DISPLAY_PAGE_PROCESS_INFO:
            updateProcessInfoPage();
            break;
        case DISPLAY_PAGE_SYSTEM_INFO:
            updateSystemInfoPage();
            break;
        case DISPLAY_PAGE_CONTROL_STATUS:
            updateControlStatusPage();
            break;
        default:
            updateMainPage();
            break;
    }
}

// Обновление главной страницы
void updateMainPage() {
    display.clearDisplay();
    display.setTextSize(1);
    
    // Заголовок
    display.setCursor(0, 0);
    display.println("Главная");
    display.drawLine(0, 9, 128, 9, WHITE);
    
    // Температуры
    display.setCursor(0, 12);
    display.print("Куб: ");
    display.print(getTemperature(TEMP_CUBE), 1);
    display.println(" C");
    
    display.setCursor(0, 22);
    display.print("Колонна: ");
    display.print(getTemperature(TEMP_COLUMN), 1);
    display.println(" C");
    
    display.setCursor(0, 32);
    display.print("Отбор: ");
    display.print(getTemperature(TEMP_REFLUX), 1);
    display.println(" C");
    
    // Статус процесса
    display.setCursor(0, 45);
    display.print("Статус: ");
    
    if (isRectificationRunning()) {
        if (isRectificationPaused()) {
            display.println("Ректиф. пауза");
        } else {
            display.print("Ректификация (");
            display.print(getRectificationPhaseName());
            display.println(")");
        }
    } else if (isDistillationRunning()) {
        if (isDistillationPaused()) {
            display.println("Дистил. пауза");
        } else {
            display.print("Дистилляция (");
            display.print(getDistillationPhaseName());
            display.println(")");
        }
    } else {
        display.println("Ожидание");
    }
    
    // Мощность нагревателя
    display.setCursor(0, 55);
    display.print("Нагрев: ");
    display.print(getHeaterPowerWatts());
    display.print("Вт (");
    display.print(getHeaterPowerPercent());
    display.println("%)");
    
    display.display();
}

// Обновление страницы температур
void updateTemperaturesPage() {
    display.clearDisplay();
    display.setTextSize(1);
    
    // Заголовок
    display.setCursor(0, 0);
    display.println("Температуры");
    display.drawLine(0, 9, 128, 9, WHITE);
    
    // Проверяем подключение датчиков и выводим информацию
    for (int i = 0; i < MAX_TEMP_SENSORS; i++) {
        display.setCursor(0, 12 + i * 10);
        display.print(getTempSensorName(i));
        display.print(": ");
        
        if (isSensorConnected(i)) {
            display.print(getTemperature(i), 1);
            display.println(" C");
        } else {
            display.println("Нет");
        }
    }
    
    display.display();
}

// Обновление страницы информации о процессе
void updateProcessInfoPage() {
    display.clearDisplay();
    display.setTextSize(1);
    
    // Заголовок
    display.setCursor(0, 0);
    
    if (isRectificationRunning()) {
        display.println("Ректификация");
        display.drawLine(0, 9, 128, 9, WHITE);
        
        // Фаза процесса
        display.setCursor(0, 12);
        display.print("Фаза: ");
        display.println(getRectificationPhaseName());
        
        // Время работы
        display.setCursor(0, 22);
        display.print("Время: ");
        unsigned long uptimeSec = getRectificationUptime();
        int hours = uptimeSec / 3600;
        int minutes = (uptimeSec % 3600) / 60;
        int seconds = uptimeSec % 60;
        display.print(hours);
        display.print(":");
        if (minutes < 10) display.print("0");
        display.print(minutes);
        display.print(":");
        if (seconds < 10) display.print("0");
        display.println(seconds);
        
        // Собранный продукт
        display.setCursor(0, 32);
        display.print("Головы: ");
        display.print(getRectificationHeadsVolume());
        display.println(" мл");
        
        display.setCursor(0, 42);
        display.print("Тело: ");
        display.print(getRectificationBodyVolume());
        display.println(" мл");
        
        display.setCursor(0, 52);
        display.print("Хвосты: ");
        display.print(getRectificationTailsVolume());
        display.println(" мл");
    }
    else if (isDistillationRunning()) {
        display.println("Дистилляция");
        display.drawLine(0, 9, 128, 9, WHITE);
        
        // Фаза процесса
        display.setCursor(0, 12);
        display.print("Фаза: ");
        display.println(getDistillationPhaseName());
        
        // Время работы
        display.setCursor(0, 22);
        display.print("Время: ");
        unsigned long uptimeSec = getDistillationUptime();
        int hours = uptimeSec / 3600;
        int minutes = (uptimeSec % 3600) / 60;
        int seconds = uptimeSec % 60;
        display.print(hours);
        display.print(":");
        if (minutes < 10) display.print("0");
        display.print(minutes);
        display.print(":");
        if (seconds < 10) display.print("0");
        display.println(seconds);
        
        // Собранный продукт
        display.setCursor(0, 32);
        if (isDistillationHeadsMode()) {
            display.print("Головы: ");
            display.print(getDistillationHeadsVolume());
            display.println(" мл");
        }
        
        display.setCursor(0, 42);
        display.print("Всего собрано: ");
        display.print(getDistillationProductVolume());
        display.println(" мл");
    }
    else {
        display.println("Нет активных процессов");
    }
    
    display.display();
}

// Обновление страницы системной информации
void updateSystemInfoPage() {
    display.clearDisplay();
    display.setTextSize(1);
    
    // Заголовок
    display.setCursor(0, 0);
    display.println("Системная информация");
    display.drawLine(0, 9, 128, 9, WHITE);
    
    // Версия прошивки
    display.setCursor(0, 12);
    display.print("Версия: ");
    display.println(FIRMWARE_VERSION);
    
    // Время работы
    display.setCursor(0, 22);
    display.print("Время работы: ");
    unsigned long uptimeSec = millis() / 1000;
    int hours = uptimeSec / 3600;
    int minutes = (uptimeSec % 3600) / 60;
    display.print(hours);
    display.print("ч ");
    display.print(minutes);
    display.println("м");
    
    // Количество датчиков
    display.setCursor(0, 32);
    display.print("Датчиков: ");
    display.println(getConnectedSensorsCount());
    
    // Статус безопасности
    display.setCursor(0, 42);
    display.print("Безопасность: ");
    if (isSafetyOK()) {
        display.println("OK");
    } else {
        SafetyStatus status = getSafetyStatus();
        display.println("ОШИБКА");
        
        display.setCursor(0, 52);
        // Ограничиваем длину сообщения
        String errorMsg = status.errorDescription;
        if (errorMsg.length() > 21) {
            errorMsg = errorMsg.substring(0, 21);
        }
        display.println(errorMsg);
    }
    
    display.display();
}

// Обновление страницы статуса устройств
void updateControlStatusPage() {
    display.clearDisplay();
    display.setTextSize(1);
    
    // Заголовок
    display.setCursor(0, 0);
    display.println("Статус устройств");
    display.drawLine(0, 9, 128, 9, WHITE);
    
    // Статус нагревателя
    display.setCursor(0, 12);
    display.print("Нагреватель: ");
    if (getHeaterPowerWatts() > 0) {
        display.println("ВКЛ");
        display.drawBitmap(100, 12, icon_heater, 16, 16, WHITE);
    } else {
        display.println("ВЫКЛ");
    }
    
    display.setCursor(0, 22);
    display.print("Мощность: ");
    display.print(getHeaterPowerWatts());
    display.print("Вт (");
    display.print(getHeaterPowerPercent());
    display.println("%)");
    
    // Статус насоса
    display.setCursor(0, 32);
    display.print("Насос: ");
    if (isPumpRunning()) {
        display.println("ВКЛ");
        display.drawBitmap(100, 32, icon_pump, 16, 16, WHITE);
        
        display.setCursor(0, 42);
        display.print("Скорость: ");
        display.print(getPumpFlowRate());
        display.println(" мл/мин");
    } else {
        display.println("ВЫКЛ");
    }
    
    // Статус клапана
    display.setCursor(0, 52);
    display.print("Клапан: ");
    if (isValveOpen()) {
        display.println("ОТКРЫТ");
        display.drawBitmap(100, 52, icon_valve, 16, 16, WHITE);
    } else {
        display.println("ЗАКРЫТ");
    }
    
    display.display();
}

// Обработка автоматического переключения страниц
void handleAutoPageChange() {
    unsigned long currentTime = millis();
    if (currentTime - lastPageChangeTime > 10000) { // Каждые 10 секунд
        switchToNextPage();
    }
}