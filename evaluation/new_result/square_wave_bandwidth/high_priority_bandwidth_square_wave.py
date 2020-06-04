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
Priority_num=3 #Priority number
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
    Block = [999 for n in range(MAX_SEND_TIMES)]
    
    # 统计每个block到达的时间
    for line in f.readlines():
        arr=line.split()
        # 文件格式：每一行第一个参数为ID，第三个参数为离发出的ms
        block_id=int(arr[0])
        block_num=int(block_id/4-1)
        t=int(arr[2])
        Block[block_num]=t

    sum=0
    block_num=0
    i=0
    for block_num in range(MAX_SEND_TIMES):
        if block_num%3==0 and Block[block_num]<=200:
            sum+=1
        # 30 blocks each sec
        if (block_num+1)%30==0:
            y[i]=sum/(30.0/Priority_num)
            i+=1
            sum=0
    y=y*100 # 为了之后百分比显示
    ax.plot(x,y,label=c_draw,linestyle=ls,linewidth=2)


# 设置百分比显示
fmt = '%.0f%%' # Format you want the ticks, e.g. '40%'
yticks = mtick.FormatStrFormatter(fmt)
ax.yaxis.set_major_formatter(yticks)
# ax.set_title('Bandwidth Square Wave')
ax.set_xlabel('Time/s',size='xx-large')
ax.set_ylabel('High priority block completion rate',size='xx-large')
plt.xlim(0)
plt.ylim(0)
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
        y1[i]=200
        # if (x1[i])%5<0.02:
        #     plt.text(x1[i],y1[i]+1,"12MBps",fontsize='xx-large')
    else:
        y1[i]=100*2/3
        # if x1[i]%5<0.02:
        #     plt.text(x1[i],y1[i]+1,"4MBps",fontsize='xx-large')

ax.fill_between(x1,y1,y2,color='#000000',where=y1>y2,interpolate=True,alpha=0.1)
plt.savefig('high_bandwidth_square_wave.png',bbox_inches='tight') # 核心是设置成tight






