#!/bin/bash
result=$1
server_log=$2
config=$3
client_log=$4
weight=$5
min_priority=$6
#echo "./server_begin.sh $result $server_log"
./server_begin.sh $result $server_log
sleep 1
./tc.sh
#echo "result=$1,server_log=$2,config=$3,client_log=$4"
./client.sh $config $client_log $weight $min_priority
./server_end.sh
echo -e "\nsleep 10\n"
sleep 10

