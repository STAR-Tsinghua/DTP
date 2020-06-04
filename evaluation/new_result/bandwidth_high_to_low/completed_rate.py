#!/usr/bin/python3
import numpy as np
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import matplotlib.ticker as mtick

lab_times=5 #重复实验次数
step=1 #带宽间隔
bandwidth_num=10 #假设测5种带宽
deadline=200 #deadline 200ms
block_num=300 #总共发包量
def completion_rate(f):
    sum=0
    f.readline()# 第一行：StreamID goodbytes bct BlockSize Priority Deadline
    for line in f.readlines():
        arr=line.split()
        t=arr[2]
        if float(t)<200:
            sum+=1
    return sum/block_num
def get_y(case,y,err):        
    for i in range(1,bandwidth_num+1):
        # 2020.2.2：每种bandwidth跑了5次，5次实验的实验结果，分别在result1~5/data/中，算完后取平均值
        arr=[]
        for j in range(lab_times):
            fpath='result'+str(j+1)+'/data/'+case+'/'+case+'-bandwidth'+'-'+str(bandwidth_num+1-i)+'M.log'
            f=open(fpath,'r')
            arr.append(completion_rate(f))
        #求均值
        y[i-1] = np.mean(arr) * 100 # 为了之后百分比显示
        #求方差
        # err[i-1] = np.var(arr) * 100 # 为了之后百分比显示
        #标准差
        err[i-1] = np.std(arr,ddof=1) * 100

x = np.arange(1,bandwidth_num+1)
y = np.arange(1.0, bandwidth_num+1)
for i in range(bandwidth_num):
    x[i]=(bandwidth_num-i)*step
fig, ax = plt.subplots(dpi=200)#figsize=(16, 12)
case=['DTP','QUIC','Deadline','Priority']
case_draw=['DTP','QUIC','DTP-D','DTP-P']
line_style = ['-','--','-.',':']
for i in range(len(case)):
    err=[0]*len(x)#方差
    get_y(case[i],y,err)
    # ax.plot(x,y,label=case_draw[i],linestyle=line_style[i])
    plt.errorbar(x,y,yerr=err,label=case_draw[i],fmt=line_style[i],capsize=4,linewidth=2)
# 设置百分比显示
fmt = '%.0f%%' # Format you want the ticks, e.g. '40%'
yticks = mtick.FormatStrFormatter(fmt)
ax.yaxis.set_major_formatter(yticks)
# ax.set_title('Bandwidth high->low',size=24)
ax.set_xlabel('Bandwidth/MBps',size='xx-large')
ax.set_ylabel('Completion rate before deadline',size='xx-large')
ax.legend(fontsize='xx-large')
ax.set_xticks(x)
plt.gca().invert_xaxis()
plt.xticks(size='xx-large')
plt.yticks(size='xx-large')
plt.grid(linestyle='--')  # 生成网格
# plt.show()
plt.savefig('completion_rate.png',bbox_inches='tight') # 核心是设置成tight

