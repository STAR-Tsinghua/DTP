#!/usr/bin/env python3
import numpy as np
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import matplotlib.ticker as mtick

lab_times=1 #重复实验次数
step=1 #带宽间隔
bandwidth_num=10 #假设测5种带宽
deadline=200 #deadline 200ms
block_num=300 #总共发包量

def canceled_block_num(f):
    cancel_times=0
    for line in f.readlines():
        if line.find('cancel block')!=-1:
            # s_arr=line.split(',')
            # canceled_block=s_arr[0].split()[3]
            # print(canceled_block)
            cancel_times+=1
    return cancel_times

def get_y(case,y):        
    for i in range(1,bandwidth_num+1):
        # 2020.2.2：每种bandwidth跑了5次，5次实验的实验结果，分别在result1~5/data/中，算完后取平均值
        average=0
        for j in range(lab_times):
            #result1/client_log/Deadline-client-8M.txt 
            fpath='result'+str(j+1)+'/client_log/'+case+'-client'+'-'+str(bandwidth_num+1-i)+'M.log'
            f=open(fpath,'r')
            average+=canceled_block_num(f)
        y[i-1]=average/lab_times

x = np.arange(1,bandwidth_num+1)
y = np.arange(1.0, bandwidth_num+1)
for i in range(bandwidth_num):
    x[i]=(bandwidth_num-i)*step

case=['DTP','QUIC','Deadline','Priority']
fig, ax = plt.subplots(dpi=200)#figsize=(16, 12)
for i in range(len(case)):
    get_y(case[i],y)
    ax.plot(x,y,label=case[i])

# ax.set_title('Bandwidth high->low',size=24)
ax.set_xlabel('Bandwidth/MBps',size='xx-large')
ax.set_ylabel('cancel events number',size='xx-large')
ax.legend(fontsize='xx-large')
ax.set_xticks(x)
plt.xticks(size='xx-large')
plt.yticks(size='xx-large')
plt.gca().invert_xaxis()
plt.savefig('cancel_event.png',bbox_inches='tight') # 核心是设置成tight