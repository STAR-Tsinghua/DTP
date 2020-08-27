FROM ubuntu:18.04
RUN apt-get update
RUN apt-get install -y libev4 libasan4 net-tools iproute2 iperf vim lsof g++
COPY ./aitrans-server /home/aitrans-server
# COPY ../examples/{config,client,server,client_trace} /home
WORKDIR /home
EXPOSE 5555
