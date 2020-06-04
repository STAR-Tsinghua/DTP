#!/usr/bin/python3
import numpy as np
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import matplotlib.ticker as mtick

lab_times=2 #重复实验次数
step=1 #带宽间隔
resolution=['360p','480p','720p','1080p','1440p']
resolution_num=len(resolution)
deadline=200 #deadline 200ms
block_num=200 #总共发包量
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
    for i in range(1,resolution_num+1):
        # 2020.2.2：每种bandwidth跑了5次，5次实验的实验结果，分别在result1~5/data/中，算完后取平均值
        arr=[]
        for j in range(lab_times):
            fpath='result'+str(j+1)+'/'+case+'-result-'+resolution[i-1]+'.log' #j+1表示实验第几次
            f=open(fpath,'r')
            arr.append(completion_rate(f))
        #求均值
        y[i-1] = np.mean(arr) * 100 # 为了之后百分比显示
        #求方差
        # err[i-1] = np.var(arr) * 100 # 为了之后百分比显示
        #标准差
        err[i-1] = np.std(arr,ddof=1) * 100

x = np.arange(1,resolution_num+1)
y = np.arange(1.0, resolution_num+1)
# x坐标，将'xxxp'转化为xxx
for i in range(resolution_num):
    x[i] = int(resolution[i][:-1])
fig, ax = plt.subplots(dpi=200)#figsize=(16, 12)
case=['DTP','QUIC','Deadline','Priority']
case_draw=['DTP','QUIC','DTP-D','DTP-P']
line_style = ['-','--','-.',':']
for i in range(len(case)):
    err=[0]*len(x)#方差
    get_y(case[i],y,err)
    plt.errorbar(x,y,yerr=err,label=case_draw[i],fmt=line_style[i],capsize=4,linewidth=2)
# 设置百分比显示
fmt = '%.0f%%' # Format you want the ticks, e.g. '40%'
yticks = mtick.FormatStrFormatter(fmt)
ax.yaxis.set_major_formatter(yticks)
# ax.set_title('Bandwidth high->low',size=24)
ax.set_xlabel('Resolution/P',size='xx-large')
ax.set_ylabel('Completion rate before deadline',size='xx-large')
ax.legend(fontsize='xx-large')
ax.set_xticks(x)
plt.xticks(size='xx-large')
plt.yticks(size='xx-large')
plt.ylim(0,)
plt.grid(linestyle='--')  # 生成网格
# plt.show()
plt.savefig('dash.png',bbox_inches='tight') # 核心是设置成tight

