#include "wifi_cust_tx.h"

void wifi_tx_raw_frame(void* frame, size_t length) {
  void *ptr = (void *)**(uint32_t **)(rltk_wlan_info + 0x10);
  void *frame_control = alloc_mgtxmitframe(((uint8_t*)ptr) + 0xae0);
  if (frame_control != 0) {
    update_mgntframe_attrib(ptr, ((uint8_t*)frame_control) + 8);
    memset((void *)*(uint32_t *)(((uint8_t*)frame_control) + 0x80), 0, 0x68);
    uint8_t *frame_data = (uint8_t *)*(uint32_t *)(((uint8_t*)frame_control) + 0x80) + 0x28;
    memcpy(frame_data, frame, length);
    *(uint32_t *)(((uint8_t*)frame_control) + 0x14) = length;
    *(uint32_t *)(((uint8_t*)frame_control) + 0x18) = length;
    dump_mgntframe(ptr, frame_control);
  }
}

void wifi_tx_deauth_frame(void* src_mac, void* dst_mac, uint16_t reason) {
  DeauthFrame frame;
  memcpy(&frame.source, src_mac, 6);
  memcpy(&frame.access_point, src_mac, 6);
  memcpy(&frame.destination, dst_mac, 6);
  frame.reason = reason;
  wifi_tx_raw_frame(&frame, sizeof(DeauthFrame));
}

void wifi_tx_disassoc_frame(void* src_mac, void* dst_mac, uint16_t reason) {
  DisassocFrame frame;
  memcpy(&frame.source, src_mac, 6);
  memcpy(&frame.access_point, src_mac, 6);
  memcpy(&frame.destination, dst_mac, 6);
  frame.reason = reason;
  wifi_tx_raw_frame(&frame, sizeof(DisassocFrame));
}

// Legacy open beacon (no security)
void wifi_tx_beacon_frame(void* src_mac, void* dst_mac, const char *ssid) {
  BeaconFrameBase frame;
  memcpy(&frame.source, src_mac, 6);
  memcpy(&frame.access_point, src_mac, 6);
  memcpy(&frame.destination, dst_mac, 6);
  frame.ap_capabilities = 0x0401;  // Open network (no privacy)
  
  int ssid_len = 0;
  for (int i = 0; ssid[i] != '\0' && i < 32; i++) {
    frame.ssid[i] = ssid[i];
    ssid_len++;
  }
  frame.ssid_length = ssid_len;
  
  wifi_tx_raw_frame(&frame, 38 + ssid_len);
}

// Beacon with WPA2-PSK security (shows lock icon 🔒)
void wifi_tx_beacon_frame_wpa2(void* src_mac, void* dst_mac, const char* ssid, uint8_t channel) {
  BeaconFrameWPA2 frame;
  
  // Set MACs
  memcpy(&frame.source, src_mac, 6);
  memcpy(&frame.access_point, src_mac, 6);
  memcpy(&frame.destination, dst_mac, 6);
  
  // Set SSID
  int ssid_len = 0;
  for (int i = 0; ssid[i] != '\0' && i < 32; i++) {
    frame.ssid[i] = ssid[i];
    ssid_len++;
  }
  frame.ssid_length = ssid_len;
  
  // Set channel
  frame.ds_channel = channel;
  
  // Build frame buffer manually for correct layout
  uint8_t buffer[150];
  int pos = 0;
  
  // Frame control + Duration
  buffer[pos++] = 0x80; buffer[pos++] = 0x00;  // Beacon
  buffer[pos++] = 0x00; buffer[pos++] = 0x00;  // Duration
  
  // Destination (broadcast)
  memcpy(&buffer[pos], dst_mac, 6); pos += 6;
  // Source
  memcpy(&buffer[pos], src_mac, 6); pos += 6;
  // BSSID
  memcpy(&buffer[pos], src_mac, 6); pos += 6;
  
  // Sequence number
  buffer[pos++] = 0x00; buffer[pos++] = 0x00;
  
  // Timestamp (8 bytes)
  for (int i = 0; i < 8; i++) buffer[pos++] = 0x00;
  
  // Beacon interval
  buffer[pos++] = 0x64; buffer[pos++] = 0x00;  // 100 TUs
  
  // Capability info (ESS + Privacy = WPA2)
  buffer[pos++] = 0x11; buffer[pos++] = 0x04;
  
  // SSID IE
  buffer[pos++] = 0;  // Element ID
  buffer[pos++] = ssid_len;
  for (int i = 0; i < ssid_len; i++) buffer[pos++] = ssid[i];
  
  // Supported Rates IE
  buffer[pos++] = 1;  // Element ID
  buffer[pos++] = 8;  // Length
  buffer[pos++] = 0x82; buffer[pos++] = 0x84;
  buffer[pos++] = 0x8b; buffer[pos++] = 0x96;
  buffer[pos++] = 0x0c; buffer[pos++] = 0x12;
  buffer[pos++] = 0x18; buffer[pos++] = 0x24;
  
  // DS Parameter Set
  buffer[pos++] = 3;  // Element ID
  buffer[pos++] = 1;  // Length
  buffer[pos++] = channel;
  
  // RSN IE (WPA2-PSK-CCMP)
  buffer[pos++] = 48;  // Element ID (RSN)
  buffer[pos++] = 20;  // Length
  // Version
  buffer[pos++] = 0x01; buffer[pos++] = 0x00;
  // Group Cipher Suite (CCMP)
  buffer[pos++] = 0x00; buffer[pos++] = 0x0f;
  buffer[pos++] = 0xac; buffer[pos++] = 0x04;
  // Pairwise Cipher Suite Count
  buffer[pos++] = 0x01; buffer[pos++] = 0x00;
  // Pairwise Cipher Suite (CCMP)
  buffer[pos++] = 0x00; buffer[pos++] = 0x0f;
  buffer[pos++] = 0xac; buffer[pos++] = 0x04;
  // AKM Suite Count
  buffer[pos++] = 0x01; buffer[pos++] = 0x00;
  // AKM Suite (PSK)
  buffer[pos++] = 0x00; buffer[pos++] = 0x0f;
  buffer[pos++] = 0xac; buffer[pos++] = 0x02;
  // RSN Capabilities
  buffer[pos++] = 0x00; buffer[pos++] = 0x00;
  
  wifi_tx_raw_frame(buffer, pos);
}
