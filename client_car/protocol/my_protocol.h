#ifndef MY_PROTOCOL_H
#define MY_PROTOCOL_H

#include <stdint.h>

// 1. Định nghĩa các "Mã loại tin nhắn" (Message ID)
typedef enum
{
    MSG_ID_CONTROL_DATA = 0x01,
    MSG_ID_TEXT_ACK = 0x02,
    MSG_ID_COMMAND = 0x03,
    MSG_ID_SHOCK_EVENT = 0x04
} message_id_t;

// 2. Định nghĩa cấu trúc khung truyền (Frame)
// Thuộc tính packed bắt buộc compiler không được thêm byte rỗng (padding)
// để đảm bảo struct gửi đi và nhận về khớp nhau từng byte một.
#pragma pack(push, 1)

// Khung chứa dữ liệu ADC
typedef struct
{
    uint8_t msg_id;    // Luôn phải là byte đầu tiên
    uint8_t direction; // 0: dừng, 1: tiến, 2: lùi, 3: trái, 4: phải
    uint8_t speed;     // 0 - 255
} packet_control_data_t;

typedef struct
{
    uint8_t msg_id; // Luôn phải là byte đầu tiên
    uint8_t is_shock;
} packet_shock_t;
// Khung chứa phản hồi Text
typedef struct
{
    uint8_t msg_id; // Luôn phải là byte đầu tiên
    char text[63];  // Chứa nội dung chữ
} packet_text_t;

#pragma pack(pop)

#endif