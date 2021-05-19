## 环境安装

目前确认正确运行的环境：

1. OS: Ubuntu 18.04
2. Docker: 20.10.5
3. Rustc: 1.50.0
4. python: 3.8.8
\*\* 5. libtorch: 1.9.0.dev20210417+cpu

一些依赖库的安装方法(以 Ubuntu18.04 为例)：

1. ev: `sudo apt install libev-dev`
2. uthash: `sudo apt install uthash-dev`
3. \*\* libtorch: 从 https://pytorch.org/resources/ 的页面最下方，选择 Nightly, Linux（其他操作系统未测试）, LibTorch, C++, CPU ，下载 **cxx11 ABI** 版本。并且将解压后的 libtorch 目录复制到 aitrans 目录下，形成`src/aitrans/libtorch/*`的目录结构

** 注：libtorch 目前不是强制需要的。只有需要在 C++ 接口中使用 pytorch 的机器学习模型的时候才会需要。

于此同时，本项目的采用了 git submodule 来管理部分组件，不要忘记进行同步。

```bash
git submodule init
git submodule update
```

boringssl 库在国内 git 下载速度缓慢，建议直接下载压缩文件：https://github.com/google/boringssl

## 额外添加 C 接口说明

目前开放的 C 的接口如下：

1. `uint64_t SolutionAckRatio`：返回一个正整数 n ，代表每 n 个数据包发送一个 ACK 包。目前建议返回一个固定数值。默认返回`4`
2. `float SolutionRedundancy`：返回一个 \[0, +\) 的浮点数，代表增加的额外冗余程度。可以根据不同的情况提供不同的冗余程度，默认返回为`0`。\*注1 
3. `bool SolutionShouldDropBlock`：返回一个 bool 类型变量，代表对于每一个数据包，是否应该在发包之前将其丢弃。返回 true 则代表丢弃该数据包。
4. `void SolutionInit`：初始化 C 代码所需要的数据
5. `uint64_t SolutionSelectBlock`：从数据块数组中选择出下一个传输的数据块
6. `void SolutionCcTrigger`：根据拥塞控制算法相关的信息来执行拥塞控制相关算法的实现

算法接口的具体定义可以见`src/aitrans/include`

* 注1：所谓冗余程度指的是如果有$m$个原本的数据包，那么给定冗余程度$\alpha$，那么会产生额外的$n = \alpha m$个数据包，那么其可以忍受最大为$n$的丢包，也就是说一共会传输$m+n$个数据包，只要收到了其中的任何$m$个数据包便可恢复所有数据。

## 仓库编译方法说明

本分支基于 AItransDTP 进行开发，AItransDTP 因为对外暴露了 C 的接口，在程序的编译和运行过程上和 DTP 有一定的区别，这里稍微进行说明。

本仓库的主体是 DTP Rust crate, 其中`src/aitrans`目录下是 AItransDTP 中新增加的接口代码。这会将原仓库中部分算法模块拓展出一个 C 的接口，并且将接口外的代码编译成动态链接库。在最终的可执行文件编译的过程中，需要先通过编译 Rust 仓库形成 libquiche.a ，将 Rust 代码编译成静态链接库；再通过编译`src/aitrans`目录下的文件形成动态链接库；最后利用两个库来编译可执行文件。`examples`目录下的`server.c`文件便是利用这种方法进行编译的，可以通过`examples`目录下的 Makefile 进行快速的编译操作。

关于这种编译方式以及接口的开发方法，有一个演示仓库可以用于参考 https://github.com/simonkorl/rust_link_demo 。该参考仓库模仿了本仓库的编译方式，并且给出了如何进行 Rust 和 C 的交叉编程。

## 额外帮助

一些额外的信息可以在 https://github.com/AItransCompetition/AitransSolution/tree/master/doc 找到。