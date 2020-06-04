#!/usr/bin/expect -f
set timeout 30
set DEV [lindex $argv 0]
set bandwidth [lindex $argv 1]
set burst [lindex $argv 2]
set lant [lindex $argv 3]
# spawn ssh root@192.168.1.1 "./DTP/dtp/rate.sh eth0.2 2mbps 2kb 1.0ms"
# DEV=$1; bandwidth=$2; burst=$3; lant=$4
spawn ssh root@192.168.1.1 "./DTP/dtp/rate.sh $DEV $bandwidth $burst $lant"
expect "password:"
send "sh123456\r"
send "exit\r"
expect eof
exit
