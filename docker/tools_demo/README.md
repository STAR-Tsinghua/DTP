
## TC Introduction

* The TC is a way to control the nic behavior to emulate your network performance like bandwidth,loss rate and rtt.

* The TC info is in [Traffic Control](https://wiki.archlinux.org/index.php/Advanced_traffic_control_(%E7%AE%80%E4%BD%93%E4%B8%AD%E6%96%87))

## The TC scripts
* The traffic_control.py main functions is based on the **tc** order in linux. 

* And its argument parser is based on the **argparse** in python library. 

* All of the functions was tested in the python3.x .

* Python3 requirements:

                            pip install argparse

## Quick Start

                            python3 traffic_control.py -once 1 -bw 10 -dl 10
                            
* This order will output change the queue discipline on the NIC eth0. 

       changed nic eth0, bandwith to 10.0mbit
       changed nic eth0, delay_time to 10.0ms

                           python3 traffic_control.py --show eth0

* This order will out all the queue discipline on the NIC eth0. 

       qdisc tbf 1: root refcnt 2 rate 10Mbit burst 11000b lat 437.5ms
       qdisc netem 10: parent 1:1 limit 1000 delay 10.0ms

* If there are none error about tc or argparser, congratulations to you ! Now, You can use it to do more things.


## Load Scripts Tools

* If you want to change the network behaviro dynamicially, recommend to use the load trace file,which can both change the bandwith and delay on a NIC 

  Suppose there is a file named "trace.txt".And it's content is below :

   |  timestampe   | bandwidth(mbps)|  delay(ms)|
   |  --------     | -----------    | --------  |
   |  0.0000023    |  1.0           |   23.0    |
   |  0.1132324    |  2.2           |   54.2    |  
  
  * the trace content like this,the split use the tab
  > 1	23	10
  >
  > 5	10	20
  >
  > 15	7	23

* Then you can input below :

                               python3 traffic_control.py -load trace.txt

## Delete the traffic control limit

* Delete all the queue disciplines on a NIC.

  If you want to delete all the queue disciplines on the NIC eth0, you can input below :

                               python3 traffic_control.py -r eth0
  >  

* If the output is nothing or "RTNETLINK answers: No such file or directory", it means there is no queue disciplines on the NIC eth0 now.


* If you want to learn more. The output involes the knowledge about tc order in linux. So, we recommend you to [Click me!](https://www.badunetworks.com/traffic-shaping-with-tc/)


## Tips

* All of above functions are enough for you in the competitions . For more detail about parameters, just run

                               python3 traffic_contril.py -h
> 
