#!/usr/bin/expect -f
set timeout 30
spawn ssh jie@192.168.10.2 ./Documents/DTP/examples/kill_server.sh
# expect "password:"
# send "1234\r"
expect eof
wait