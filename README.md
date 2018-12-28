# XinuLabFinal

XINU综合实验——区块链小游戏

## 设计

为了保证程序的可调试性，在`test`文件夹下面包含一份运行在Linux上面的代码，通过建立namespace模拟多个设备，进行通信。

这部分是程序的接口，当port到XINU上运行时需要仿照实现相应功能

```c
void bc_user_input(char c);  // 当用户输入时调用这个函数
int bc_listen_udp(unsigned short port);
```
