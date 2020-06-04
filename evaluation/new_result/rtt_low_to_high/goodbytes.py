#!/usr/bin/python3

'good bytes: sum of bytes before deadline of all blocks'

import numpy as np
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import matplotlib.ticker as mtick

lab_times=1 #重复实验次数
step=20 #rtt间隔
rtt_num=10 #假设测5种rtt
block_num=300 #总共发包量

def sum_good_bytes(f):
    sum=0
    f.readline()# 第一行：StreamID goodbytes bct BlockSize Priority Deadline
    for line in f.readlines():
        arr=line.split()
        # 文件格式：每一行第一个参数为ID，参数二：deadline之前到的大小，
        # 参数三：为complete time，参数四：BlockSize；
        # 参数五：priority
        good_bytes=arr[1]
        id=arr[0]
        if int(id)>(1000-300)*4:
            sum+=int(good_bytes)
    return sum

def get_y(case,y):
    for i in range(1,rtt_num+1):
        # 2020.2.2：每种bandwidth跑了5次，5次实验的实验结果，分别在result1~5/data/中，算完后取平均值
        average=0
        for j in range(lab_times):
            fpath='result'+str(j+1)+'/data/'+case+'/'+case+'-rtt'+'-'+str(20*i-20)+'ms.log'
            f=open(fpath,'r')
            average+=sum_good_bytes(f)
        y[i-1]=average/lab_times
    return y

x = np.arange(1,rtt_num+1)
y = np.arange(1.0, rtt_num+1)
for i in range(rtt_num):
    x[i]=i*step
fig, ax = plt.subplots(dpi=200)#figsize=(16, 12)
case=['DTP','QUIC','Deadline','Priority']
case_draw=['DTP','QUIC','DTP-D','DTP-P']
line_style = ['-','--','-.',':']
for i in range(4):
    get_y(case[i],y)
    ax.plot(x,y,label=case_draw[i],linestyle=line_style[i])

# ax.set_title('rtt low->high',size=24)
ax.set_xlabel('rtt/ms',size='xx-large')
ax.set_ylabel('Good Bytes',size='xx-large')
ax.legend(fontsize='xx-large')
ax.set_xticks(x)
plt.xticks(size='xx-large')
plt.yticks(size='xx-large')
plt.grid(linestyle='--')  # 生成网格
# plt.show()
plt.savefig('good_bytes.png',bbox_inches='tight') # 核心是设置成tight

