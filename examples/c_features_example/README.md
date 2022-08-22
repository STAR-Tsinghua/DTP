# Features example

该样例程序依赖以下两个库，你可以通过后面类似后面给出的指令进行安装，或是从源码进行构建。

1. libev: `sudo apt install libev-dev`
2. uthash: `sudo apt install uthash-dev`

## 编译运行

- 配置好 DTP 编译环境，使用 `make` 进行编译。
  - 该样例开启了 `boringssl-vendored interface fec` 三个 feature。后两个为新增特性。
  - 为了使用 `interface` 特性，你可以参考`solutions`目录下的文件来编译对应的库文件。编译命令则可参考 Makefile 文件
  - 为了使用 `fec` 特性，你需要在数据的发送端配置 `tail_size` 和 `redundancy rate` 两个参数。你可以在 client 端找到对应的配置命令。
- 运行 server：

```bash
./server 127.0.0.1 5555 2> server.log
```

- 运行 client：

```bash
./client 127.0.0.1 5555 2> client.log
```
- 服务端显示

服务端应该会有如下的几个显示内容：

1. 接口函数 debug 信息
   
   所有以 "Solution" 开头的信息表示正在被调用的接口函数。应该有如下的几个：

   1. SolutionInit: 代表初始化用的接口函数
   2. SolutionShouldDropBlock: 代表判断是否丢弃块的函数
   3. SolutionSelectBlock: 代表选择块发送的函数
   4. SolutionAckRatio: 代表修改 ACK 发送频率的函数

2. 接收到 hello 和随机字符串，显示类似下例

```bash
recv: hello
recv: 1281 string
```

3. 发送挥手字符串

```bash
send: byez
```

- 客户端显示

客户端显示与服务端类似，分为三部分

1. 接口函数 debug 信息
   
   所有以 "Solution" 开头的信息表示正在被调用的接口函数。应该有如下的几个：

   1. SolutionInit: 代表初始化用的接口函数
   2. SolutionShouldDropBlock: 代表判断是否丢弃块的函数
   3. SolutionSelectBlock: 代表选择块发送的函数
   4. SolutionAckRatio: 代表修改 ACK 发送频率的函数
   5. SolutionCcTrigger: 代表实现拥塞控制算法的函数
   6. redundancy func: 代表实现动态冗余算法的函数

2. 发送 hello 和随机字符串，显示类似下例

```bash
send: hello
send string, size: 10000
```

3. 接收到服务端的挥手字符串

```bash
recv: byez
recv: byez
```
