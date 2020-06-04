#!/usr/bin/expect -f
set timeout -1
set result [lindex $argv 0]
set server_log [lindex $argv 1]
spawn ssh jie@192.168.10.2 "cd ~/Documents/DTP/examples; mkdir result; cd result; mkdir data;mkdir server_log;cd data;mkdir QUIC;mkdir Deadline;mkdir Priority;mkdir DTP;"
# expect "password:"
# send "1234\r"
expect eof
wait