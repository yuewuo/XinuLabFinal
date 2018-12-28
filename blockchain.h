#ifndef __BLOCKCHAIN_H
#define __BLOCKCHAIN_H

#include "bc_packet.h"

extern unsigned int bc_ip;
extern unsigned int bc_amount;
#define BLOCKCHAIN_PORT 1024
#define BLOCKCHAIN_LINEBUF_LEN 128
#define BLOCKCHAIN_MAX_TRANSACTION 16  // 同时进行的最多的transaction个数
#define BLOCKCHAIN_OTHERS_TIMEOUT 1000  // 和其他人的交易，从建立之初，多少毫秒后交易失效
#define BLOCKCHAIN_INIT_MONEY 10000  // 100元

// 错误码定义
#define BC_LINEBUF_OVERFLOW -1
#define BC_WANT_EXIT -2
#define BC_UNKNOWN_CMD -3
#define BC_PACKET_DECODE_ERROR -4
#define BC_SELF_BUSY -5
#define BC_BAD_PACKET -6

// this is blockchain.c implemented
int bc_init(unsigned int ip);
int bc_loop(void);  // call this like `while(bc_loop());`, this will check UDP packet and timeout
int bc_input_char(char c);
int bc_exit();
int bc_input_packet(const char* buf, unsigned int len, unsigned int remip, unsigned short remport);
#define bc_println(format, ...) do {bc_back();printf( format "\n", ##__VA_ARGS__);bc_forward();} while(0)

// need to be implemented depends on platform
unsigned long long bc_gettime_ms(void);
int udp_sendpacket(char* buf, unsigned int length, unsigned int remip, unsigned short remport);

// 内部函数
int bc_handle_line(void);
int bc_back(void);
int bc_forward(void);

// 状态机定义
typedef struct {  // 仅作为交易发起方
#define SELF_STATUS_IDLE 1
#define SELF_STATUS_WAIT_FINISH 2
    unsigned char status;
    unsigned int receiver;
    unsigned int amount;
} fsm_self_t;
extern fsm_self_t fsm_self;

typedef struct {
    unsigned char status;
    short next;  // list point to next
    short prev;
    unsigned long long createtime;
    unsigned int sender;
    unsigned int receiver;
    unsigned int amount;
} fsm_other_t;
extern fsm_other_t fsm_others[BLOCKCHAIN_MAX_TRANSACTION];
extern unsigned int fsm_others_idle_cnt;
extern short fsm_others_idle;
extern short fsm_others_busy;
void fsm_busy2idle(int idx);
void fsm_idle2busy(int idx);

#endif
