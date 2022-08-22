# DTP 样例代码

当前目录下分别包括了 Rust 和 C 语言版本的 DTP 样例程序。

一组样例程序基本包含如下的内容：

1. client 端代码
2. server 端代码
3. cert.crt / cert.key ，用于进行验证
4. 其他辅助文件

## Rust C/S 样例

Rust 版本样例程序使用 `cargo build --examples` 进行构建，在**项目根目录**下运行下面指令进行测试。

```bash
$ cargo run --example server
$ cargo run --example client http://127.0.0.1:4433/hello_world --no-verify
```

Rust 版本的样例程序可以执行一个简单的 HTTP 请求，展示了一个使用 DTP 的简单网络传输框架。

## C Ping-Pong

C 版本的一个样例程序位于 c_ping-pong 目录下，在该目录下使用`make`命令进行编译。更多细节可见 ping-pong 目录下的说明文档。

## C Features Example

c_features_example 目录下使用 C 语言展示了 DTP 新增的部分特性。细节详见该目录下的说明文档。
