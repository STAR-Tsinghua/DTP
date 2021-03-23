#!/usr/bin/python
# -*- coding: utf-8 -*-

'''
# @ModuleName : tc_ctl
# @Function : 
# @Author : azson
# @Time : 2019/11/25 14:00
'''

import random
import time, os, sys
import argparse
import datetime

help_info = '''
    Examples :
    1. -load test.txt 
        You can change the bandwith and delay according to the 'test.txt' file
        The 'test.txt' file should be the pattern : "timestamp,bandwith_mbit,delay_ms". Just like below
           1,10,20
           5.2,5.5,10,
           ......
        Then it will use '1' as now timestamp, and changed eth0's bandwith to 10Mbit and delay to 20ms.
        After 4.2s (5.2-1), it changed eth0's bandwith to 5.5Mbit and delay to 10ms.

        PS : eth0 is the parameter of nic default value, you can specify like this "-nic eth0"

    2. -o bw_delay
        You can change the eth0's bandwith and delay per internal ms.
        By default, 
            the bandwith is an random value in [1, 10), you can specify the "-mn_bw 1" and "-mx_bw 10" to config;
            the delay is an random value in [0, 100), you can specify the "-mn_dl 0" and "-mx_dl 100" to config;
            internal is 5, you can specify "-i 5" to config.

    3. -once 
        You can change the eth0's bandwith and delay once.
        By default,
            You can specify "--bandwith 10" to set bandwith to 10Mbit;
            You can specify "--delay 10" to set delay to 10ms.
    '''

def get_now_time():
    return datetime.datetime.now().strftime('%Y-%m-%d %H:%M:%S')


def tc_easy_bandwith(**kwargs):

    nic_name = kwargs['nic']
    # bandwith
    mx_bw = float(kwargs['max_bandwith'])
    mn_bw = float(kwargs['min_bandwith'])

    op_mode = 'change'
    if 'first' in kwargs and kwargs['first']:
        # del old qdisc
        os.system('tc qdisc del dev {0} root'.format(nic_name))
        op_mode = 'add'

    if kwargs['bandwith'] == None:
        # generate bandwith in [mn_bd, mx_bd)
        bw = random.random() * (mx_bw - mn_bw) + mn_bw
    else:
        bw = float(kwargs['bandwith'])

    # tbf
    # for ubuntu 16.04, the latest buffer to reach rate = rate / HZ
    # buffer = bw*11000 if not kwargs['buffer'] else float(kwargs['buffer'])
    # buffer = 1350*20 if not kwargs['buffer'] else float(kwargs['buffer'])
    # latency = 100 if not kwargs['latency'] else float(kwargs['latency']) # bw*43.75

    # loss rate
    loss_rate = 0 if not kwargs['loss_rate'] else float(kwargs['loss_rate'])
    loss_parser = "" if loss_rate <= 0.000001 else "loss {0}%".format(loss_rate)

    # os.system('tc qdisc {4} dev {0} root handle 1:0 tbf rate {1}mbit buffer {2} latency {3}ms'.format(
                # nic_name, bw, buffer, latency, op_mode))
    # htb
    if kwargs['first']:
        os.system("tc qdisc add dev {0} root handle 1: htb default 11".format(nic_name))
    os.system("tc class {0} dev eth0 parent 1: classid 1:11 htb rate {1}mbit ceil {1}mbit".format(op_mode, bw))
    if kwargs['first']:
        os.system("tc filter add dev eth0 protocol ip parent 1:0 prio 1 u32 match ip dport 0 0x0000 flowid 1:11")
    print("Time : {2}, {3} nic {0}, bandwith to {1}mbit".format(nic_name, bw, get_now_time(), op_mode))

    # delay
    mx_delay_ms = float(kwargs['max_delay'])
    mn_delay_ms = float(kwargs['min_delay'])

    if kwargs['delay'] == None:
        delay_ms = random.random() * (mx_delay_ms - mn_delay_ms) + mn_delay_ms
    else:
        delay_ms = float(kwargs['delay']) * 1000
        if delay_ms <= 0.000001:
            return

    os.system('tc qdisc {3} dev {0} parent 1:11 handle 10: netem delay {1}ms {2}'.format(nic_name, delay_ms, loss_parser, op_mode))
    print("Time : {2}, {3} nic {0}, delay_time to {1}ms".format(nic_name, delay_ms, get_now_time(), op_mode))


def load_file(**kwargs):

    file_path = kwargs['load_file']

    try:
        with open(file_path, 'r') as f:
            # file content pattern :
            # time_stamp,bandwith,delay
            info_list = list(map(lambda x: x.strip().split(','), f.readlines()))

    except Exception as e:
        print("File path %s is wrong!" % file_path)
        print(e)
        return 

    try:
        for idx, item in enumerate(info_list):
            kwargs['bandwith'] = item[1]
            kwargs['delay'] = item[2]
            pre_time = time.time()

            if idx:
                sleep_sec = (float(item[0]) - float(info_list[idx-1][0])) - (time.time() - pre_time)
                if sleep_sec > 0:
                    time.sleep(sleep_sec)

            tc_easy_bandwith(**kwargs)
    except Exception as e:
        print("Please check file %s content is right!" % file_path)
        print(e)


def load_new_file(**kwargs):
    file_path = kwargs['load_file']

    try:
        with open(file_path, 'r') as f:
            # file content pattern :
            # time_stamp,bandwith,loss_rate,delay
            info_list = list(map(lambda x: x.strip().split(','), f.readlines()))

    except Exception as e:
        print("File path %s is wrong!" % file_path)
        print(e)
        return 

    try:
        for idx, item in enumerate(info_list):
            # MB to Mbit
            kwargs['bandwith'] = float(item[1]) * 8
            # to pencentage
            kwargs['loss_rate'] = float(item[2]) * 100
            kwargs['delay'] = item[3]
            pre_time = time.time()

            if idx:
                sleep_sec = (float(item[0]) - float(info_list[idx-1][0])) - (time.time() - pre_time)
                if sleep_sec > 0:
                    time.sleep(sleep_sec)
            kwargs['first'] = False if idx else True
            tc_easy_bandwith(**kwargs)
            sys.stdout.flush()
    except Exception as e:
        print("Please check file %s content is right!" % file_path)
        print(e)



def get_params_dict(arg_list):

    # default parameters
    params = {
        "op" : "help",
        "nic" : "eth0",
        "max_bw" : "10",
        "min_bw" : "1",
        "internal" : "5",

        "max_delay" : "100",
        "min_delay" : "0",
    }

    for item in arg_list:
        tmp = item.split("=")
        if len(tmp) == 2:
            params[tmp[0]]=tmp[1]

    return params


def init_argparse():

    parser = argparse.ArgumentParser(
        prog="Easy TC Controller",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        description=help_info)

    parser.add_argument("-o", "--oper",
                        choices=["bw_delay"],
                        default="bw_delay",
                        help="Choose the function you want to do")

    parser.add_argument("-load", "--load_file",
                        metavar="FILE_PATH",
                        help="You can change the bandwith and delay according to a file")

    parser.add_argument("-nic", nargs=1, default="eth0",
                        metavar="ETHERNET",
                        help="Network Interface Controller")

    parser.add_argument("-i", "--internal",
                        type=float,
                        help="The time you want to change the TC rule.")

    # detail parameters
    parser.add_argument("-mx_bw", "--max_bandwith",
                        type=float, default=10,
                        help="For random bandwith")
    parser.add_argument("-mn_bw", "--min_bandwith",
                        type=float, default=1,
                        help="For random bandwith")
    parser.add_argument("-mx_dl", "--max_delay",
                        type=float, default=100,
                        help="For random delay")
    parser.add_argument("-mn_dl", "--min_delay",
                        type=float, default=0,
                        help="For random delay")

    parser.add_argument("-once", "--change_once",
                        action="store_true",
                        help="You can change the nic's bandwith and delay once.")

    # detail parameters
    parser.add_argument("-bw", "--bandwith",
                        type=float,
                        help="The value of bandwith you want to change")
    parser.add_argument("-dl", "--delay",
                        type=float,
                        help="The value of delay you want to change")
    parser.add_argument("-bf", "--buffer",
                        type=float,
                        help="The value of buffer size you want to change in tbf")
    parser.add_argument("-lat", "--latency",
                        type=float,
                        help="The value of latency you want to change in tbf")
    parser.add_argument("-loss", "--loss_rate",
                        type=float,
                        help="The value of loss_rate you want to change in netem")

    parser.add_argument("-r", "--reset",
                        metavar="ETHERNET",
                        help="You will delete eth0's all queue discplines.")

    parser.add_argument("-sh", "--show",
                        metavar="ETHERNET",
                        help="You can get eth0's all queue discplines.")

    parser.add_argument("-aft", "--after",
                        metavar="ETHERNET",
                        help="run after time")

    parser.add_argument('-v', '--version', action='version', version='%(prog)s 1.0')


    return parser

if __name__ == '__main__':

    parser = init_argparse()


    pre_time = time.time()
    params=parser.parse_args()
    params=vars(params)
    params['first'] = True

    # for aitrans
    if params['after']:
        time.sleep(float(params['after']))

    # change once
    if params['change_once']:
        if params['oper'] == 'bw_delay':
            tc_easy_bandwith(**params)

    # change bandwith and delay according file
    elif params['load_file']:
        load_new_file(**params)

    elif params["reset"]:
        os.system('tc qdisc del dev {0} root'.format(params['reset']))

    elif params["show"]:
        os.system('tc qdisc show dev {0}'.format(params['show']))

    # change per internal
    elif params['internal']:
        fir = True
        while True:
            if time.time()-pre_time >= params['internal']:
                pre_time=time.time()

                print("time {0}".format(pre_time))
                if params['oper'] == 'bw_delay':
                    tc_easy_bandwith(**params)
                if fir:
                    fir = False
                    params['first'] = False
                time.sleep(params['internal'])
    else:
        parser.print_help()
