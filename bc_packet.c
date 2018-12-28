#include "bc_packet.h"
#include "string.h"
#include "stdio.h"

int bc_packet(unsigned char type, unsigned int sender, unsigned int receiver, unsigned int amount, bc_packet_t* packet) {
    packet->padding = 0;
    packet->type = type;
    packet->version = BC_VERSION_1;
    packet->sender = sender;
    packet->receiver = receiver;
    packet->amount = amount;
}

int bc_packet_parse(const unsigned char* msg, unsigned int length , bc_packet_t* packet) {
    printf("msg[0]: 0x%02X, length=%u, size=%lu\n", msg[0], length, sizeof(bc_packet_t));
    if (length == sizeof(bc_packet_t) && msg[0] == BC_VERSION_1) {
        memcpy(packet, msg, sizeof(bc_packet_t));
        return 0;
    }
    // TODO 兼容其他实现，比如字符串？
    return -1;
}

void bc_packet_print(bc_packet_t* packet) {
    unsigned char* src = (unsigned char*)&packet->sender;
    unsigned char* dst = (unsigned char*)&packet->receiver;
    printf("from %hhu.%hhu.%hhu.%hhu ", src[0], src[1], src[2], src[3]);
    printf("to %hhu.%hhu.%hhu.%hhu ", dst[0], dst[1], dst[2], dst[3]);
    switch(packet->type) {
        case BC_TYPE_START_TRANSACTION: printf("START_TRANSACTION"); break;
        case BC_TYPE_REQUEST_CONTRAST: printf("REQUEST_CONTRAST"); break;
        case BC_TYPE_REPLY_CONTRAST: printf("REPLY_CONTRAST"); break;
        case BC_TYPE_CONFIRM_CONTRAST: printf("CONFIRM_CONTRAST"); break;
        case BC_TYPE_TRANSACTION_SUCCESS: printf("TRANSACTION_SUCCESS"); break;
        case BC_TYPE_TRANSACTION_BOARDCAST: printf("TRANSACTION_BOARDCAST"); break;
        case BC_TYPE_REQUEST_INFO: printf("REQUEST_INFO"); break;
        case BC_TYPE_REPLY_INFO: printf("REPLY_INFO"); break;
        default: printf("UNKNOWN_TYPE");
    }
    printf(" amount: %d.%02d\n", packet->amount / 100, packet->amount % 100);
}
