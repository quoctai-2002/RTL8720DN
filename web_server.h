#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include "config.h"

// Parse HTTP request path
String parseRequest(String request);

// Parse POST data
std::vector<std::pair<String, String>> parsePost(String &request);

// Tạo HTTP response header
String makeResponse(int code, String content_type);

// Xử lý trang chính (dashboard)
void handleRoot(WiFiClient &client);

// Xử lý 404
void handle404(WiFiClient &client);

// Web Server Task
void WebServerTask(void *pvParameters);

#endif // WEB_SERVER_H
