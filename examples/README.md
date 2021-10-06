# MPDTP 测试代码使用说明

MPDTP 的测试代码目前均使用 C 语言设计，并且整体的结构以及使用方法和 AItransDTP 上的测试系统略有不同，需要加以区别。


在这里，我们称使用 `server.c` 与 `client.c` 的程序为“一般测试程序”，使用 `server-libuv` 与 `client-libuv` 的程序为“特殊测试程序”，他们的主要区别如下：

1. 一般测试程序使用 `libev` 作为事件循环库，而特殊测试程序使用 `libuv`
2. 一般测试程序会在客户端建立两条连接到同一个服务端的端口（二打一），但是特殊测试程序在服务端和客户端之间建立了两条完全不同的连接（二打二）

## 编译方式

一般测试程序：

在 example 目录下运行 `make` 即可获得服务端和测试端的可执行程序。编译环境与当前系统相同，可能需要 `libev` 与 `uthash` 两个额外的库（其中后者已经在仓库中给出）。

特殊测试程序：

编译方法是运行 `make server-libuv` 和 `make client-libuv` 分别得到服务端和客户端的程序进行测试。

## 测试方法——一般测试程序

### 服务端程序使用方法

服务端程序需要给定：

1. 服务端 IP 地址
2. 服务端端口号 （port）
3. 服务端测试数据文件相对位置

运行的方法可以参考下面的命令：

```bash
./server 127.0.0.1 5555 test.txt
```

#### 输入数据格式

服务端输入一个参数读如测试数据文件的相对位置。该文件用于描述测试用的数据块的 deadline, priority 以及数据块大小的信息。该文件的格式如下：

```txt
10
200 1 5000
500 2 1300
...
```

其中第一个整数代表测试用块的个数，之后的每一行的整数从左到右分别代表：截止时间、优先级以及块的大小。所有超过第一个整数描述的块的个数的行均会被忽略。

该解析功能由 `./dtp_config.h:parse_dtp_config` 实现。

### 客户端程序使用方法

客户端程序除了需要指定服务端的 IP 与端口之外，还需要指定两个客户端的 IP 与端口。共计六个参数：

1. 服务端 IP 
2. 服务端端口
3. 客户端 IP 1
4. 客户端端口 1
5. 客户端 IP 2
6. 客户端端口 2

运行的方法可以参考下面的命令：

```bash
./client 127.0.0.1 5555 127.0.0.1 5556 127.0.0.1 5557
```

### 测试方法

1. 启动服务端程序
2. 启动客户端程序
3. 两个程序均没有报错，而且命令行显示两个程序均输出若干 log 后停止运行即为运行正确。

## TODO: 测试方法——特殊测试程序

特殊测试程序和一般测试程序的结构一致，都是使用服务端和客户端连接的方式进行测试。但是因为特殊测试程序需要建立两条连接，所需要的参数并不相同。

注：在 Ubuntu 20.04 上特殊测试程序暂时无法正确运行，原因暂且未知。

### 服务端使用方法

使用类似下面的命令运行：

```bash
./server-libuv 127.0.0.1 5555 127.0.0.1 5557 test.txt
```

参数含义：

1. 服务端的 IP 1
2. 服务端的端口 1
3. 服务端的 IP 2
4. 服务端的端口 2
5. 服务端测试数据文件相对位置

### 客户端使用方法

使用类似下面的命令运行：

```bash
./client-libuv 127.0.0.1 5555 127.0.0.1 5556 127.0.0.1 5557 127.0.0.1 5558
```

参数含义：

1. 服务端的 IP 1
2. 服务端的端口 1
3. 客户端的 IP 1
4. 客户端的端口 1
5. 服务端的 IP 2
6. 服务端的端口 2
7. 客户端的 IP 2
8. 客户端的端口 2

How to build C examples
-----------------------

### Requirements

You will need the following libraries to build the C examples in this directory.
You can use your OS package manager (brew, apt, pkg, ...) or install them from
source.

- [libev](http://software.schmorp.de/pkg/libev.html)
- [uthash](https://troydhanson.github.io/uthash/)

### Build

Simply run `make` in this directory.

```
% make clean
% make
```

Examples Docker image
---------------------
You can experiment with [http3-client](http3-client.rs),
[http3-server](http3-server.rs), [client](client.rs) and [server](server.rs)
using Docker.

The Examples [Dockerfile](Dockerfile) builds a Debian image.

To build:

```
docker build -t cloudflare-quiche .
```

To make an HTTP/3 request:

```
docker run -it cloudflare-quiche http3-client https://cloudflare-quic.com
```
