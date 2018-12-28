#include "bc_packet.h"
#include "string.h"

int bc_packet(unsigned char type, unsigned int sender, unsigned int receiver, unsigned int amount, bc_packet_t* packet) {
    packet->padding = 0;
    packet->type = type;
    packet->version = BC_VERSION_1;
    packet->sender = sender;
    packet->receiver = receiver;
}

int bc_packet_parse(const char* msg, unsigned int length , bc_packet_t* packet) {
    if (length == sizeof(bc_packet_t) && msg[0] == BC_VERSION_1) {
        memcpy(packet, msg, sizeof(bc_packet_t));
        return 0;
    }
    // TODO 兼容其他实现，比如字符串？
    return -1;
}
