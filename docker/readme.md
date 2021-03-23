# aitrans docker运行环境开发者使用步骤（内部文件）

开发环境：在 Ubuntu18.04 环境下完成以下工作

## 1. docker环境的安装(略)

## 2. 预备工作

1. 在 `windows_app_client` 分支的项目根目录执行 `cargo build --release --example client` 指令，得到 client 可执行文件
2. 将 client 可执行文件从 `AItransDTP/target/release/examples/client` 目录复制到 `AItransDTP/docker/aitrans-server` 下，之后不需要再去处理 client 相关的文件。

## 2. 创建镜像与测试

在 `examples/` 目录下有已经修改过的 Makefile 文件，其中提供了若干指令：

`make server` 可以生成 server 可执行文件和所需要依赖的 libsolution.so 库文件
`make pre_docker` 会在 docker 文件夹中准备好 docker 需要的所有文件。会执行 docker 目录下的脚本`pre_docker.sh`
`make build` 会构建一个名称为 `aitrans_ubuntu:0.0.2` 的镜像到本地。执行的脚本为`build_image.sh`
`make image_test` 会在构建好最新的镜像后运行测试程序。执行的脚本为`image_test.sh`
`make clean` 会删除所有生成的可执行文件，但是不会影响到 docker 目录下的所有内容。在重新编译 server 前都需要运行这条指令

## 额外帮助

一些额外的信息可以在 https://github.com/AItransCompetition/AitransSolution/tree/master/doc 找到。

<!-- 
## 3. 新建containers
1. 例：docker container run --name=aitrans_server --cap-add=NET_ADMIN -it aitrans_ubuntu:0.0.2 /bin/bash
2. 执行以上命令后，会新建container并直接进入其/home目录中，可以看到如下文件(夹)：aitrans-server

## 运行server程序
参考aitrans_docker.md

## TC设置
1. 使用/home/TC/目录中的脚本进行TC设置。
2. 如限制带宽为4MB/s。在container的/home目录下运行命令：./TC/rate.sh eth0 4mbps 4kb 1ms。  
注意TC限制的是发送端，因此该操作在用作发送端的container上执行。 -->
