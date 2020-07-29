#!/usr/bin/expect -f
# todo: test
set timeout 30
set trace [lindex $argv 0]
spawn ssh root@192.168.1.1 "cd ~/DTP/dtp;./${trace} &"
expect "password:"
send "sh123456\r"
send "exit\r"
expect eof
exit
