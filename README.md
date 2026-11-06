# Bluetooth Control Car with Two ESP32

Dự án này gồm 2 board ESP32:
- ESP32 Client: gắn trên xe, nhận lệnh điều khiển từ ESP32 Server và điều khiển động cơ, cảm biến, còi.
- ESP32 Server: làm bộ điều khiển từ xa, đọc joystick/ADC và nút bấm rồi gửi dữ liệu qua Bluetooth Classic SPP.

Project này đang dùng ESP-IDF và có thể build/flash trực tiếp từ Visual Studio Code bằng extension ESP-IDF.

---

## 1. Yêu cầu phần mềm

### Cài đặt trước
- VS Code
- Git
- Python 3.10 hoặc 3.11 (khuyến nghị 3.11)
- USB driver cho ESP32 (nếu máy chưa có, cài driver CH340/CP210x tuỳ board)

### Cài đặt ESP-IDF 5.5
Khuyến nghị cài đúng phiên bản ESP-IDF 5.5.x.

Các bước:
1. Mở VS Code.
2. Cài extension: Espressif IDF.
3. Mở Command Palette (Ctrl+Shift+P).
4. Chạy lệnh:
   - "ESP-IDF: Configure ESP-IDF extension"
5. Chọn:
   - ESP-IDF version: 5.5.x
   - Python version: 3.11 (nếu có)
   - Toolchain: tự động download
6. Đợi extension tải toolchain và các package cần thiết.

> Nếu bạn chưa từng cài ESP-IDF, quá trình này có thể mất vài phút và cần kết nối internet.

---

## 2. Mở project đúng thư mục
Dự án có 2 project riêng biệt:
- Thư mục client_car: firmware cho xe
- Thư mục server_remote: firmware cho bộ điều khiển từ xa

Bạn nên mở từng thư mục riêng trong VS Code để build/flash riêng.

Ví dụ:
- Mở thư mục: client_car
- Sau đó mở lại VS Code khác cho: server_remote

---

## 3. Sơ đồ nối dây cho ESP32 Client (xe)
Board này chạy firmware trong thư mục client_car.

### 3.1. Module động cơ L298N
| Chân ESP32 | Chân module L298N | Chú thích |
|---|---|---|
| GPIO25 | IN1 | PWM bánh trái |
| GPIO26 | IN2 | Hướng bánh trái |
| GPIO27 | IN3 | PWM bánh phải |
| GPIO14 | IN4 | Hướng bánh phải |

### 3.2. Cảm biến siêu âm HC-SR04
| Chân ESP32 | Chân HC-SR04 | Chú thích |
|---|---|---|
| GPIO5 | TRIG | Tín hiệu phát xung |
| GPIO18 | ECHO | Tín hiệu nhận xung |

### 3.3. MPU6050
| Chân ESP32 | Chân MPU6050 | Chú thích |
|---|---|---|
| GPIO21 | SDA | I2C Data |
| GPIO22 | SCL | I2C Clock |
| 3V3 | VCC | Cấp nguồn |
| GND | GND | Mass |

### 3.4. Các chân phụ khác
| Chân ESP32 | Chú thích |
|---|---|
| GPIO32 | Còi / relay (được code dùng cho horn) |

### 3.5. Cấp nguồn
- L298N: dùng nguồn 5V riêng cho động cơ nếu cần, đảm bảo nối chung GND với ESP32.
- ESP32: cấp 5V qua USB hoặc bộ nguồn 5V ổn định.

> Nếu động cơ mạnh, nên dùng nguồn riêng cho motor và giữ chung GND với ESP32 để tránh nhiễu.

---

## 4. Sơ đồ nối dây cho ESP32 Server (bộ điều khiển từ xa)
Board này chạy firmware trong thư mục server_remote.

### 4.1. Joystick analog / ADC
Code đang đọc 2 giá trị ADC từ các chân sau:
| Chân ESP32 | Chú thích |
|---|---|
| GPIO36 | ADC đầu vào X (VP) |
| GPIO34 | ADC đầu vào Y (VN) |

### 4.2. Nút bấm và còi
| Chân ESP32 | Chú thích |
|---|---|
| GPIO32 | Nút bấm 1 |
| GPIO33 | Nút bấm 2 |
| GPIO22 | Còi / tín hiệu horn |

### 4.3. Cấp nguồn
- Joystick và nút bấm có thể dùng 3.3V từ ESP32.
- Nếu dùng module nối ngoài, nối GND chung.

---

## 5. Build và flash bằng extension ESP-IDF trên VS Code

### Bước 1: Mở project
- Mở thư mục client_car để build firmware xe.
- Mở thư mục server_remote để build firmware bộ điều khiển từ xa.

### Bước 2: Chọn target
Trong VS Code, chạy lệnh:
- "ESP-IDF: Set Espressif device target"
- Chọn: esp32

### Bước 3: Build project
Bạn có thể dùng một trong các cách sau:
- Nhấn biểu tượng Build trong thanh công cụ ESP-IDF
- Hoặc chạy lệnh: "ESP-IDF: Build your project"

### Bước 4: Chọn cổng COM/tty
- Kết nối ESP32 bằng cáp USB.
- Chạy lệnh:
  - "ESP-IDF: Select Port"
- Chọn cổng COM tương ứng (ví dụ COM3, COM4, /dev/ttyUSB0)

### Bước 5: Flash firmware
- Chạy lệnh:
  - "ESP-IDF: Flash your project"
- Extension sẽ tự động build lại nếu cần rồi nạp code vào board.

### Bước 6: Mở Serial Monitor
- Chạy lệnh:
  - "ESP-IDF: Monitor your device"
- Tốc độ baud: 115200

---

## 6. Cách nạp code cho từng board

### 6.1. Nạp firmware cho xe (client_car)
1. Mở thư mục client_car trong VS Code.
2. Đảm bảo ESP-IDF đã cấu hình xong.
3. Build.
4. Flash.
5. Mở monitor để xem log.

### 6.2. Nạp firmware cho bộ điều khiển (server_remote)
1. Mở thư mục server_remote trong VS Code.
2. Build.
3. Flash.
4. Mở monitor.

> Hai firmware là hai project riêng. Không dùng chung một build output.

---

## 7. Khởi động và kiểm tra
Sau khi flash xong:
- ESP32 Server: nên hiện log Bluetooth Server và chờ client kết nối.
- ESP32 Client: sẽ kết nối tới server qua Bluetooth Classic SPP.
- Nếu kết nối thành công, xe sẽ nhận lệnh điều khiển từ bộ điều khiển.

### Kiểm tra nhanh
- Nhấn nút trên server, xem log có xuất hiện thông tin không.
- Nếu xe không chạy, kiểm tra lại:
  - Cáp nối động cơ và GND
  - Cổng USB đã được nhận đúng
  - Board đã chọn đúng target esp32
  - Firmware đã flash đúng project

---

## 8. Gỡ lỗi thường gặp

### Lỗi không thấy cổng COM
- Kiểm tra cáp USB
- Cài driver phù hợp cho chip USB của board
- Thử cắm lại board

### Build lỗi thiếu ESP-IDF
- Chạy lại "ESP-IDF: Configure ESP-IDF extension"
- Đảm bảo đã chọn ESP-IDF 5.5.x

### Bluetooth không kết nối
- Đảm bảo cả 2 board đều đang chạy firmware đúng
- Kiểm tra board ESP32 có hỗ trợ Bluetooth Classic
- Nếu dùng board không chuẩn, có thể bị lỗi về module hoặc pin

### Động cơ không chạy
- Kiểm tra nguồn 5V cho motor driver
- Kiểm tra GND chung giữa ESP32 và driver
- Kiểm tra chân GPIO nối đúng với code

---

## 9. Ghi chú quan trọng
- Dự án này dùng ESP-IDF 5.5.x, không nên dùng phiên bản cũ hơn.
- Mỗi thư mục client_car và server_remote là một project riêng.
- Nếu bạn muốn sửa code, hãy mở đúng thư mục project trước khi build.

---

## 10. Tóm tắt nhanh
- Cài VS Code + ESP-IDF 5.5.x
- Cài extension Espressif IDF
- Mở đúng thư mục project
- Build và Flash qua extension
- Theo đúng bảng chân GPIO ở trên

Nếu bạn cần, mình có thể tiếp tục viết thêm một phiên bản README có hình ảnh sơ đồ nối dây rõ hơn hoặc tạo file PDF/PNG sơ đồ cho từng board.
