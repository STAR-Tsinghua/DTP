#!/usr/bin/python3
import numpy as np
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import matplotlib.ticker as mtick
from matplotlib.pyplot import MultipleLocator
#从pyplot导入MultipleLocator类，这个类用于设置刻度间隔

deadline=200
MAX_SEND_TIMES=600
fig, ax = plt.subplots(dpi=200)#figsize=(16, 12)
case=['DTP','QUIC','Deadline','Priority']
case_draw=['DTP','QUIC','DTP-D','DTP-P']
line_style = ['-','--','-.',':']
x = np.arange(1.0,21.0,1.0)
y = np.arange(1.0,21.0,1.0)
for i in range(len(case)):
    c=case[i]
    c_draw=case_draw[i]
    ls=line_style[i]
    fpath='result/data/'+c+'/'+c+'-square-wave.log'
    f=open(fpath,'r')
    f.readline()# 第一行：StreamID goodbytes bct BlockSize Priority Deadline
    Block = [999999 for n in range(MAX_SEND_TIMES)] #bct全部初始化为999 ms
    
    # 统计每个block到达的时间
    for line in f.readlines():
        arr=line.split()
        # 文件格式：每一行第一个参数为ID，第三个参数为离发出的ms
        block_id=int(arr[0])
        block_num=int(block_id/4-1)
        t=int(arr[2])
        Block[block_num]=t

    wait_time=[]
    one_way_delay = 0
    bw = 0 # 12MB or 4MB
    block_num=0
    i=0
    for block_num in range(MAX_SEND_TIMES):
        # if Block[block_num]<=200:
        #     sum+=1
        if Block[block_num]!=999999:
            if block_num < 150 or ( 300 <= block_num < 450):
                bw = 12
            else:
                bw = 4
            send_time = 0.2/bw*1000 #ms
            wait_time.append(Block[block_num] - one_way_delay - send_time)
        # 30 blocks each sec
        if (block_num+1)%30==0:
            y[i]=np.mean(wait_time)
            i+=1
            # print(wait_time)
            wait_time=[]
    # ax.plot(x,y,label=c_draw,linestyle=ls)
    ax.semilogy(x,y,label=c_draw,linestyle=ls,linewidth=2)

ax.set_xlabel('Time/s',size='xx-large')
ax.set_ylabel('Wait time/ms',size='xx-large')
plt.xlim(0)
plt.ylim(1,100000)
# plt.ylim(0,5000)
ax.legend(fontsize='large')
ax.set_xticks(x)

x_major_locator=MultipleLocator(5)
ax.xaxis.set_major_locator(x_major_locator)

plt.xticks(size='xx-large')
plt.yticks(size='xx-large')

# 画方波
x1 = np.linspace(0,20,1000)
y1 = np.linspace(0,0,1000)
y2 = np.linspace(0,0,1000)
for i in range(x1.size):
    if (x1[i])%10<5:
        y1[i]=2000000
        # # 标注带宽
        # if (x1[i])%5<0.02:
        #     plt.text(x1[i],y1[i]+1,"12MBps",fontsize='xx-large')
    else:
        y1[i]=100*2/3
        # # 标注带宽
        # if x1[i]%5<0.02:
        #     plt.text(x1[i],y1[i]+1,"4MBps",fontsize='xx-large')

ax.fill_between(x1,y1,y2,color='#000000',where=y1>y2,interpolate=True,alpha=0.1)



plt.savefig('wait_time.png',bbox_inches='tight') # 核心是设置成tight






