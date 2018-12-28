#ifndef __BC_PACKET_H
#define __BC_PACKET_H

typedef struct {
#define BC_VERSION_1 0x80  // 和字符串兼容，> 0x7F
    unsigned char version;  // 协议版本
#define BC_TYPE_START_TRANSACTION 1
#define BC_TYPE_REQUEST_CONTRAST 2
#define BC_TYPE_REPLY_CONTRAST 3
#define BC_TYPE_CONFIRM_CONTRAST 4
#define BC_TYPE_TRANSACTION_SUCCESS 5
#define BC_TYPE_TRANSACTION_BOARDCAST 6
    unsigned char type;
    unsigned short padding;

    unsigned int sender;
    unsigned int receiver;
    unsigned int amount;  // 精确到0.1分，实际钱数为 amount/1000.0
} bc_packet_t;

int bc_packet(unsigned char type, unsigned int sender, unsigned int receiver, unsigned int amount, bc_packet_t* packet);
int bc_packet_parse(const char* msg, unsigned int length , bc_packet_t* packet);

#endif
