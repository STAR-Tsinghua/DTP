#!/bin/bash
cd ~/Documents/DTP/examples
echo "./client 192.168.10.2 5555 $1 &> $2 $3 $4"
./client 192.168.10.2 5555 $1 &> $2 $3 $4
# $1: config file ; $2: log file