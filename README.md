# XinuLabFinal

XINU综合实验——区块链小游戏

## 设计

为了保证程序的可调试性，在`test`文件夹下面包含一份运行在Linux上面的代码，通过建立namespace模拟多个设备，进行通信。

这部分是程序的接口，当port到XINU上运行时需要仿照实现相应功能

```c
void bc_user_input(char c);  // 当用户输入时调用这个函数
int bc_listen_udp(unsigned short port);
```

## 测试

最终大作业的形式应该是所有Galileo板子桥接到一起，通过一个路由器分配IP。在Linux环境下调试的时候，可以通过namespace建立独立的网络栈，**手动**分配IP地址，然后通过虚拟网桥连接到一起。

为了方便，本程序提供一个python脚本用来设置虚拟网络环境，建议在虚拟机环境下使用，它会删除已有的所有虚拟网络，如果遇到问题可以重启解决。

默认生成的网络为4个namespace，你可以通过如下命令在这个namespace里执行程序

```shell
sudo ip netns exec <namespace> <command>
```
