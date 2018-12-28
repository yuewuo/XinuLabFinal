#include "blockchain.h"
#include "stdio.h"
#include "string.h"

char bc_ip[32];
char bc_linebuf[BLOCKCHAIN_LINEBUF_LEN];
int bc_linebuf_idx;

int bc_init(const char* ip) {
    bc_linebuf_idx = 0;
    int len = strlen(ip);
    if (len >= sizeof(bc_ip)) return -1;  // 长度不正常
    strcpy(bc_ip, ip);  // 保存地址字符串
    
    return 0;
}

int bc_input_char(char c) {
    putchar(c);
    if (c == '\n') {
        bc_handle_line();
        bc_linebuf_idx = 0;
    } else {
        if (bc_linebuf_idx < 0 || bc_linebuf_idx > BLOCKCHAIN_LINEBUF_LEN - 2)
            return BC_LINEBUF_OVERFLOW;  // buffer不够
        bc_linebuf[bc_linebuf_idx++] = c;
        bc_linebuf[bc_linebuf_idx] = '\0';
        putchar(c);  // 直接输出
    }
    return 0;
}

int bc_loop(int timeout) {

}

int bc_handle_line(void) {
    // TODO 解析bc_linebuf字符串
}

int bc_back(void) {
    for (int i = 0; i < bc_linebuf_idx; ++i) putchar('\b');
}

int bc_forward(void) {
    printf("%s\n", bc_linebuf);
}
