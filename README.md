# AitranDTP docker autobuild

本分支用于内部开发人员的 server docker 环境开发。本分支增加了一些文档和脚本来帮助后来者进行开发。

## 环境安装

目前确认正确运行的环境：

1. OS: Ubuntu 18.04
2. Docker: 20.10.5
3. Rustc: 1.50.0
4. python: 3.8.8

## 开发工作说明

### 仓库编译方法说明

本分支基于 AItransDTP 进行开发，AItransDTP 因为对外暴露了 C 的接口，在程序的编译和运行过程上和 DTP 有一定的区别，这里稍微进行说明。

本仓库的主体是 DTP Rust crate, 其中`src/aitrans`目录下是 AItransDTP 中新增加的接口代码。这会将原仓库中拥塞控制部分的算法拓展出一个 C 的接口，并且将接口外的代码编译成动态链接库。在最终的可执行文件编译的过程中，需要先通过编译 Rust 仓库形成 libquiche.a ，将 Rust 代码编译成静态链接库；再通过编译`src/aitrans`目录下的文件形成动态链接库；最后利用两个库来编译可执行文件。`examples`目录下的`server.c`文件便是利用这种方法进行编译的，可以通过`examples`目录下的 Makefile 进行快速的编译操作。

关于这种编译方式以及接口的开发方法，有一个演示仓库可以用于参考 https://github.com/simonkorl/rust_link_demo 。该参考仓库模仿了本仓库的编译方式，并且给出了如何进行 Rust 和 C 的交叉编程。

### docker 环境开发说明

`docker`目录下有若干用于辅助内部进行镜像开发的工具，其中生成的镜像内容会在目录`docker/aitrans-server`中。

为了得到可以运行的 AItransDTP server 镜像，目前**必须**提供如下的内容：

1. 一个基于 DTP 开发的 client 端
    * 这可以从 `windows_app_client` 分支编译得到。对应可执行文件需要处于`docker/aitrans-server`目录下
2. 一个基于 AItransDTP 开发的 server 端
    * 可执行文件需要位于`docker/aitrans-server/bin`目录下
3. server 端所需要使用 trace
    * 格式详见 https://github.com/AItransCompetition/AitransSolution/blob/master/final_datasets/README.md 。可以使用`docker/block_trace`目录下的工具生成。生成出来的 trace 文件需要位于 `docker/aitrans-server/trace/block_trace`目录下。
4. server 端运行需要依赖的 libsolution.so 
    * 需要通过 `solution.hxx` 编译得到。
    * 需要位于 `docker/aitrans-server/lib` 目录下
5. server 需要的密钥
    * 将`examples`目录下的 cert.crt 与 cert.key 复制到`docker/aitrans-server`目录下

**注**：如果你还打算使用“一键运行”脚本（位于 `docker/tools_demo`目录下。可以查看该目录下的说明文档来了解更多的信息）。那么你还需要做如下的事情：

1. 将你的需要编译成 libsolution.so 的文件复制到 `docker/aitrans-server/demo` 目录下。目前其中包括了基础的 solution.hxx, solution.cxx
    * 你可以利用“一键运行”脚本完成这个复制操作。请阅读说明文档来了解如何完成这件事

## 基础编译与测试

在完成上述的 docker 环境准备工作后在`examples`下可以运行下面的指令

* `make server` 可以生成 server 可执行文件和所需要依赖的 libsolution.so 库文件
* `make pre_docker` 会在 docker 文件夹中准备好 docker 需要的所有文件。会执行 docker 目录下的脚本`pre_docker.sh`
* `make build` 会构建一个名称为 `aitrans_ubuntu:0.0.2` 的镜像到本地。执行的脚本为`build_image.sh`
* `make image_test` 会在构建好最新的镜像后运行测试程序。执行的脚本为`image_test.sh`
* `make clean` 会删除所有生成的可执行文件，但是不会影响到 docker 目录下的所有内容。在重新编译 server 前都需要运行这条指令
## 额外帮助

一些额外的信息可以在 https://github.com/AItransCompetition/AitransSolution/tree/master/doc 找到。
