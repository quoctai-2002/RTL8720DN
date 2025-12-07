# RTL8720DN Dual-Band Deauther & Evil Twin

Advanced WiFi Deauther and Evil Twin attack firmware designed specifically for the **Realtek RTL8720DN (BW16)** dual-band WiFi module. This project leverages the 5GHz capabilities of the RTL8720DN to target both 2.4GHz and 5GHz networks.

> **⚠️ DISCLAIMER**  
> This software is for **EDUCATIONAL PURPOSES ONLY**. It is intended to test and demonstrate security vulnerabilities in your own networks.  
> - Do not use this tool on networks you do not own or have permission to test.  
> - The authors take no responsibility for any misuse or legal consequences.

---

## 🔥 Key Features

- **Dual-Band Support**: Targets both **2.4GHz** and **5GHz** networks simultaneously.
- **Efficient Deauthentication**: 
  - Optimized frame intervals (2s) to maintain stability.
  - Automatically detects if a target SSID exists on both bands and attacks both.
- **Evil Twin Attack**:
  - Clones target SSID (Open network).
  - **Captive Portal**: Professional "Firmware Update" style page to capture passwords.
  - **Chunked Transfer Encoding**: Optimized HTML serving for low RAM usage.
  - **Native DNS Server**: Redirects all traffic to the captive portal using lwIP callbacks.
- **Credential Harvesting**:
  - Captures WPA2 passwords.
  - Stores up to 10 credentials in Flash memory (persistent across reboots).
  - Displays captured keys in the Web Interface.
- **System Stability**:
  - Auto-hard reset after successful password capture to restore clean state.
  - Optimized power settings for battery operation.

---

## 🛠️ Hardware Requirements

- **Module**: Realtek RTL8720DN (BW16)
- **Power**: 5V/1A Source (MicroUSB) or 3.7V LiPo Battery (ensure stable voltage).

---

## ⚙️ Installation

1. **Environment**: Arduino IDE with `AmebaD` SDK installed.
2. **Library Dependencies**:
   - `FlashStorage_RTL8720` (included in source).
3. **Flashing**:
   - Board: `Realtek RTL8720DN (BW16)`
   - Upload Method: Serial (UART)
4. **Compile & Upload**: Open `main.ino` and upload to the board.

---

## 🚀 Usage

### 1. Connect
Power on the device. It will create a management Access Point:
- **SSID**: `RTL8720-Deauther`
- **Password**: `deauther`
- **IP Address**: `192.168.1.1`

### 2. Web Interface
Access `http://192.168.1.1` in your browser.

- **Scan Tab**: Scan for nearby 2.4GHz and 5GHz networks.
- **Attack Tab**: Select a target to perform Disassociation/Deauthentication attacks.
- **Evil Twin Tab**:
  1. Select a target network (Radio button).
  2. Click **Start Evil Twin**.
  3. The device will switch to Evil Twin mode, creating a clone Open AP.
  4. Victims connecting to this AP will see a "Firmware Update" page asking for the password.
  5. Once a password is submitted:
     - It is saved to Flash.
     - The device shows a success page.
     - The device **reboots automatically** to restore full functionality.

### 3. Viewing Passwords
After the device reboots, reconnect to the management AP (`RTL8720-Deauther`).
- Go to the Web Interface.
- Click the **🔑 Keys** button to reveal captured credentials.

---

## 🔴 LED Indicators

- **Red (Blinking Slow)**: Idle / Ready.
- **Red (Blinking Fast)**: Scanning.
- **Green (Blinking)**: Deauth Attack Active.
- **Blue (Blinking)**: Evil Twin Mode Active.

---

## 📝 Technical Notes

- **Deauth Strategy**: Uses a 2-second interval for deauth frames to prevent self-interference and maintain AP stability.
- **Encoding**: Captive portal uses chunked transfer encoding to keep RAM usage low (~2KB).
- **Reset**: The system uses `sys_reset()` from the Realtek SDK to ensure a clean exit from Evil Twin mode.

