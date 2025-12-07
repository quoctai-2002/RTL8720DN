# RTL8720DN Deauther v3.0

Công cụ kiểm tra bảo mật WiFi dual-band (2.4GHz + 5GHz) trên nền tảng RTL8720DN/BW16.

## ⚡ Tính năng

- **Deauth/Disassoc Attack** - Ngắt kết nối thiết bị khỏi WiFi (cả 2.4 & 5GHz)
- **Evil Twin** - Tạo mạng giả với Captive Portal để thu thập mật khẩu
- **Multi-target** - Tấn công nhiều mạng cùng lúc
- **Web UI** - Giao diện điều khiển qua trình duyệt

## 🔧 Phần cứng

| Thành phần | Mô tả |
|------------|-------|
| Board | BW16 / RTL8720DN |
| Anten | Onboard hoặc anten rời 2.4/5GHz (khuyến nghị) |
| Nguồn | USB 5V hoặc pin 3.7V LiPo |

## 📦 Cài đặt

1. Cài đặt [Arduino IDE](https://www.arduino.cc/en/software)
2. Thêm board RTL8720DN:
   - File → Preferences → Additional Boards Manager URLs:
   ```
   https://github.com/ambiot/ambd_arduino/raw/master/Arduino_package/package_realtek_amebad_index.json
   ```
3. Tools → Board → Boards Manager → Tìm "Ameba" → Install
4. Chọn board: **RTL8720DN(BW16)**
5. Upload code

## 🚀 Sử dụng

1. Cấp nguồn cho board
2. Kết nối WiFi: `RTL8720-Deauther` (pass: `deauther`)
3. Mở trình duyệt: `http://192.168.1.1`
4. Bấm **Scan** để quét mạng
5. Chọn mục tiêu và **Launch Attack**

## 📱 Giao diện

### Dashboard
- **Networks** - Số mạng đã quét (tối đa 15)
- **Frames Sent** - Số frame đã gửi
- **Captured** - Số mật khẩu đã thu thập

### Deauth Attack
- Chọn nhiều mạng cùng lúc
- Chế độ: Combo, Deauth Only, Disassoc Only, Aggressive
- Reason codes: All, Standard, iOS, Android

### Evil Twin
- Chọn 1 mạng để clone
- Tự động deauth thiết bị về mạng giả
- Captive Portal thu thập mật khẩu

## 📁 Cấu trúc file

```
main/
├── main.ino          # Entry point, setup, FreeRTOS tasks
├── config.h          # Cấu hình, định nghĩa, biến global
├── attack.cpp/h      # Deauth/Disassoc attack logic
├── evil_twin.cpp/h   # Evil Twin + Captive Portal
├── web_server.cpp/h  # Web UI controller
├── web_pages.h       # HTML/CSS/JS templates
├── dns_server.cpp/h  # DNS hijacking cho captive portal
├── flash_storage.cpp/h # Lưu mật khẩu vào flash
└── led_utils.cpp/h   # LED status indicators
```

## ⚙️ Cấu hình

Chỉnh trong `config.h`:

```cpp
#define MAX_SCAN_RESULTS      15   // Giới hạn mạng hiển thị
#define OPTIMIZED_FRAMES_24   10   // Số frame mỗi burst (2.4GHz)
#define OPTIMIZED_FRAMES_5    20   // Số frame mỗi burst (5GHz)
#define OPTIMIZED_INTERVAL_24 20   // Delay giữa các burst (ms)
#define OPTIMIZED_INTERVAL_5  50   
```

## 💡 LED Status

| Màu | Trạng thái |
|-----|------------|
| 🔴 Đỏ nhấp nháy chậm | Idle |
| 🔴 Đỏ nhấp nháy nhanh | Đang scan |
| 🟢 Xanh lá nhấp nháy | Đang tấn công |
| 🔵 Xanh dương nhấp nháy | Evil Twin active |

## ⚠️ Lưu ý

- **Chỉ sử dụng cho mục đích học tập và kiểm tra bảo mật**
- Không sử dụng để tấn công mạng không có sự cho phép
- Việc sử dụng sai mục đích có thể vi phạm pháp luật

## 📝 Changelog

### v3.0
- Tối ưu RAM với chunked transfer encoding
- Giới hạn 15 mạng WiFi khi quét
- Thêm nút Clear credentials
- Fix lỗi Select All
- Dual-band deauth cho Evil Twin

---

**Made with ❤️ for educational purposes only**
