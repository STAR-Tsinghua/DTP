# Ping-Pong example

该样例程序依赖以下两个库，你可以通过后面类似后面给出的指令进行安装，或是从源码进行构建。

1. libev: `sudo apt install libev-dev`
2. uthash: `sudo apt install uthash-dev`

## 编译运行

- 配置好 DTP 编译环境，使用 `make` 进行编译。
- 运行 server：

```bash
./server 127.0.0.1 5555 2> server.log
```

- 运行 client：

```bash
./client 127.0.0.1 5555 2> client.log
```
- 服务端显示

```bash
recv: hello
send: byez
```

- 客户端显示

```bash
send: hello
recv: byez
```

- 可尝试将例程中的 quiche_conn_stream_send 函数替换为 quiche_conn_stream_send_full 函数（需指定deadline 和 priority），重新编译运行。
