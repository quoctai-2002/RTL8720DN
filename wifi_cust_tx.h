#ifndef WIFI_CUST_TX
#define WIFI_CUST_TX

#include <Arduino.h>

typedef struct {
  uint16_t frame_control = 0xC0;
  uint16_t duration = 0xFFFF;
  uint8_t destination[6];
  uint8_t source[6];
  uint8_t access_point[6];
  const uint16_t sequence_number = 0;
  uint16_t reason = 0x06;
} DeauthFrame;

// Disassociation frame structure (Type 0, Subtype 10 = 0xA0)
typedef struct {
  uint16_t frame_control = 0xA0;
  uint16_t duration = 0xFFFF;
  uint8_t destination[6];
  uint8_t source[6];
  uint8_t access_point[6];
  const uint16_t sequence_number = 0;
  uint16_t reason = 0x08;
} DisassocFrame;

// Beacon frame with WPA2 security capability
// ap_capabilities = 0x0411 means ESS + Privacy (shows lock icon)
typedef struct {
  uint16_t frame_control = 0x80;  // Beacon
  uint16_t duration = 0;
  uint8_t destination[6];
  uint8_t source[6];
  uint8_t access_point[6];
  uint16_t sequence_number = 0;
  uint64_t timestamp = 0;
  uint16_t beacon_interval = 0x64;  // 100 TUs
  uint16_t ap_capabilities = 0x0411;  // ESS + Privacy bit (shows 🔒)
  // Tagged parameters follow
  uint8_t ssid_tag = 0;
  uint8_t ssid_length = 0;
  uint8_t ssid[32];  // Max SSID length
} BeaconFrameBase;

// Full beacon with RSN IE for WPA2
typedef struct {
  // Base beacon
  uint16_t frame_control = 0x80;
  uint16_t duration = 0;
  uint8_t destination[6];
  uint8_t source[6];
  uint8_t access_point[6];
  uint16_t sequence_number = 0;
  uint64_t timestamp = 0;
  uint16_t beacon_interval = 0x64;
  uint16_t ap_capabilities = 0x0411;  // ESS + Privacy
  // SSID IE
  uint8_t ssid_tag = 0;
  uint8_t ssid_length = 0;
  uint8_t ssid[32];
  // Supported Rates IE (required)
  uint8_t rates_tag = 1;
  uint8_t rates_length = 8;
  uint8_t rates[8] = {0x82, 0x84, 0x8b, 0x96, 0x0c, 0x12, 0x18, 0x24};  // 1,2,5.5,11,6,9,12,18 Mbps
  // DS Parameter Set (channel)
  uint8_t ds_tag = 3;
  uint8_t ds_length = 1;
  uint8_t ds_channel = 1;
  // RSN IE (WPA2-PSK)
  uint8_t rsn_tag = 48;  // RSN Information Element
  uint8_t rsn_length = 20;
  uint8_t rsn_version[2] = {0x01, 0x00};  // Version 1
  uint8_t rsn_group_cipher[4] = {0x00, 0x0f, 0xac, 0x04};  // CCMP
  uint8_t rsn_pairwise_count[2] = {0x01, 0x00};  // 1 pairwise cipher
  uint8_t rsn_pairwise_cipher[4] = {0x00, 0x0f, 0xac, 0x04};  // CCMP
  uint8_t rsn_akm_count[2] = {0x01, 0x00};  // 1 AKM
  uint8_t rsn_akm_cipher[4] = {0x00, 0x0f, 0xac, 0x02};  // PSK
  uint8_t rsn_capabilities[2] = {0x00, 0x00};  // No special caps
} BeaconFrameWPA2;

extern uint8_t* rltk_wlan_info;
extern "C" void* alloc_mgtxmitframe(void* ptr);
extern "C" void update_mgntframe_attrib(void* ptr, void* frame_control);
extern "C" int dump_mgntframe(void* ptr, void* frame_control);

void wifi_tx_raw_frame(void* frame, size_t length);
void wifi_tx_deauth_frame(void* src_mac, void* dst_mac, uint16_t reason = 0x06);
void wifi_tx_disassoc_frame(void* src_mac, void* dst_mac, uint16_t reason = 0x08);

// Beacon with WPA2 security (shows lock icon)
void wifi_tx_beacon_frame_wpa2(void* src_mac, void* dst_mac, const char* ssid, uint8_t channel);

// Legacy open beacon (no password)
void wifi_tx_beacon_frame(void* src_mac, void* dst_mac, const char *ssid);

#endif
