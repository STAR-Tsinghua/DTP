#!/usr/bin/expect -f
set timeout -1
# set result [lindex $argv 0]
# set server_log [lindex $argv 1]
spawn ssh jie@192.168.10.2 "cd ~/Documents/AItransDTP/examples ; ./server 192.168.10.2 5555 trace/block_trace/aitrans_block.txt &> server_aitrans.log &"
# expect "password:"
# send "1234\r"
expect eof
wait