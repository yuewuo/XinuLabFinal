#include <stdio.h>
#include <ifaddrs.h>
#include <arpa/inet.h>
#include <string.h>
#include "blockchain.h"
#include <stdio.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <string.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <termios.h>
#include <fcntl.h>
#include <time.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <sys/types.h>

int getselfIP(char* ip, int len);  // 返回第一个不是127.0.0.1的IP地址
void setecho();  // 关闭回显。XINU默认不回显
void recoverecho();  // 恢复回显
void my_sigint(int signo);
unsigned char loop_run;

int main() {
    int sock_fd;
    struct sockaddr_in client_addr;
    struct sockaddr_in server_addr;
    int rcv_num = -1;
    int client_len;
    char rcv_buff[512];

    char selfip[32];
    if (getselfIP(selfip, sizeof(selfip)) != 0) {
        perror("get self IP error\n");
        exit(1);
    }
    printf("self IP is %s\n", selfip);

    if ((sock_fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
    {
        perror("socket create error\n");
        exit(1);
    }

    struct timeval timeout = { 0, 30000 };  // 30ms
    if (setsockopt(sock_fd, SOL_SOCKET,SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout))) {
        perror("set timeout failed\n");
        exit(1);
    }
    
    memset(&server_addr, 0, sizeof(struct sockaddr_in));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(BLOCKCHAIN_PORT);
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(sock_fd, (struct sockaddr *)&server_addr, sizeof(struct sockaddr_in)) < 0)
    {
        perror("bind error.\n");
        exit(1);
    }

    bc_init(selfip);  // 初始化库函数
    signal(SIGINT, my_sigint); 

    setecho();  // 取消linux自带终端IO控制，自定义控制
    loop_run = 1;
    while (loop_run) {

        rcv_num = recvfrom(sock_fd, rcv_buff, sizeof(rcv_buff), 0, (struct sockaddr*)&client_addr, &client_len);
        if (rcv_num > 0) {
            rcv_buff[rcv_num] = '\0';
            printf("%s %u says: %s\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port), rcv_buff);
        } else if (rcv_buff) {
            
        } else {
            perror("recv error\n");
            break;
        }
        bc_loop();

        char c = getchar();
        while (c != EOF) {
            int ret = bc_input_char(c);
            if (ret == BC_WANT_EXIT) { loop_run = 0; break; }
            c = getchar();
        }
    }
    recoverecho();
    bc_exit();
    close(sock_fd);

    printf("stop normally\n");
    return 0;
}

void my_sigint(int signo) {
    loop_run = 0;
}

int getselfIP(char* ip, int len) {
    struct ifaddrs * ifAddrStruct = NULL;
    if (getifaddrs(&ifAddrStruct) != 0) return -1;
    struct ifaddrs * iter = ifAddrStruct;
    in_addr_t tmpip;
    while (iter != NULL) {
        if (iter->ifa_addr->sa_family == AF_INET) { //if ip4
            tmpip = ((struct sockaddr_in *)iter->ifa_addr)->sin_addr.s_addr;
            if (tmpip != 0x0100007F) {  // 不是127.0.0.1
                // printf("0x%08X\n", tmpip);
                inet_ntop(AF_INET, &tmpip, ip, len);
                return 0;
            }
        }
        iter = iter->ifa_next;
    }
}

#define ECHOFLAGS (ECHO | ECHOE | ECHOK | ECHONL | ICANON) 
struct termios oldterm;
void recoverecho() {
    if(tcsetattr(fileno(stdin), TCSANOW, &oldterm) != 0) {
        perror("tcsetattr failed\n");
        exit(-6);
    }
    int flags = fcntl(fileno(stdin), F_GETFL, 0);
    flags &= ~O_NONBLOCK;
    fcntl(fileno(stdin), F_SETFL, flags);
}
void setecho() {
    struct termios term;
    if(tcgetattr(fileno(stdin), &term) != 0){  
        perror("Cannot get the attribution of the terminal");  
        exit(-5);
    }
    oldterm = term;
    term.c_lflag &= ~ECHOFLAGS;
    term.c_cc[VMIN] = 1;  // VMIN等待最小的字符数
    term.c_cc[VTIME] = 0;  // 等待的最小时间
    if(tcsetattr(fileno(stdin), TCSANOW, &term) != 0) {
        perror("tcsetattr failed\n");
        exit(-6);
    }
    int flags = fcntl(fileno(stdin), F_GETFL, 0);
    flags |= O_NONBLOCK;
    fcntl(fileno(stdin), F_SETFL, flags);  // 配置为非阻塞模式
}

unsigned long long bc_gettime_ms(void) {
    struct timeval stuTimeVal;
    memset(&stuTimeVal, 0, sizeof(struct timeval));
    int ret = gettimeofday(&stuTimeVal,NULL);
    if (ret == 0) {
        return stuTimeVal.tv_sec * 1000 + stuTimeVal.tv_usec / 1000;
    }
    return 0;  // failed, always return 0
}
