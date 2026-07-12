#ifndef MY_PROTOCOL_H
#define MY_PROTOCOL_H

#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif
    typedef struct
    {
        uint8_t x_val;
        uint8_t y_val;
        uint8_t z_val;
    } adc_data_t;
    // 1. Định nghĩa các "Mã loại tin nhắn" (Message ID)
    typedef enum
    {
        MSG_ID_ADC_DATA = 0x01,
        MSG_ID_TEXT_ACK = 0x02,
        MSG_ID_COMMAND = 0x03
    } message_id_t;

// 2. Định nghĩa cấu trúc khung truyền (Frame)
// Thuộc tính packed bắt buộc compiler không được thêm byte rỗng (padding)
// để đảm bảo struct gửi đi và nhận về khớp nhau từng byte một.
#pragma pack(push, 1)

    // Khung chứa dữ liệu ADC
    typedef struct
    {
        uint8_t msg_id; // Luôn phải là byte đầu tiên
        adc_data_t data;
    } packet_adc_t;

    // Khung chứa phản hồi Text
    typedef struct
    {
        uint8_t msg_id; // Luôn phải là byte đầu tiên
        char text[63];  // Chứa nội dung chữ
    } packet_text_t;
    // ... (Khai báo enum và struct của bạn ở đây) ...

#ifdef __cplusplus
}
#endif

#endif // MY_PROTOCOL_H