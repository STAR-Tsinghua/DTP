#!/usr/bin/expect -f
set timeout 30
set DEV [lindex $argv 0]
set delay [lindex $argv 1]
# spawn ssh root@192.168.1.1 "./DTP/dtp/rtt.sh eth0.2 2mbps 2kb 1.0ms"
# DEV=$1; brandwidth=$2; burst=$3; lant=$4
spawn ssh root@192.168.1.1 "./DTP/dtp/rtt.sh $DEV $delay"
expect "password:"
send "sh123456\r"
send "exit\r"
expect eof
exit
