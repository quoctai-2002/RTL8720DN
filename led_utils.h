#ifndef LED_UTILS_H
#define LED_UTILS_H

#include "config.h"

// Cập nhật trạng thái LED dựa trên device state
void updateLEDs();

// FreeRTOS task cho LED
void LEDUpdateTask(void *pvParameters);

#endif // LED_UTILS_H
