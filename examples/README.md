# TODO: DTP server/client 样例代码

## 开发辅助脚本

在 `examples/` 目录下有 Makefile 文件，其中提供了若干指令：

* `make server`: 可以生成 server 可执行文件，该可执行文件将生成**没有接口**的测试用服务端程序
* `make client`: 生成 client 可执行文件
* `make clean`: 会删除所有生成的可执行文件和构造用的 build 目录

## dtp_config.h

提供了可以用于解析 block_trace 的信息的函数，给出了 block_trace 的数据结构，具体可以查看下表。

| Time(s)  | Deadline(ms) | Block size(Bytes) | Priority |
| -------- | ------------ | ----------------- | -------- |
| 0.0      | 200          | 8295              | 2        |
| 0.021333 | 200          | 2560              | 1        |
| ……       | ……           | ……                | ……       |

- time : 该block的创建时间与上一个block的创建时间之差；
- Deadline ： 该block的有效时间；
- Block size ： 该block的总大小；
- Priority ： 该block的优先级。