#include "blockchain.h"
#include "stdio.h"
#include "string.h"

char bc_ip[32];
char bc_linebuf[BLOCKCHAIN_LINEBUF_LEN];
int bc_linebuf_idx;
unsigned long long lasttime;

int bc_init(const char* ip) {
    bc_linebuf_idx = 0;
    bc_linebuf[0] = '\0';
    bc_forward();
    int len = strlen(ip);
    if (len >= sizeof(bc_ip)) return -1;  // 长度不正常
    strcpy(bc_ip, ip);  // 保存地址字符串
    lasttime = bc_gettime_ms();
    return 0;
}

int bc_input_char(char c) {
    // printf("0x%02X\n", c);
    if (c == '\n') {
        putchar(c);
        bc_handle_line();
        bc_linebuf_idx = 0;
        bc_linebuf[0] = '\0';
        bc_forward();
    } else if (c >= 0x21 && c <= 0x7E) {  // 可见字符区间
        if (bc_linebuf_idx < 0 || bc_linebuf_idx > BLOCKCHAIN_LINEBUF_LEN - 2)
            return BC_LINEBUF_OVERFLOW;  // buffer不够
        bc_linebuf[bc_linebuf_idx++] = c;
        bc_linebuf[bc_linebuf_idx] = '\0';
        putchar(c);  // 直接输出
    } else if (c == 0x7F) {  // backspace，删除一个字符
        if (bc_linebuf_idx <= 0) return 0;
        bc_linebuf[--bc_linebuf_idx] = '\0';
        putchar('\b'); putchar(' '); putchar('\b');
    } else if (c == 0x1B) {
        // TODO 实现光标左右移动编辑，同时也需要更改0x7F的逻辑
    } else if (c == 0x1A) {
        // TODO 同上
    }
    return 0;
}

int bc_loop() {
    int nowtime = bc_gettime_ms();
    if (nowtime - lasttime >= 1000) {  // 1s
        lasttime = nowtime;
        bc_println("hahahha");
    }
    return 0;
}

int bc_handle_line(void) {
    // TODO 解析bc_linebuf字符串
    // printf("\b\b");
    return 0;
}

int bc_back(void) {
    for (int i = 0; i < bc_linebuf_idx + 2; ++i) printf("\b \b");
}

int bc_forward(void) {
    printf("> %s", bc_linebuf);
}
