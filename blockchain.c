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
    int ret = 0;
    if (c == '\n') {
        putchar(c);
        ret = bc_handle_line();
        bc_linebuf_idx = 0;
        bc_linebuf[0] = '\0';
        bc_forward();
    } else if (c >= 0x21 && c <= 0x7E || c == ' ') {  // 可见字符区间
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
    return ret;
}

int bc_loop() {
    unsigned long long nowtime = bc_gettime_ms();
    if (nowtime - lasttime >= 1000) {  // 1s
        lasttime = nowtime;
        bc_println("1 second passed, now time is %llums", nowtime);
    }
    return 0;
}

#define iS_COMMAND(cmd) (strncmp(bc_linebuf, cmd, strlen(cmd)) == 0)
#define EQUAL_CMD(cmd) (strcmp(bc_linebuf, cmd) == 0)
#define EQUAL_CMD_PARA(bias, cmd) (strcmp(bc_linebuf + bias, cmd) == 0)
int bc_handle_line(void) {
    if (iS_COMMAND("show ")) {
        if (EQUAL_CMD_PARA(5, "ip")) {
            printf("ip: %s\n", bc_ip);
        } else {
            printf("error: don't known what to show, see \"help\"\n");
        }
    } else if (EQUAL_CMD("exit")) {
        return BC_WANT_EXIT;
    } else if (EQUAL_CMD("help")) {
        printf("blockchain command instruction:\n");
        printf("    show [ip]: print system ip\n");
    } else {
        printf("error: unknown command, try \"help\"\n");
    }
    return 0;
}

int bc_back(void) {
    for (int i = 0; i < bc_linebuf_idx + 2; ++i) printf("\b \b");
}

int bc_forward(void) {
    printf("> %s", bc_linebuf);
}

int bc_exit() {
    printf("\nblock chain exit...\n");
}
