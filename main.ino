// ============================================================
// RTL8720DN Deauther v3.0 - Refactored Edition
// ============================================================
// Dual-band WiFi security testing tool with:
// - Deauth/Disassoc attacks (2.4GHz + 5GHz)
// - KARMA Attack (auto-respond to probe requests)
// - Evil Twin with Captive Portal
// - Modern Web UI
// ============================================================

#include "config.h"
#include "flash_storage.h"
#include "led_utils.h"
#include "attack.h"
#include "evil_twin.h"
#include "web_server.h"
#include "dns_server.h"

// ============== GLOBAL VARIABLE DEFINITIONS ==============
volatile DeviceState currentState = STATE_IDLE;
volatile AttackMode currentAttackMode = ATTACK_COMBO;
bool ledEnabled = true;

SemaphoreHandle_t channelMutex = NULL;
volatile uint8_t currentChannel = 0;

const char *ssid_ap = "BW16";
const char *pass_ap = "deauther";
int current_channel = 6;

std::vector<WiFiScanResult> scan_results;
std::vector<int> attack_targets_24;
std::vector<int> attack_targets_5;


// Evil Twin
String evilTwinSSID = "";
int evilTwinTargetIdx = -1;
String lastCapturedPassword = "";
std::vector<String> passwordHistory;
bool evilTwinActive = false;
uint8_t deauth_bssid[6];

// Statistics
volatile uint32_t totalFramesSent24 = 0;
volatile uint32_t totalFramesSent5 = 0;

// ============== SETUP ==============
void setup() {
  // LED pins
  pinMode(LED_R, OUTPUT);
  pinMode(LED_G, OUTPUT);
  pinMode(LED_B, OUTPUT);
  
  // Serial debug
  DEBUG_SER_INIT();
  DEBUG_SER_PRINT("\n");
  DEBUG_SER_PRINT("=========================================\n");
  DEBUG_SER_PRINT("  RTL8720DN Deauther v3.0\n");
  DEBUG_SER_PRINT("  Dual-band WiFi Security Tool\n");
  DEBUG_SER_PRINT("=========================================\n");
  
  // Create mutex for thread-safe channel switching
  channelMutex = xSemaphoreCreateMutex();
  if (channelMutex == NULL) {
    DEBUG_SER_PRINT("ERROR: Failed to create mutex!\n");
  }
  
  // Disable power save for maximum TX performance
  wifi_disable_powersave();
  
  // Start Access Point
  char channelStr[4];
  snprintf(channelStr, sizeof(channelStr), "%d", current_channel);
  WiFi.apbegin((char*)ssid_ap, (char*)pass_ap, channelStr);
  DEBUG_SER_PRINT("AP started: " + String(ssid_ap) + "\n");
  
  // Initial network scan
  scanNetworks();
  
  // Load saved passwords
  loadPasswordsFromFlash();
  
  currentState = STATE_IDLE;
  digitalWrite(LED_R, HIGH);
  
  // Create FreeRTOS tasks
  xTaskCreate(LEDUpdateTask, "LED", 1024, NULL, 0, NULL);
  xTaskCreate(WebServerTask, "Web", 4096, NULL, 1, NULL);
  // xTaskCreate(DNSServerTask, "DNS", 2048, NULL, 2, NULL);  // DISABLED for testing
  xTaskCreate(AttackTask24, "Atk24", 3072, NULL, 3, NULL);
  xTaskCreate(AttackTask5, "Atk5", 3072, NULL, 3, NULL);
  xTaskCreate(EvilTwinDeauthTask, "ETwin", 2048, NULL, 3, NULL);
  
  DEBUG_SER_PRINT("All tasks started. Ready!\n");
  DEBUG_SER_PRINT("Connect to WiFi '" + String(ssid_ap) + "' and open browser.\n");
}

// ============== LOOP ==============
void loop() {
  // All work is done in FreeRTOS tasks
  vTaskDelay(pdMS_TO_TICKS(100));
}
