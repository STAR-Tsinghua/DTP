#!/usr/bin/expect -f
set timeout 30
set DEV [lindex $argv 0]
set delay [lindex $argv 1]
spawn ssh root@192.168.1.1 "cd ~/DTP/dtp;./trace.sh &"
expect "password:"
send "sh123456\r"
send "exit\r"
expect eof
exit
