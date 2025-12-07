#ifndef ATTACK_H
#define ATTACK_H

#include "config.h"

// Reason code arrays definitions
extern const uint16_t REASONS_STANDARD[8];
extern const uint16_t REASONS_SECURITY[8];
extern const uint16_t REASONS_IOS[9];
extern const uint16_t REASONS_ANDROID[8];
extern const uint16_t REASONS_ALL[22];

// Thread-safe channel switching
void switchChannelSafe(uint8_t ch);

// Gửi burst attack frames
void sendAttackBurst(uint8_t* bssid, int frameCount);

// Sắp xếp targets theo channel
void sortTargetsByChannel(std::vector<int>& targets);

// Scan networks
rtw_result_t scanResultHandler(rtw_scan_handler_result_t *scan_result);
int scanNetworks();

// FreeRTOS Tasks
void AttackTask24(void *pvParameters);
void AttackTask5(void *pvParameters);
void BeaconTask(void *pvParameters);

#endif // ATTACK_H
