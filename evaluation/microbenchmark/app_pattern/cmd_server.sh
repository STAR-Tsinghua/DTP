#!/bin/bash
cd ~/Documents/DTP/examples
echo 'server begin'
# QUIC
# {360; 480; 720; 1080; 1440}p
./server 192.168.10.2 5555 1> result/QUIC-result-360p.log 2> result/server_log/QUIC-server-360p.log
# ./server 192.168.10.2 5555 1> result/QUIC-result-480p.log 2> result/server_log/QUIC-server-480p.log
# ./server 192.168.10.2 5555 1> result/QUIC-result-720p.log 2> result/server_log/QUIC-server-720p.log
# ./server 192.168.10.2 5555 1> result/QUIC-result-1080p.log 2> result/server_log/QUIC-server-1080p.log
# ./server 192.168.10.2 5555 1> result/QUIC-result-1440p.log 2> result/server_log/QUIC-server-1440p.log

# Deadline
# {360; 480; 720; 1080; 1440}p
# ./server 192.168.10.2 5555 1> result/Deadline-result-360p.log 2> result/server_log/Deadline-server-360p.log
# ./server 192.168.10.2 5555 1> result/Deadline-result-480p.log 2> result/server_log/Deadline-server-480p.log
# ./server 192.168.10.2 5555 1> result/Deadline-result-720p.log 2> result/server_log/Deadline-server-720p.log
# ./server 192.168.10.2 5555 1> result/Deadline-result-1080p.log 2> result/server_log/Deadline-server-1080p.log
# ./server 192.168.10.2 5555 1> result/Deadline-result-1440p.log 2> result/server_log/Deadline-server-1440p.log

# Priority
# {360; 480; 720; 1080; 1440}p
# ./server 192.168.10.2 5555 1> result/Priority-result-360p.log 2> result/server_log/Priority-server-360p.log
# ./server 192.168.10.2 5555 1> result/Priority-result-480p.log 2> result/server_log/Priority-server-480p.log
# ./server 192.168.10.2 5555 1> result/Priority-result-720p.log 2> result/server_log/Priority-server-720p.log
# ./server 192.168.10.2 5555 1> result/Priority-result-1080p.log 2> result/server_log/Priority-server-1080p.log
# ./server 192.168.10.2 5555 1> result/Priority-result-1440p.log 2> result/server_log/Priority-server-1440p.log

# DTP
# {360; 480; 720; 1080; 1440}p
# ./server 192.168.10.2 5555 1> result/DTP-result-360p.log 2> result/server_log/DTP-server-360p.log
# ./server 192.168.10.2 5555 1> result/DTP-result-480p.log 2> result/server_log/DTP-server-480p.log
# ./server 192.168.10.2 5555 1> result/DTP-result-720p.log 2> result/server_log/DTP-server-720p.log
# ./server 192.168.10.2 5555 1> result/DTP-result-1080p.log 2> result/server_log/DTP-server-1080p.log
# ./server 192.168.10.2 5555 1> result/DTP-result-1440p.log 2> result/server_log/DTP-server-1440p.log

cd -