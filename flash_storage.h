#ifndef FLASH_STORAGE_H
#define FLASH_STORAGE_H

#include "config.h"

#define MAX_SSID_LENGTH 32
#define MAX_PWD_LENGTH 64
#define MAX_CREDENTIAL_ENTRIES 10
#define CREDENTIAL_MAGIC 0xA55A

// WiFi credential structure
struct WiFiCredential {
  char ssid[MAX_SSID_LENGTH + 1];
  char password[MAX_PWD_LENGTH + 1];
  uint32_t timestamp;  // For ordering (millis at capture)
};

// Global credential storage
extern WiFiCredential capturedCredentials[MAX_CREDENTIAL_ENTRIES];
extern int credentialCount;

// Save SSID + password to flash (auto-delete oldest if full)
void saveCredentialToFlash(String ssid, String password);

// Load saved credentials from flash
void loadCredentialsFromFlash();

// Clear all saved credentials
void clearCredentialsFromFlash();

// Get all credentials as HTML table rows for display
String getCredentialsHTML();

// Legacy functions for backward compatibility
void savePasswordToFlash(String password);
void loadPasswordsFromFlash();
void clearPasswordsFromFlash();

#endif // FLASH_STORAGE_H
