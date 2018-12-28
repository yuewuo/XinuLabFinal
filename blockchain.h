#ifndef __BLOCKCHAIN_H
#define __BLOCKCHAIN_H

extern char bc_ip[32];
#define BLOCKCHAIN_PORT 1024
#define BLOCKCHAIN_LINEBUF_LEN 128

// 错误码定义
#define BC_LINEBUF_OVERFLOW -1


// this is blockchain.c implemented
int bc_init(const char* ip);
int bc_loop(void);  // call this like `while(bc_loop());`, this will check UDP packet and timeout
int bc_input_char(char c);
#define bc_println(format, ...) do {bc_back();printf(format "\n", ##__VA_ARGS__);bc_forward();} while(0)

// need to be implemented depends on platform
void bc_user_input(char c);  // 当用户输入时调用这个函数
int bc_listen_udp(unsigned short port);
unsigned long long bc_gettime_ms(void);

// 内部函数
int bc_handle_line(void);
int bc_back(void);
int bc_forward(void);

#endif
