# evaluation docker运行环境使用步骤
## 1. docker环境的安装(略)
## 2. build
1. 进入DTP/docker文件夹
2. build镜像，例如：docker image build -t dtp-evaluation:0.0.4 .

## 3. 新建containers
1. 例：docker container run --cap-add=NET_ADMIN -it dtp-evaluation:0.0.4 /bin/bash
2. 执行以上命令后，会新建container并直接进入其/home目录中，可以看到如下文件(夹)：TC  cert.crt  cert.key  config

## 运行程序
1. 在Ubuntu18.04的环境下编译好client和server后，拷进container的/home目录中即可直接运行
2. 如在IP地址为172.17.0.2 的container中运行：./server 172.17.0.2 5555 1> result.log 2> server.log  
在另一container运行：./client 172.17.0.2 5555 config/config-DTP.txt 0.5 0.5 &> client.log

## TC设置
1. 使用/home/TC/目录中的脚本进行TC设置。
2. 如限制带宽为4MB/s。在container的/home目录下运行命令：./TC/rate.sh eth0 4mbps 4kb 1ms。  
注意TC限制的是发送端，因此该操作在用作发送端的container上执行。