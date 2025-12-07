#ifndef CONFIG_H
#define CONFIG_H

// ============== INCLUDES ==============
#include <Arduino.h>
#include <WiFi.h>
#include <WiFiServer.h>
#include <WiFiClient.h>
#include <vector>
#include <map>
#include <stdlib.h>
#include "wifi_conf.h"
#include "wifi_cust_tx.h"
#include "wifi_util.h"
#include "wifi_structures.h"
#include "debug.h"

#include <FreeRTOS.h>
#include <task.h>
#include <semphr.h>


// ============== WLAN INTERFACE NAMES ==============
#ifndef WLAN0_NAME
  #define WLAN0_NAME "wlan0"
#endif
#ifndef WLAN1_NAME
  #define WLAN1_NAME "wlan1"
#endif

// ============== LED PINS ==============
#ifndef LED_R
  #define LED_R 2
#endif
#ifndef LED_G
  #define LED_G 3
#endif
#ifndef LED_B
  #define LED_B 4
#endif

// ============== PERFORMANCE CONFIG ==============
#define DEBUG_CHANNEL_SWITCH 0

// Frame counts per target
#define OPTIMIZED_FRAMES_24 10      // 2.4GHz frames per burst
#define OPTIMIZED_FRAMES_5  20      // 5GHz frames per burst
#define OPTIMIZED_BEACON    300     // Beacon frames per target

// Timing (milliseconds)
#define OPTIMIZED_INTERVAL_24  20   // 2.4GHz attack interval
#define OPTIMIZED_INTERVAL_5   50   // 5GHz attack interval

// Microsecond delays
#define FRAME_DELAY_US         100  // Delay between frames
#define CHANNEL_SWITCH_DELAY_US 200 // Delay after channel switch

// Dual-band time slicing
#define BAND_TIMESLICE_MS      80   // Time per band

// ============== FLASH STORAGE CONFIG ==============
#define MAX_STORED_PASSWORDS  20  // Legacy compatibility
#define MAX_SCAN_RESULTS      15  // Limit WiFi networks displayed

// ============== BLINK INTERVALS ==============
#define BLINK_INTERVAL_IDLE     1500
#define BLINK_INTERVAL_ATTACK   500
#define BLINK_INTERVAL_SCANNING 100
#define BLINK_INTERVAL_BEACON   500



// ============== STATE DEFINITIONS ==============
enum DeviceState {
  STATE_IDLE,
  STATE_SCANNING,
  STATE_ATTACK,
  STATE_EVIL_TWIN
};

enum AttackMode {
  ATTACK_DEAUTH_ONLY,
  ATTACK_DISASSOC_ONLY,
  ATTACK_COMBO,
  ATTACK_AGGRESSIVE
};

// ============== DATA STRUCTURES ==============
struct WiFiScanResult {
  String ssid;
  String bssid_str;
  uint8_t bssid[6];
  short rssi;
  uint8_t channel;
};

// ============== REASON CODES (IEEE 802.11) ==============
// Standard deauth reasons
const uint16_t REASON_UNSPECIFIED = 1;
const uint16_t REASON_PREV_AUTH_INVALID = 2;
const uint16_t REASON_LEAVING_BSS = 3;
const uint16_t REASON_INACTIVITY = 4;
const uint16_t REASON_AP_BUSY = 5;
const uint16_t REASON_CLASS2_NONAUTH = 6;
const uint16_t REASON_CLASS3_NONASSOC = 7;
const uint16_t REASON_LEAVING_ESS = 8;

// Authentication related
const uint16_t REASON_STA_NOT_ASSOC = 9;
const uint16_t REASON_PWR_CAP_INVALID = 10;
const uint16_t REASON_SUPP_CHAN_INVALID = 11;

// Security related
const uint16_t REASON_INVALID_IE = 13;
const uint16_t REASON_MIC_FAILURE = 14;
const uint16_t REASON_4WAY_TIMEOUT = 15;
const uint16_t REASON_GKEY_TIMEOUT = 16;
const uint16_t REASON_IE_MISMATCH = 17;
const uint16_t REASON_INVALID_GRP_CIPHER = 18;
const uint16_t REASON_INVALID_PAIR_CIPHER = 19;
const uint16_t REASON_INVALID_AKMP = 20;

// QoS related
const uint16_t REASON_UNSUP_RSNE_VER = 21;
const uint16_t REASON_INVALID_RSNE_CAP = 22;
const uint16_t REASON_IEEE802_1X_FAIL = 23;
const uint16_t REASON_CIPHER_REJECT = 24;

// Reason code arrays
extern const uint16_t REASONS_STANDARD[];
extern const uint16_t REASONS_SECURITY[];
extern const uint16_t REASONS_IOS[];
extern const uint16_t REASONS_ANDROID[];
extern const uint16_t REASONS_ALL[];

extern const int REASONS_STANDARD_COUNT;
extern const int REASONS_SECURITY_COUNT;
extern const int REASONS_IOS_COUNT;
extern const int REASONS_ANDROID_COUNT;
extern const int REASONS_ALL_COUNT;

// ============== GLOBAL VARIABLES (extern) ==============
extern volatile DeviceState currentState;
extern volatile AttackMode currentAttackMode;
extern bool ledEnabled;

extern SemaphoreHandle_t channelMutex;
extern volatile uint8_t currentChannel;

extern const char *ssid_ap;
extern const char *pass_ap;
extern int current_channel;

extern std::vector<WiFiScanResult> scan_results;
extern std::vector<int> attack_targets_24;
extern std::vector<int> attack_targets_5;


// Evil Twin variables
extern String evilTwinSSID;
extern int evilTwinTargetIdx;
extern String lastCapturedPassword;
extern std::vector<String> passwordHistory;
extern bool evilTwinActive;
extern uint8_t deauth_bssid[6];

// Attack statistics
extern volatile uint32_t totalFramesSent24;
extern volatile uint32_t totalFramesSent5;

// Active reason codes
extern const uint16_t* activeReasons;
extern int activeReasonCount;

// ============== UTILITY FUNCTIONS ==============
String urlDecode(String input);
void switchChannelSafe(uint8_t ch);

#endif // CONFIG_H
