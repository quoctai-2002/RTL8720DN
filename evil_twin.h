#ifndef EVIL_TWIN_H
#define EVIL_TWIN_H

#include "config.h"

// Khởi động Evil Twin attack
void startEvilTwin(int targetIdx);

// Dừng Evil Twin và khôi phục AP gốc
void stopEvilTwin();

// Xử lý captive portal page
void handleCaptivePortal(WiFiClient &client);

// Trang thành công sau khi capture password
void handleCaptureSuccess(WiFiClient &client);

// Xử lý form submit password
void handlePasswordSubmit(WiFiClient &client, String &request);

// Kiểm tra có phải captive portal request không
bool isCaptivePortalRequest(String &path);

// FreeRTOS task - Deauth liên tục trong Evil Twin mode
void EvilTwinDeauthTask(void *pvParameters);

// Helper - Chunked HTTP transfer
void sendChunk(WiFiClient &client, String &data);

#endif // EVIL_TWIN_H
