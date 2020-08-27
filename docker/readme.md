# aitrans docker运行环境开发者使用步骤（内部文件）
## 1. docker环境的安装(略)
## 2. build
1. 进入AItransDTP/docker/文件夹
2. 预备工作：在Ubuntu18.04的环境下编译好client和server后，将client复制到aitrans-server文件夹中，server放到aitrans-server/bin文件夹中。运行./pre_docker.sh
3. build镜像，例如：docker image build -t aitrans_ubuntu:0.0.2 .

## 3. 新建containers
1. 例：docker container run --name=aitrans_server --cap-add=NET_ADMIN -it aitrans_ubuntu:0.0.2 /bin/bash
2. 执行以上命令后，会新建container并直接进入其/home目录中，可以看到如下文件(夹)：aitrans-server

## 运行server程序
参考aitrans_docker.md

## TC设置
1. 使用/home/TC/目录中的脚本进行TC设置。
2. 如限制带宽为4MB/s。在container的/home目录下运行命令：./TC/rate.sh eth0 4mbps 4kb 1ms。  
注意TC限制的是发送端，因此该操作在用作发送端的container上执行。