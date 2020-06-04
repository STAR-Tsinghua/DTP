#!/usr/bin/python3

'good bytes: sum of bytes before deadline of all blocks'

import numpy as np
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import matplotlib.ticker as mtick

lab_times=2 #重复实验次数
step=20 #rtt间隔
rtt_num=10 #假设测5种rtt
block_num=300  #有用的block
total_block_num=1000 #总共发包量

def good_bytes_list(f):
    goodbytes = [0]*block_num
    for line in f.readlines():
        if line.find('goodbytes') != -1:
            s_arr = line.split()
            id = int(s_arr[4])
            index = int(id/4)-1
            #################
            index -= 700
            if index < 0:
                continue
            ################
            good_bytes_add = int(s_arr[6])
            goodbytes[index] += good_bytes_add
    return goodbytes

def sum_good_bytes(fpath):
    with open(fpath, 'r') as f:
        gb_list = good_bytes_list(f)
    total_gb = 0
    for gb in gb_list:
        total_gb += gb
    return total_gb/1000_000

def payload_send(fpath):
    sended = 0
    f=open(fpath,'r')
    for line in f.readlines():
        if line.find('payload send') != -1:
            # print(line)
            s_arr = line.split()
            # print(s_arr[3],s_arr[7])
            # break
            ######################################
            #也需要根据ID进行筛选，把前700个过滤掉
            id = int(s_arr[7])
            if id/4-1<700:
                continue
            ######################################
            buf_len = int(s_arr[3])
            # print(line)
            sended += buf_len
    return sended/1000_000


def get_y(case,y,err):
    for i in range(1,rtt_num+1):
        # 2020.2.2：每种bandwidth跑了5次，5次实验的实验结果，分别在result1~5/data/中，算完后取平均值
        arr=[]
        for j in range(lab_times):
            fpath='result'+str(j+1)+'/server_log/'+case+'-server-'+str(20*i-20)+'ms.log'
            g_bytes = sum_good_bytes(fpath) #good bytes
            fpath = 'result'+str(j+1)+'/client_log/'+case +'-client'+'-'+str(20*i-20)+'ms.log'
            s_bytes = payload_send(fpath) #send bytes total
            arr.append(g_bytes/s_bytes)
        #求均值
        y[i-1] = np.mean(arr) * 100
        #求方差
        # err[i-1] = np.var(arr)
        #标准差
        err[i-1] = np.std(arr,ddof=1) * 100

x = np.arange(1,rtt_num+1)
y = np.arange(1.0, rtt_num+1)
for i in range(rtt_num):
    x[i]=i*step
fig, ax = plt.subplots(dpi=200)#figsize=(16, 12)
case=['DTP','QUIC','Deadline','Priority']
case_draw=['DTP','QUIC','DTP-D','DTP-P']
line_style = ['-','--','-.',':']
for i in range(4):
    err=[0]*len(x)#方差
    get_y(case[i],y,err)
    plt.errorbar(x,y,yerr=err,label=case_draw[i],fmt=line_style[i],capsize=4,linewidth=2)
# 设置百分比显示
fmt = '%.0f%%' # Format you want the ticks, e.g. '40%'
yticks = mtick.FormatStrFormatter(fmt)
ax.yaxis.set_major_formatter(yticks)
ax.set_xlabel('RTT/ms',size='xx-large')
ax.set_ylabel('Good bytes ratio',size='xx-large')
ax.legend(fontsize='xx-large')
ax.set_xticks(x)
plt.xticks(size='xx-large')
plt.yticks(size='xx-large')
plt.grid(linestyle='--')  # 生成网格
# plt.show()
plt.savefig('new_good_bytes_ratio.png',bbox_inches='tight') # 核心是设置成tight

