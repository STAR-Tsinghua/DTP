FROM ubuntu:18.04
RUN apt-get update
RUN apt-get install -y libev4 libasan4 net-tools iproute2 iperf vim lsof g++ python3 python3-pip
COPY ./aitrans-server /home/aitrans-server
WORKDIR /home
EXPOSE 5555
