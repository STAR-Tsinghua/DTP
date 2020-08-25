## import docker
```dash
docker import aitrans_ubuntu.tar
```
## docker使用 步骤（内部版）
1. 文件地址：/aitrans-server
2. 编译 libsolution.so
```bash
cd /aitrans-server/demo
g++ -shared -fPIC solution.cxx -I include -o libsolution.so
cp libsolution.so ../lib
```
2. 分别运行server和client
```bash
cd /aitrans-server
./run_server.sh
./run_client.sh
# ./kill_server.sh 关闭后台的server
```
3. 查看log
- client log 记录了block完成情况: /aitrans-server/client.log
- server log 记录了server端log，可查看报错: /aitrans-server/log/server_aitrans.log