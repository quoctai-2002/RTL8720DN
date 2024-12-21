Dưới đây là hướng dẫn sử dụng chi tiết, bao gồm các trường và endpoint cụ thể mà bạn có thể truy cập trên cổng Web của ESP8266:
 
 
 
ESP8266 WiFi Captive Portal
 
Dự án này tạo ra một cổng kết nối WiFi captive đơn giản trên ESP8266, cho phép bạn tùy chỉnh trang truy cập mạng. Bạn có thể cấu hình các cài đặt mạng và hiển thị trang HTML tùy chỉnh cho người dùng khi họ kết nối vào mạng WiFi của bạn.
 
Tính năng
 
- Trang Cổng Mặc định và Tùy chỉnh: Lựa chọn giữa trang cấu hình mặc định hoặc một trang HTML tùy chỉnh do bạn tạo.
- Cấu hình Mạng: Cho phép thay đổi SSID, mật khẩu, và kênh của mạng WiFi.
- Quản lý Mật khẩu: Lưu trữ mật khẩu an toàn và cung cấp tùy chọn để xóa chúng.
- Chỉ báo LED: LED trên board nhấp nháy để báo hiệu khi mật khẩu được cập nhật.
 
Yêu cầu
 
- Mô-đun ESP8266
- Arduino IDE với hỗ trợ board ESP8266
- Các thư viện cần thiết:
-  <ESP8266WiFi.h> 
-  <DNSServer.h> 
-  <ESP8266WebServer.h> 
-  <EEPROM.h> 
 
Cài đặt
 
1. Chuẩn bị Arduino IDE: Đảm bảo Arduino IDE đã được cài đặt cùng với ESP8266 board từ Boards Manager.
2. Cài đặt Thư viện: Sử dụng Arduino Library Manager để cài đặt các thư viện cần thiết.
3. Kết nối ESP8266: Kết nối ESP8266 đến máy tính của bạn thông qua cáp USB.
 
Tải lên Dự án
 
1. Mở mã nguồn: Tải mã vào Arduino IDE và mở file  .ino .
2. Chọn Board và Cổng: Trong Arduino IDE, chọn đúng board ESP8266 và cổng COM mà thiết bị đang kết nối.
3. Tải lên: Nhấn nút  Upload  trong Arduino IDE để tải mã lên ESP8266.
 
Sử dụng
 
1. Khởi động ESP8266: Sau khi tải mã lên, ESP8266 sẽ khởi động và tạo một mạng WiFi mới với SSID mặc định là "Free WiFi".
2. Kết nối WiFi: Sử dụng một thiết bị khác để kết nối đến mạng "Free WiFi".
3. Truy cập Cổng Cấu hình:
- Mở trình duyệt web và nhập địa chỉ IP của gateway (thường là  172.0.0.1 ) để truy cập vào cổng cấu hình.
 
Các Endpoint Trên Cổng Web
 
-  / : Trang chính với thông báo cập nhật firmware.
-  /post : Xử lý việc gửi mật khẩu và cập nhật chúng.
-  /ssid : Trang cho phép bạn thay đổi SSID, mật khẩu và kênh WiFi.
-  /postSSID : Xử lý dữ liệu khi gửi từ trang  /ssid .
-  /pass : Hiển thị danh sách mật khẩu đã lưu.
-  /clear : Xóa tất cả mật khẩu đã lưu.
-  /captive : Trang cấu hình để chọn giữa trang cổng mặc định hoặc tùy chỉnh.
-  /postCaptive : Xử lý dữ liệu khi gửi từ trang  /captive .
 
Tùy chỉnh Trang Cổng
 
- Sử dụng endpoint  /captive  để chọn tùy chọn trang cổng và nhập HTML tùy chỉnh.
- Lưu ý rằng nội dung HTML phải hợp lệ để đảm bảo hiển thị đúng cách.
 
Ví dụ HTML Tùy chỉnh
 
Đây là một trang "Hello World" đơn giản bạn có thể sử dụng:
 
html
  
<!DOCTYPE html>
<html>
<head>
    <title>Hello World</title>
</head>
<body>
    <h1>Hello, World!</h1>
</body>
</html>
 
 
Lưu ý
 
- Đảm bảo rằng nội dung HTML nhập vào là hợp lệ để tránh lỗi hiển thị.
- Sử dụng cẩn thận với các thông tin nhạy cảm khi lưu trữ mật khẩu trong EEPROM.
 
Tác giả
 
- Quốc Tài
 
Lời cảm ơn
 
- Cảm ơn đến cộng đồng mã nguồn mở và những người đã đóng góp ý tưởng cho dự án này.
