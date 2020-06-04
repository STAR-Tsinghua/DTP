#!/usr/bin/python3
import numpy as np
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import matplotlib.ticker as mtick
import math

lab_times=5 #重复实验次数
step=1 #带宽间隔
bandwidth_num=9 #假设测5种带宽
deadline=200 #deadline 200ms
block_num=300 #总共发包量
def mean_wait_time(f,bw,case,lab_times):
    wait_times=[]
    f.readline()# 第一行：StreamID goodbytes bct BlockSize Priority Deadline
    for line in f.readlines():
        arr=line.split()
        t=arr[2]
        one_way_delay = 0
        send_time = 0.2/bw*1000 #ms
        wait_time=float(t)-one_way_delay-send_time
        # wait_time_log=math.log(wait_time)
        wait_times.append(wait_time)
    if len(wait_times)>0:
        return np.mean(wait_times)
    else:
        print(bw,case,lab_times)
        return 200
def get_y(case,y,err):        
    for i in range(1,bandwidth_num+1):
        # 2020.2.2：每种bandwidth跑了5次，5次实验的实验结果，分别在result1~5/data/中，算完后取平均值
        arr=[]
        for j in range(lab_times):
            bw = bandwidth_num+2-i
            fpath='result'+str(j+1)+'/data/'+case+'/'+case+'-bandwidth'+'-'+str(bw)+'M.log'
            f=open(fpath,'r')
            arr.append(mean_wait_time(f,bw,case,j+1))
        #求均值
        y[i-1] = np.mean(arr)
        #求方差
        # err[i-1] = np.var(arr)
        #标准差
        err[i-1] = np.std(arr,ddof=1)

x = np.arange(1,bandwidth_num+1)
y = np.arange(1.0, bandwidth_num+1)
for i in range(bandwidth_num):
    x[i]=(bandwidth_num-i)*step+1
fig, ax = plt.subplots(dpi=200)#figsize=(16, 12)
case=['DTP','QUIC','Deadline','Priority']
case_draw=['DTP','QUIC','DTP-D','DTP-P']
line_style = ['-','--','-.',':']
for i in range(len(case)):
    err=[0]*len(x)#方差
    get_y(case[i],y,err)
    # ax.plot(x,y,label=case_draw[i],linestyle=line_style[i])
    plt.semilogy(x,y,label=case_draw[i],linestyle=line_style[i],linewidth=2)
# ax.set_title('Bandwidth high->low',size=24)
ax.set_xlabel('Bandwidth/MBps',size='xx-large')
ax.set_ylabel('Wait time/ms',size='xx-large')
ax.legend(fontsize='xx-large')
ax.set_xticks(x)
plt.gca().invert_xaxis()
plt.xticks(size='xx-large')
plt.yticks(size='xx-large')
plt.grid(linestyle='--')  # 生成网格
# plt.show()
plt.savefig('wait_time.png',bbox_inches='tight') # 核心是设置成tight

