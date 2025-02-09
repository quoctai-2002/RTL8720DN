
```markdown
# RTL8720DN Deauther

![License](https://img.shields.io/badge/license-Apache%202.0-blue)

## Giới thiệu

**RTL8720DN Deauther** là một dự án mã nguồn mở được xây dựng trên chip RTL8720DN (Ameba) với mục tiêu trình diễn các kỹ thuật tấn công deauthentication và beacon flooding trên mạng WiFi. Dự án này được phát triển bằng C/C++ với Arduino framework, sử dụng FreeRTOS để quản lý các tác vụ song song và các API WiFi của RTL8720DN SDK.

> **Lưu ý quan trọng:**  
> Dự án này được phát triển cho mục đích học tập và nghiên cứu. Việc sử dụng nó để tấn công hoặc làm gián đoạn các mạng không thuộc quyền kiểm soát của bạn là vi phạm pháp luật.

## Tính năng

- **Quét mạng WiFi:**  
  Quét và hiển thị các mạng WiFi xung quanh, hiển thị thông tin SSID, BSSID, kênh và RSSI.
  
- **Chọn mục tiêu tấn công:**  
  Giao diện web cho phép chọn các mạng mục tiêu theo từng băng tần (2.4GHz và 5GHz).

- **Tấn công deauthentication:**  
  Sử dụng các frame deauth tùy chỉnh để làm gián đoạn kết nối của các thiết bị mục tiêu.
  
- **Phát Beacon giả:**  
  Gửi các beacon frame “nhái” để tạo rối loạn cho các thiết bị mục tiêu.

- **Quản lý tác vụ bằng FreeRTOS:**  
  Tách riêng các task cho việc cập nhật LED, xử lý HTTP, tấn công deauth và phát beacon nhằm tối ưu hiệu năng và giảm chồng lấn.

## Cấu trúc dự án

- **main.cpp:**  
  Tập tin chính chứa hàm `setup()` và `loop()`, khởi tạo WiFi AP, quét mạng ban đầu và tạo các task FreeRTOS.

- **wifi_conf.h, wifi_cust_tx.h, wifi_util.h, wifi_structures.h:**  
  Các tập tin header cung cấp các định nghĩa, cấu trúc và hàm API liên quan đến WiFi và truyền dữ liệu trên RTL8720DN.

- **debug.h:**  
  Cung cấp các hàm debug và logging để theo dõi quá trình chạy của các tác vụ.

## Yêu cầu

- **Phần cứng:**  
  - RTL8720DN (Ameba) module  
  - Thiết bị có khả năng kết nối WiFi (để sử dụng giao diện web)
  - LED (nếu có) được định nghĩa qua chân (hoặc tích hợp sẵn)

- **Phần mềm:**  
  - Arduino IDE hoặc môi trường phát triển tương thích với RTL8720DN  
  - RTL8720DN SDK (các file header như `wifi_conf.h`, `wifi_cust_tx.h`,… cần được tích hợp)  
  - FreeRTOS tích hợp (đã có sẵn trong SDK hoặc cần cài đặt riêng)

## Hướng dẫn cài đặt

1. **Cài đặt Arduino IDE và RTL8720DN SDK:**  
   Đảm bảo bạn đã cài đặt Arduino IDE và thêm bộ thư viện của RTL8720DN SDK theo hướng dẫn của nhà sản xuất.

2. **Tải mã nguồn dự án:**  
   Clone repository từ GitHub:
   ```bash
   git clone https://github.com/quoctai-2002/RTL8720DN.git
   ```
   
3. **Mở dự án trong Arduino IDE:**  
   Mở tập tin `main.cpp` (hoặc file `.ino` nếu dự án được định dạng như vậy).

4. **Biên dịch và tải lên:**  
   Chọn board phù hợp (RTL8720DN) và tải mã lên thiết bị.

## Cách sử dụng

1. **Khởi động và cấu hình WiFi AP:**  
   Sau khi tải mã, thiết bị của bạn sẽ khởi tạo AP với SSID `BW16` và mật khẩu `deauther`.

2. **Quét mạng:**  
   Giao diện web sẽ hiển thị các mạng WiFi xung quanh, cùng thông tin SSID, BSSID, kênh và RSSI.

3. **Chọn mục tiêu tấn công:**  
   Chọn các mục tiêu mong muốn qua giao diện web và nhấn nút "Launch Deauth" để bắt đầu tấn công deauth hoặc "Launch Beacon" để phát Beacon giả.

4. **Dừng tấn công:**  
   Sử dụng nút "Stop Attack/Beacon" trên giao diện web để dừng tấn công.

## Cấu hình hiệu suất

Để tối ưu hiệu suất tấn công trên hai băng tần, dự án cho phép điều chỉnh số frame gửi và khoảng thời gian giữa các chu kỳ tấn công:

- **2.4 GHz:**
  - Số frame deauth: **5**
  - ATTACK_INTERVAL: **40–50 ms** (đề xuất ban đầu là 40ms nếu giảm tối đa)

- **5 GHz:**
  - Số frame deauth: **10**
  - ATTACK_INTERVAL: **90–110 ms** (đề xuất ban đầu là 90ms hoặc 110ms nếu cần tối ưu)
  
- **Beacon:**
  - Số frame beacon: **200**  
    (Có thể điều chỉnh nếu cần chu kỳ gửi beacon nhanh hơn hoặc chậm hơn)

Các thông số này có thể được điều chỉnh trong code bằng cách thay đổi các định nghĩa:

```cpp
#define FRAMES_PER_DEAUTH_24 5    // cho băng 2.4GHz
#define FRAMES_PER_DEAUTH_5  10   // cho băng 5GHz
#define FRAMES_PER_BEACON    200  // cho beacon

const unsigned long ATTACK_INTERVAL_24 = 40;  // cho băng 2.4GHz
const unsigned long ATTACK_INTERVAL_5  = 90;  // cho băng 5GHz
```

Bạn nên thử nghiệm và điều chỉnh các giá trị này dựa trên hiệu suất thực tế và môi trường sử dụng.

## Cảnh báo pháp lý

**Quan trọng:**  
Dự án này chỉ dành cho mục đích học tập và nghiên cứu. Việc sử dụng công cụ này để tấn công, làm gián đoạn hoặc xâm nhập vào các hệ thống mà bạn không có quyền truy cập là vi phạm pháp luật. Hãy sử dụng dự án này một cách có trách nhiệm và chỉ trong môi trường kiểm soát.

Liên hệ: nguyendinhquoctai@gmail.com
---
