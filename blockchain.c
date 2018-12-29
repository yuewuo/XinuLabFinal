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
unsigned int bc_random_ms;
char bc_cmdrecord[BLOCKCHAIN_CURSOR_RECORD_SIZE][BLOCKCHAIN_LINEBUF_LEN];
unsigned int bc_cmdrecord_idx;
char bc_cmdrecord_current[BLOCKCHAIN_LINEBUF_LEN];
unsigned char bc_escape_char;

#define bc_warning() printf("\033[1;33mwarning:\033[0m ")
#define bc_error() printf("\033[1;31merror:\033[0m ")
#define bc_info() printf("\033[1;34minfo:\033[0m ")
#define bc_debug() printf("\033[1;36mdebug:\033[0m ")
#define bc_success() printf("\033[1;32msuccess:\033[0m ")

int bc_10per(unsigned int money) {
    if (money < 10) return 1;  // 无论如何矿机至少拿到1分钱
    return money / 10;
}
#define bc_90per(money) (money - bc_10per(money))
int bc_delta_peer(unsigned int remip, int delta);

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
    bc_peer_cnt = 0;
    bc_amount = amount;
    bc_random_ms = BLOCKCHAIN_INIT_RANDOM_DELAY;
    bc_cmdrecord_idx = 0;
    bc_escape_char = 0;
    for (int i=0; i<BLOCKCHAIN_CURSOR_RECORD_SIZE; ++i) bc_cmdrecord[i][0] = '\0';
    // 向其他机器广播自己的消息，并接收回复
    bc_packet_t packet;
    bc_packet(BC_TYPE_REQUEST_INFO, bc_ip, BC_BROADCAST, bc_amount, &packet);
    unsigned int packet_len;
    bc_packet_send(bc_packetbuf, &packet_len, &packet);
    udp_sendpacket(bc_packetbuf, packet_len, BC_BROADCAST, BLOCKCHAIN_PORT);
    return 0;
}

int bc_input_char(char c) {
    // printf("0x%02X\n", c);
    int ret = 0;
    if (c == '\n') {
        putchar(c);
        if (strcmp(bc_linebuf, bc_cmdrecord[1]) && bc_linebuf[0]) {  // 和上一条命令不同才记录进去，不为空命令才记录进去
            strcpy(bc_cmdrecord[0], bc_linebuf);
            for (unsigned int i=BLOCKCHAIN_CURSOR_RECORD_SIZE-1; i>0; --i) {
                strcpy(bc_cmdrecord[i], bc_cmdrecord[i-1]);  // 滚动复制，效率低，不过无所谓
            }
        }
        bc_cmdrecord_idx = 0;
        ret = bc_handle_line();
        bc_linebuf_idx = 0;
        bc_linebuf[0] = '\0';
        bc_forward();
    } else if (c == 0x1B) {
        bc_escape_char = 2;
    } else if (bc_escape_char == 2 && c == '[') {
        bc_escape_char = 1;
    } else if (bc_escape_char == 1) {  // escape字符
        bc_escape_char = 0;
        if (c == 'A') {  // 上箭头
            if (bc_cmdrecord_idx + 1 < BLOCKCHAIN_CURSOR_RECORD_SIZE && bc_cmdrecord[bc_cmdrecord_idx + 1][0] != '\0') {
                if (bc_cmdrecord_idx == 0) {  // copy进入bc_cmdrecord[0]
                    strcpy(bc_cmdrecord[0], bc_linebuf);
                }
                bc_back();
                bc_cmdrecord_idx += 1;
                bc_linebuf_idx = strlen(strcpy(bc_linebuf, bc_cmdrecord[bc_cmdrecord_idx]));
                bc_forward();
            }
        } else if (c == 'B') {  // 下箭头
            if (bc_cmdrecord_idx > 0) {
                bc_back();
                bc_cmdrecord_idx -= 1;
                bc_linebuf_idx = strlen(strcpy(bc_linebuf, bc_cmdrecord[bc_cmdrecord_idx]));
                bc_forward();
            }
        }
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
    } else if (c == 0x19) {  // 下光标

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
        if (nowtime - fsm_others[busy].createtime > BLOCKCHAIN_OTHERS_TIMEOUT) {  // timeout
            bc_back();
            bc_warning(); printf("transaction from ");
            bc_printip(fsm_others[busy].sender); printf(" to ");
            bc_printip(fsm_others[busy].receiver); printf(" of $");
            bc_printamount(fsm_others[busy].amount); printf(" timeout, drop it\n");
            bc_forward();
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
        } else if (EQUAL_CMD_PARA(5, "peer")) {  // 打印已知的peer的信息
            printf("got %u peers recorded:\n", bc_peer_cnt);
            for (unsigned int i=0; i<bc_peer_cnt; ++i) {
                printf("    "); bc_printip(bc_peers[i].ip); printf(": \033[1;34m$"); 
                bc_printamount(bc_peers[i].money); printf("\033[0m\n");
            }
        } else if (EQUAL_CMD_PARA(5, "delay")) {
            printf("delay is %u\n", bc_random_ms);
        } else if (EQUAL_CMD_PARA(5, "fsm")) {
            printf("fsm idle remains %u out of %u, the busy ones are\n", fsm_others_idle_cnt, BLOCKCHAIN_MAX_TRANSACTION);
            unsigned int busy = fsm_others_busy;
            while (busy != -1) {
                printf("    %hu: status(%hhu): sender(", busy, fsm_others[busy].status); bc_printip(fsm_others[busy].sender); printf("), receiver(");
                bc_printip(fsm_others[busy].receiver); printf("), amount $"); bc_printamount(fsm_others[busy].amount); putchar('\n');
                busy = fsm_others[busy].next;
            }
        } else if (EQUAL_CMD_PARA(5, "cmd")) {
            printf("cmd records are:\n");
            for (int i=1; i<BLOCKCHAIN_CURSOR_RECORD_SIZE; ++i) {
                if (bc_cmdrecord[i][0] == '\0') break;  // 发现一条0记录则终止
                printf("    -%d: \"%s\"\n", i, bc_cmdrecord[i]);
            }
        } else {
            bc_error(); printf("don't known what to show, see \"help\"\n");
        }
    } else if (iS_COMMAND("set ")) {
        if (strncmp(bc_linebuf + 4, "delay ", strlen("delay ")) == 0) {
            sscanf(bc_linebuf + 10, "%u", &bc_random_ms);
            printf("set delay to %u success\n", bc_random_ms);
        } else {
            bc_error(); printf("don't known what to set, see \"help\"\n");
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
        bc_debug(); printf("ip = 0x%08X (big endian), amount = ", ip);
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
            bc_error(); printf("nothing to cancel\n");
        } else {
            unsigned char* ptr = (unsigned char*)&fsm_self.receiver;
            bc_warning(); printf("you canceled transaction with ");
            bc_printip(fsm_self.receiver); printf(", $");
            bc_printamount(fsm_self.amount); printf(", with high risk that others still working on this transaction.\n");
            fsm_self.status = SELF_STATUS_IDLE;
        }
    } else if (EQUAL_CMD("exit")) {
        return BC_WANT_EXIT;
    } else if (EQUAL_CMD("help")) {
        printf("blockchain command instruction:\n");
        printf("    show ip: print system ip\n");
        printf("    show peer: print found peers\n");
        printf("    show delay: print max random delay when receiving contrast request\n");
        printf("    set delay <delay> set max random delay when receiving contrast request, 0 for no delay\n");
        printf("    show fsm: print asynochronized waiting events\n");
    } else {
        bc_error(); printf("unknown command, try \"help\"\n");
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


int bc_update_peer(unsigned int remip, unsigned int amount);
int bc_handle_packet(const char* buf, unsigned int len, unsigned int remip) {
    unsigned long long nowtime = bc_gettime_ms();
    bc_packet_t packet;
    int ret = bc_packet_parse(buf, len, &packet);
    // printf("ret = %d\n", ret);
    if (ret != 0) return BC_PACKET_DECODE_ERROR;
    bc_debug(); bc_packet_print(&packet); printf(" [\033[1;30mremip:"); bc_printip(remip); printf("\033[0m]\n");
    if (packet.type == BC_TYPE_START_TRANSACTION) {
        if (remip != packet.sender) {
            bc_warning(); bc_printip(remip); printf(" want to start a transaction with sender ip = ");
            bc_printip(packet.sender); printf(", which might be a malicious attack! drop it\n");
            return BC_BAD_PACKET;
        }
        if (packet.receiver != bc_ip) {
            bc_warning(); bc_printip(remip); printf(" want to start a transaction with ");
            bc_printip(packet.sender); printf(", but why send to me? ("); bc_printip(bc_ip); printf(")\n");
            return BC_BAD_PACKET;
        }
        int idx = fsm_getidle();
        printf("fsm_others_busy = %hu\n", fsm_others_busy);
        if (idx < 0) {
            printf("warning: "); printf("fsm buffer is full, try recompile with larger buffer size\n");
            return BC_FSM_FULL;
        }
        bc_packet_t packet_send;
        bc_packet(BC_TYPE_REQUEST_CONTRAST, packet.sender, packet.receiver, packet.amount, &packet_send);
        unsigned int packet_len;
        bc_packet_send(bc_packetbuf, &packet_len, &packet_send);
        udp_sendpacket(bc_packetbuf, packet_len, BC_BROADCAST, BLOCKCHAIN_PORT);
        fsm_others[idx].status = FSM_STATUS_WAIT_REPLY_CONTRAST;  // 等待矿机回应
        fsm_others[idx].sender = remip;
        fsm_others[idx].receiver = bc_ip;
        fsm_others[idx].createtime = nowtime;
        fsm_others[idx].amount = packet.amount;
        bc_info(); bc_printip(remip); printf(" try to send $"); bc_printamount(packet.amount); printf(" to me, request contrast and wait\n");
    } else if (packet.type == BC_TYPE_REQUEST_CONTRAST) {  // 别的交易请求一台矿机介入
        if (packet.receiver == bc_ip) return 0;  // 自己发的包，不管
        if (packet.sender == bc_ip) return 0;  // 交易的发起者是自己，不管
        if (remip != packet.receiver) {
            bc_warning(); bc_printip(remip); printf(" try to request contrast but transaction receiver ");
            bc_printip(packet.receiver); printf(" is not himself, which might be a malicious attack! drop it\n");
            return BC_BAD_PACKET;
        }
        int idx = fsm_getidle();
        if (idx < 0) {
            printf("warning: "); printf("fsm buffer is full, try recompile with larger buffer size\n");
            return BC_FSM_FULL;
        }
        unsigned int ms2sleep = (bc_random_ms == 0) ? 0 : bc_random(bc_random_ms);
        bc_info(); printf("receive a contrast request from "); bc_printip(remip); printf(", delay %ums to reply...", ms2sleep);
        if (ms2sleep) bc_sleep_ms(ms2sleep);
        printf("done, send reply\n");
        bc_packet_t packet_send;
        bc_packet(BC_TYPE_REPLY_CONTRAST, packet.sender, packet.receiver, packet.amount, &packet_send);
        unsigned int packet_len;
        bc_packet_send(bc_packetbuf, &packet_len, &packet_send);
        udp_sendpacket(bc_packetbuf, packet_len, remip, BLOCKCHAIN_PORT);
        fsm_others[idx].status = FSM_STATUS_WAIT_CONFIRM_CONTRAST;  // 等待合同确认，不过因为竞争，很可能得不到确认，那么超时
        fsm_others[idx].sender = packet.sender;
        fsm_others[idx].receiver = packet.receiver;
        fsm_others[idx].createtime = nowtime;
        fsm_others[idx].amount = packet.amount;
        bc_info(); printf("send contrast reply to "); bc_printip(remip); printf(", transaction is $"); bc_printamount(packet.amount); 
        printf(" from "); bc_printip(packet.sender); printf(", wait for confirm\n");
    } else if (packet.type == BC_TYPE_REPLY_CONTRAST) {  // 矿机回复
        if (packet.sender == bc_ip || packet.receiver != bc_ip) {
            bc_warning(); printf("why reply contrast to me? Might be a malicious attack! drop it\n");
            return BC_BAD_PACKET;
        }
        if (packet.sender == remip) {
            bc_warning(); printf("you shouldn't reply contrast because you are the sender! drop it\n");
            return BC_BAD_PACKET;
        }
        // 在busy链表中选择寻找这个回复
        unsigned int busy = fsm_others_busy;
        while (busy != -1) {
            if (fsm_others[busy].status == FSM_STATUS_WAIT_REPLY_CONTRAST
                    && fsm_others[busy].sender == packet.sender && fsm_others[busy].receiver == packet.receiver) {
                break;
            }
            busy = fsm_others[busy].next;
        }
        if (busy == -1) return 0;  // 正常操作，因为很多矿机都会回复消息
        if (packet.amount != fsm_others[busy].amount) {
            bc_warning(); printf("transaction money is modified by "); bc_printip(remip); printf(" from $"); bc_printamount(fsm_others[busy].amount);
            printf(" to $"); bc_printamount(packet.amount); printf(", which might be a malicious attack! drop it\n");
            return BC_BAD_PACKET;
        }
        bc_info(); printf("transaction from "); bc_printip(packet.sender); printf(" to "); bc_printip(packet.receiver); printf(" of $");
        bc_printamount(packet.amount); printf(" is done with the help of "); bc_printip(remip); printf(", confirm it and wait for broadcast\n");
        // 向矿机发送confirm消息
        bc_packet_t packet_send;
        bc_packet(BC_TYPE_CONFIRM_CONTRAST, packet.sender, packet.receiver, packet.amount, &packet_send);
        unsigned int packet_len;
        bc_packet_send(bc_packetbuf, &packet_len, &packet_send);
        udp_sendpacket(bc_packetbuf, packet_len, remip, BLOCKCHAIN_PORT);
        fsm_others[busy].status = FSM_STATUS_WAIT_FINISH;  // FSM变为等待矿机发布广播
    } else if (packet.type == BC_TYPE_CONFIRM_CONTRAST) {
        if (packet.receiver != remip) {
            bc_warning(); bc_printip(remip); printf(" try to confirm contrast but transaction receiver ");
            bc_printip(packet.receiver); printf(" is not himself, which might be a malicious attack! drop it\n");
            return BC_BAD_PACKET;
        }
        // 在busy链表中选择寻找这个回复
        unsigned int busy = fsm_others_busy;
        while (busy != -1) {
            if (fsm_others[busy].status == FSM_STATUS_WAIT_CONFIRM_CONTRAST
                    && fsm_others[busy].sender == packet.sender && fsm_others[busy].receiver == packet.receiver) {
                break;
            }
            busy = fsm_others[busy].next;
        }
        if (busy == -1) {
            bc_warning(); bc_printip(remip); printf(" try to confirm contrast but cannot find the transaction in local table");
            printf(", which might be a malicious attack! drop it\n");
            return BC_BAD_PACKET;
        }
        if (packet.amount != fsm_others[busy].amount) {
            bc_warning(); printf("transaction money is modified by "); bc_printip(remip); printf(" from $"); bc_printamount(fsm_others[busy].amount);
            printf(" to $"); bc_printamount(packet.amount); printf(", which might be a malicious attack! drop it\n");
            return BC_BAD_PACKET;
        }
        bc_success(); printf("transaction from "); bc_printip(packet.sender); printf(" to "); bc_printip(packet.receiver); printf(" of $");
        bc_printamount(packet.amount); printf(" is handled by myself, broadcast it\n");
        // 全局广播交易完成
        bc_packet_t packet_send;
        bc_packet(BC_TYPE_TRANSACTION_BOARDCAST, packet.sender, packet.receiver, packet.amount, &packet_send);
        unsigned int packet_len;
        bc_packet_send(bc_packetbuf, &packet_len, &packet_send);
        udp_sendpacket(bc_packetbuf, packet_len, BC_BROADCAST, BLOCKCHAIN_PORT);
        fsm_busy2idle(busy);  // 完成了交易
        bc_amount += bc_10per(packet.amount);
    } else if (packet.type == BC_TYPE_TRANSACTION_BOARDCAST) {
        if (packet.sender == bc_ip) {  // 如果是自己发出的
            if (fsm_self.status == SELF_STATUS_WAIT_FINISH && fsm_self.receiver == packet.receiver && fsm_self.amount == packet.amount) {
                bc_amount -= packet.amount;
                bc_delta_peer(remip, bc_10per(packet.amount));
                bc_delta_peer(packet.receiver, bc_90per(packet.amount));
                bc_success(); printf("transaction to "); bc_printip(packet.receiver); printf(" of $"); bc_printamount(packet.amount);
                printf(" finished! now my account is $"); bc_printamount(bc_amount); printf("\n");
                fsm_self.status = SELF_STATUS_IDLE;
            } else {
                bc_warning(); bc_printip(remip); printf(" broadcast a transaction from myself to "); bc_printip(packet.receiver); printf(" of $");
                bc_printamount(packet.amount); printf(" but not true, which might be a malicious attack! drop it\n");
            }
        } else if (packet.receiver == bc_ip) {  // 是发给自己的
            unsigned int busy = fsm_others_busy;
            while (busy != -1) {
                if (fsm_others[busy].status == FSM_STATUS_WAIT_FINISH
                        && fsm_others[busy].sender == packet.sender && fsm_others[busy].receiver == packet.receiver) {
                    break;
                }
                busy = fsm_others[busy].next;
            }
            if (busy == -1) {
                bc_warning(); bc_printip(remip); printf(" try to broadcast transaction but cannot find that in local table");
                printf(", which might be a malicious attack! drop it\n");
                return BC_BAD_PACKET;
            }
            if (packet.amount != fsm_others[busy].amount) {
                bc_warning(); printf("transaction money is modified by "); bc_printip(remip); printf(" from $"); bc_printamount(fsm_others[busy].amount);
                printf(" to $"); bc_printamount(packet.amount); printf(", which might be a malicious attack! drop it\n");
                return BC_BAD_PACKET;
            }
            bc_amount += bc_90per(packet.amount);
            bc_delta_peer(remip, bc_10per(packet.amount));
            bc_delta_peer(packet.sender, -packet.amount);
            bc_success(); printf("transaction from "); bc_printip(packet.sender); printf(" of $"); bc_printamount(packet.amount);
            printf(" finished! now my account is $"); bc_printamount(bc_amount); printf("\n");
            fsm_busy2idle(busy);
        } else {  // 其他的消息，无从考证，但直接记录
            bc_success(); printf("transaction from "); bc_printip(packet.sender); printf(" to "); bc_printip(packet.receiver); printf(" of $"); 
            bc_printamount(packet.amount); printf(" finished but not checked"); printf("\n");
            // 更新三方的数据
            bc_delta_peer(remip, bc_10per(packet.amount));
            bc_delta_peer(packet.sender, -packet.amount);
            bc_delta_peer(packet.receiver, bc_90per(packet.amount));
        }
    } else if (packet.type == BC_TYPE_REQUEST_INFO) {  // 别的机器开机启动的时候会发送这个消息，获得别的主机的信息，回复之
        if (packet.sender == bc_ip) return 0;  // 自己发的包，不管
        if (remip != packet.sender) {
            bc_warning(); bc_printip(remip); printf(" try to request info declaring its IP as ");
            bc_printip(packet.sender); printf(", which might be a malicious attack! drop it\n");
            return BC_BAD_PACKET;
        }
        bc_update_peer(remip, packet.amount);
        bc_packet_t packet_send;
        bc_packet(BC_TYPE_REPLY_INFO, bc_ip, remip, bc_amount, &packet_send);
        unsigned int packet_len;
        bc_packet_send(bc_packetbuf, &packet_len, &packet_send);
        udp_sendpacket(bc_packetbuf, packet_len, remip, BLOCKCHAIN_PORT);
    } else if (packet.type == BC_TYPE_REPLY_INFO) {  // 别的机器回复信息，记录在表格里面
        if (remip != packet.sender) {
            bc_warning(); bc_printip(remip); printf(" try to reply info declaring its IP as ");
            bc_printip(packet.sender); printf(", which might be a malicious attack! drop it\n");
            return BC_BAD_PACKET;
        }
        // 添加到peers列表中
        bc_update_peer(remip, packet.amount);
    } else {
        bc_warning(); printf("receive not-implemented packet type\n");
    }
    return 0;
}

int bc_delta_peer(unsigned int remip, int delta) {
    unsigned int i=0;
    for (; i<bc_peer_cnt; ++i) {
        if (bc_peers[i].ip == remip) {
            bc_peers[i].money += delta;
            return 0;
        }
    }
    return -1;  // 没有找到这个人，不过OK，可能是没有实现peer搜寻协议的程序
}

int bc_update_peer(unsigned int remip, unsigned int amount) {
    // bc_debug(); printf("amount = %u\n", amount);
    unsigned int i=0;
    for (; i<bc_peer_cnt; ++i) {
        if (bc_peers[i].ip == remip) {
            if (bc_peers[i].money != amount) {
                bc_warning(); bc_printip(remip); printf(" change its money from recorded ");
                bc_printamount(bc_peers[i].money); printf(" to "); bc_printamount(amount);
                printf(", but still trust him? yes! trust him.\n");
                bc_peers[i].money = amount;
            }
            return 0;
        }
    }
    if (i == bc_peer_cnt) {
        bc_info(); bc_printip(remip); printf(" join the peers table, with initial money $");
        bc_printamount(amount); printf("\n");
        bc_peers[bc_peer_cnt].ip = remip;
        bc_peers[bc_peer_cnt].money = amount;
        ++bc_peer_cnt;
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
    if (next != -1) fsm_others[next].prev = prev;
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
    ++fsm_others_idle_cnt;
}

void fsm_idle2busy(int idx) {
    // 从idle链表中删除
    short next = fsm_others[idx].next;
    short prev = fsm_others[idx].prev;
    // printf("next: %hd, prev: %hd\n", next, prev);
    if (prev == -1) fsm_others_idle = next;
    else fsm_others[prev].next = next;
    if (next != -1) fsm_others[next].prev = prev;
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
    --fsm_others_idle_cnt;
}

int fsm_getidle(void) {
    if (fsm_others_idle_cnt == 0) return -1;
    int idx = fsm_others_idle;
    // printf("idx = %d\n", idx);
    fsm_idle2busy(idx);
    return idx;
}
