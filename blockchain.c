#include "blockchain.h"
#include "stdio.h"
#include "string.h"

unsigned int bc_ip;
unsigned int bc_amount;
char bc_linebuf[BLOCKCHAIN_LINEBUF_LEN];
int bc_linebuf_idx;
char bc_packetbuf[512];
unsigned long long lasttime;
fsm_self_t fsm_self;
short fsm_others_idle;
short fsm_others_busy;
unsigned int fsm_others_idle_cnt;
fsm_other_t fsm_others[BLOCKCHAIN_MAX_TRANSACTION];
bc_peer_t bc_peers[BLOCKCHAIN_MAX_PEER];

int bc_init(unsigned int ip, unsigned int amount) {
    fsm_self.status = SELF_STATUS_IDLE;
    bc_linebuf_idx = 0;
    bc_linebuf[0] = '\0';
    bc_forward();
    bc_ip = ip;
    lasttime = bc_gettime_ms();
    fsm_others_idle_cnt = BLOCKCHAIN_MAX_TRANSACTION;
    for (int i=0; i<BLOCKCHAIN_MAX_TRANSACTION; ++i) {
        fsm_others[i].next = i+1;  // 建立空闲链表
        fsm_others[i].prev = i-1;  // 0 => -1, it OK
    }
    fsm_others_busy = -1;
    fsm_others_idle = 0;
    bc_amount = amount;
    // 向其他机器广播自己的消息，并接收回复
    bc_packet_t packet;
    bc_packet(BC_TYPE_REQUEST_INFO, bc_ip, 0xFFFFFFFF, bc_amount, &packet);
    unsigned int packet_len;
    bc_packet_send(bc_packetbuf, &packet_len, &packet);
    udp_sendpacket(bc_packetbuf, packet_len, 0xFFFFFFFF, BLOCKCHAIN_PORT);
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
    // if (nowtime - lasttime >= 1000) {  // 1s
    //     lasttime = nowtime;
    //     bc_println("1 second passed, now time is %llums", nowtime);
    // }
    int busy = fsm_others_busy;
    while (busy >= 0) {
        int next = fsm_others[busy].next;  // 预先保存，之后可能修改
        fsm_other_t* optr;
        if (nowtime - optr->createtime > BLOCKCHAIN_OTHERS_TIMEOUT) {  // timeout
            printf("timeout: transaction from ");
            bc_printip(fsm_others[busy].sender); printf(" to ");
            bc_printip(fsm_others[busy].receiver); printf(" with $");
            bc_printamount(fsm_others[busy].amount); printf(", drop it\n");
            fsm_busy2idle(busy);
        }
        busy = next;
    }
    return 0;
}

#define iS_COMMAND(cmd) (strncmp(bc_linebuf, cmd, strlen(cmd)) == 0)
#define EQUAL_CMD(cmd) (strcmp(bc_linebuf, cmd) == 0)
#define EQUAL_CMD_PARA(bias, cmd) (strcmp(bc_linebuf + bias, cmd) == 0)
int bc_handle_line(void) {
    if (iS_COMMAND("show ")) {
        if (EQUAL_CMD_PARA(5, "ip")) {
            printf("ip: "); bc_printip(bc_ip); putchar('\n');
        } else {
            printf("error: don't known what to show, see \"help\"\n");
        }
    } else if (iS_COMMAND("send ")) {
        if (fsm_self.status != SELF_STATUS_IDLE) {  // 正在处理别的事务，不能发送
            printf("busy: handling one transaction now. stop it using \"cancel\"\n");
            return BC_SELF_BUSY;
        }
        unsigned int ip;
        unsigned int amount;
        float amountf;
        unsigned char* ptr = (unsigned char*)&ip;
        sscanf(bc_linebuf + 5, "%hhu.%hhu.%hhu.%hhu %f\n", ptr, ptr+1, ptr+2, ptr+3, &amountf);  // 网络字节序，大端法
        amount = amountf * 100;
        printf("ip = 0x%08X (big endian), amount = ", ip);
        bc_printamount(amount); putchar('\n');
        fsm_self.receiver = ip;
        fsm_self.amount = amount;
        bc_packet_t packet;
        bc_packet(BC_TYPE_START_TRANSACTION, bc_ip, ip, amount, &packet);
        unsigned int packet_len;
        bc_packet_send(bc_packetbuf, &packet_len, &packet);
        udp_sendpacket(bc_packetbuf, packet_len, ip, BLOCKCHAIN_PORT);
        fsm_self.status = SELF_STATUS_WAIT_FINISH;
    } else if (EQUAL_CMD("cancel")) {
        if (fsm_self.status != SELF_STATUS_WAIT_FINISH) {
            printf("error: nothing to cancel\n");
        } else {
            unsigned char* ptr = (unsigned char*)&fsm_self.receiver;
            printf("success: you canceled transaction with ");
            bc_printip(fsm_self.receiver); printf(", $");
            bc_printamount(fsm_self.amount); putchar('\n');
            fsm_self.status = SELF_STATUS_IDLE;
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

int bc_handle_packet(const char* buf, unsigned int len, unsigned int remip) {
    bc_packet_t packet;
    int ret = bc_packet_parse(buf, len, &packet);
    // printf("ret = %d\n", ret);
    if (ret != 0) return BC_PACKET_DECODE_ERROR;
    bc_packet_print(&packet);
    if (packet.type == BC_TYPE_START_TRANSACTION) {
        if (remip != packet.sender) {
            printf("warning: "); bc_printip(remip); printf(" want to start a transaction with sender ip = ");
            bc_printip(packet.sender); printf(" , which might be a malicious attack! drop it\n");
            return BC_BAD_PACKET;
        }
    } else if (packet.type == BC_TYPE_REQUEST_INFO) {  // 别的机器开机启动的时候会发送这个消息，获得别的主机的信息，回复之
        if (packet.sender == bc_ip) return 0;  // 自己发的包，不管
        if (remip != packet.sender) {
            printf("warning: "); bc_printip(remip); printf(" try to request info declaring its IP as ");
            bc_printip(packet.sender); printf(" , which might be a malicious attack! drop it\n");
            return BC_BAD_PACKET;
        }
        bc_packet_t packet;
        bc_packet(BC_TYPE_REPLY_INFO, bc_ip, remip, bc_amount, &packet);
        unsigned int packet_len;
        bc_packet_send(bc_packetbuf, &packet_len, &packet);
        udp_sendpacket(bc_packetbuf, packet_len, remip, BLOCKCHAIN_PORT);
    } else if (packet.type == BC_TYPE_REPLY_INFO) {  // 别的机器回复信息，记录在表格里面
        if (remip != packet.sender) {
            printf("warning: "); bc_printip(remip); printf(" try to reply info declaring its IP as ");
            bc_printip(packet.sender); printf(" , which might be a malicious attack! drop it\n");
            return BC_BAD_PACKET;
        }
        // bc_packet_print(&packet);
    }
    return 0;
}

int bc_input_packet(const char* buf, unsigned int len, unsigned int remip, unsigned short remport) {
    bc_back();
    bc_handle_packet(buf, len, remip);
    bc_forward();
}

void fsm_busy2idle(int idx) {
    // 从busy链表中删除
    short next = fsm_others[idx].next;
    short prev = fsm_others[idx].prev;
    if (prev == -1) fsm_others_busy = next;
    else fsm_others[prev].next = next;
    if (next != -1) fsm_others[prev].prev = prev;
    // 插入idle链表头部
    if (fsm_others_idle == -1) {
        fsm_others_idle = idx;
        fsm_others[idx].prev = -1;
        fsm_others[idx].next = -1;
    } else {
        fsm_others[fsm_others_idle].prev = idx;
        fsm_others[idx].next = fsm_others_idle;
        fsm_others[idx].prev = -1;
        fsm_others_idle = idx;
    }
}

void fsm_idle2busy(int idx) {
    // 从idle链表中删除
    short next = fsm_others[idx].next;
    short prev = fsm_others[idx].prev;
    if (prev == -1) fsm_others_idle = next;
    else fsm_others[prev].next = next;
    if (next != -1) fsm_others[prev].prev = prev;
    // 插入busy链表头部
    if (fsm_others_busy == -1) {
        fsm_others_busy = idx;
        fsm_others[idx].prev = -1;
        fsm_others[idx].next = -1;
    } else {
        fsm_others[fsm_others_busy].prev = idx;
        fsm_others[idx].next = fsm_others_busy;
        fsm_others[idx].prev = -1;
        fsm_others_busy = idx;
    }
}
