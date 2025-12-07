#include "led_utils.h"

void updateLEDs() {
  if (!ledEnabled) {
    digitalWrite(LED_R, LOW);
    digitalWrite(LED_G, LOW);
    digitalWrite(LED_B, LOW);
    return;
  }
  
  static unsigned long lastToggleTime = 0;
  static bool ledState = false;
  unsigned long now = millis();
  
  digitalWrite(LED_R, LOW);
  digitalWrite(LED_G, LOW);
  digitalWrite(LED_B, LOW);
  
  switch (currentState) {
    case STATE_IDLE:
      if (now - lastToggleTime >= BLINK_INTERVAL_IDLE) {
        ledState = !ledState;
        lastToggleTime = now;
      }
      digitalWrite(LED_R, ledState ? HIGH : LOW);
      break;
      
    case STATE_SCANNING:
      if (now - lastToggleTime >= BLINK_INTERVAL_SCANNING) {
        ledState = !ledState;
        lastToggleTime = now;
      }
      digitalWrite(LED_R, ledState ? HIGH : LOW);
      break;
      
    case STATE_ATTACK:
      if (now - lastToggleTime >= BLINK_INTERVAL_ATTACK) {
        ledState = !ledState;
        lastToggleTime = now;
      }
      digitalWrite(LED_G, ledState ? HIGH : LOW);
      break;
      
    case STATE_EVIL_TWIN:
      // Evil Twin: Blue blink (same speed as Deauth)
      if (now - lastToggleTime >= BLINK_INTERVAL_ATTACK) {
        ledState = !ledState;
        lastToggleTime = now;
      }
      digitalWrite(LED_B, ledState ? HIGH : LOW);
      break;
  }
}

void LEDUpdateTask(void *pvParameters) {
  (void) pvParameters;
  while (1) {
    updateLEDs();
    vTaskDelay(pdMS_TO_TICKS(50));
  }
}
