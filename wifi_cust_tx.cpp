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

void wifi_tx_beacon_frame(void* src_mac, void* dst_mac, const char *ssid) {
  BeaconFrame frame;
  memcpy(&frame.source, src_mac, 6);
  memcpy(&frame.access_point, src_mac, 6);
  memcpy(&frame.destination, dst_mac, 6);
  for (int i = 0; ssid[i] != '\0'; i++) {
    frame.ssid[i] = ssid[i];
    frame.ssid_length++;
  }
  wifi_tx_raw_frame(&frame, 38 + frame.ssid_length);
}
