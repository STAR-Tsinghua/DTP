#!/usr/bin/expect -f
set timeout -1
set result [lindex $argv 0]
set server_log [lindex $argv 1]
spawn ssh jie@192.168.10.2 "cd ~/Documents/DTP/examples ; ./server 192.168.10.2 5555 1> $result 2> $server_log &"
# expect "password:"
# send "1234\r"
expect eof
wait