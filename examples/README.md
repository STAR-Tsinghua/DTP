# AItransDTP server 开发

AItransDTP 的 examples 目录基本上只用于测试开发 server 

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