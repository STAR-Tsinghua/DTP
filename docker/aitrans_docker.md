## import docker
```dash
docker import aitrans_ubuntu.tar
```
## docker使用 步骤（内部版）
1. 文件地址：/home/aitrans-server
2. 编译 libsolution.so
```bash
cd /aitrans-server/demo
g++ -shared -fPIC solution.cxx -I include -o libsolution.so
cp libsolution.so ../lib
```
2. 分别运行server和client
```bash
cd /aitrans-server
./run_server.sh #ip需要改成该容器的
#另开一个容器作为client，并运行：
./run_client.sh #ip需要改成上方作为server的容器的
# ./kill_server.sh 关闭后台的server
```
3. 查看log
- client log 记录了block完成情况: /aitrans-server/client.log
- server log 记录了server端log，可查看报错: /aitrans-server/log/server_aitrans.log

## qoe 计算
1. 将qoe.py放到和client.log所在路径下;
2. 运行 `python qoe.py`.