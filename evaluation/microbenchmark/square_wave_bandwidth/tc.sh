#!/usr/bin/expect -f
set timeout 30
set DEV [lindex $argv 0]
set brandwidth [lindex $argv 1]
set burst [lindex $argv 2]
set lant [lindex $argv 3]
spawn ssh root@192.168.1.1 "cd ~/DTP/dtp;./square_wave_tc.sh &"
expect "password:"
send "sh123456\r"
send "exit\r"
expect eof
exit
