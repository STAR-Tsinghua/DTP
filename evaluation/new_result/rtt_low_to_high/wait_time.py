#!/usr/bin/python3
import numpy as np
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import matplotlib.ticker as mtick

run_times=2 #重复实验次数
step=20 #rtt间隔
rtt_num=10 #假设测5种rtt
deadline=200 #deadline 200ms
block_num=300 #总共发包量
def mean_wait_time(f,rtt,case,times_of_run):
    wait_times=[]
    one_way_delay = rtt/2
    send_time = 0.2/10*1000 #ms
    f.readline()# 第一行：StreamID goodbytes bct BlockSize Priority Deadline
    for line in f.readlines():
        arr=line.split()
        t=arr[2]
        id=arr[0]
        if int(id)<(1000-300)*4:
            continue
        wait_time=float(t)-one_way_delay-send_time
        wait_times.append(wait_time)
    if len(wait_times)>0:
        return np.mean(wait_times)
        # return min(np.mean(wait_times),1000) # 100ms时的QUIC太大了，看不出来趋势
    else:
        print(rtt,case,times_of_run)
        return 200

def get_y(case,y,err):
    for i in range(1,rtt_num+1):
        # 2020.2.2：每种bandwidth跑了5次，5次实验的实验结果，分别在result1~5/data/中，算完后取平均值
        arr=[]
        for j in range(run_times):
            rtt=20*i-20
            fpath='result'+str(j+1)+'/data/'+case+'/'+case+'-rtt'+'-'+str(rtt)+'ms.log'
            f=open(fpath,'r')
            arr.append(mean_wait_time(f,rtt,case,j+1))
        #求均值
        y[i-1] = np.mean(arr)
        #求方差
        # err[i-1] = np.var(arr)
        #标准差
        err[i-1] = np.std(arr,ddof=1)

x = np.arange(1,rtt_num+1)
y = np.arange(1.0, rtt_num+1)
for i in range(rtt_num):
    x[i]=i*step
fig, ax = plt.subplots(dpi=200)#figsize=(16, 12)
case=['DTP','QUIC','Deadline','Priority']
case_draw=['DTP','QUIC','DTP-D','DTP-P']
line_style = ['-','--','-.',':']
for i in range(len(case)):
    err=[0]*len(x)#方差
    get_y(case[i],y,err)
    plt.errorbar(x,y,yerr=err,label=case_draw[i],fmt=line_style[i],capsize=4,linewidth=2)

ax.set_xlabel('RTT/ms',size='xx-large')
ax.set_ylabel('Wait time/ms',size='xx-large')
ax.legend(fontsize='xx-large')
ax.set_xticks(x)
plt.xticks(size='xx-large')
plt.yticks(size='xx-large')
plt.grid(linestyle='--')  # 生成网格
plt.savefig('wait_time.png',bbox_inches='tight') # 核心是设置成tight

