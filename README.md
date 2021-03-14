# Usage
## 环境安装
## 编译
- 在src/aitrans下运行 `g++ -shared -fPIC solution.cxx -I include -o libsolution.so` 得到子模块的动态链接库
- 在 examples下运行 `make` 命令生成server
## 运行
- 运行server需要：libsolution.so，cert.crt，cert.key文件
- 运行命令：查看docker/aitrans-server/run_server.sh脚本