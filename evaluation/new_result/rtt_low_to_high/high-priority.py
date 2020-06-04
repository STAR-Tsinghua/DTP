#!/usr/bin/python3
import numpy as np
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import matplotlib.ticker as mtick

lab_times=2 #重复实验次数
step=20 #rtt间隔
rtt_num=10 #假设测5种rtt
deadline=200 #deadline 200ms
block_num=300 #总共发包量
Priority_num=3 #Priority number
def completion_rate(f):
    sum=0
    f.readline()# 第一行：StreamID goodbytes bct BlockSize Priority Deadline
    for line in f.readlines():
        arr=line.split()
        # 判断高优先级：根据第5个参数
        t=int(arr[2])
        block_id=int(arr[0])
        if (block_id/4-1)%3==0 and t<100 and block_id>(1000-300)*4:
            sum+=1
    return sum/(block_num/Priority_num)
def get_y(case,y,err):
    for i in range(1,rtt_num+1):
        # 2020.2.2：每种bandwidth跑了5次，5次实验的实验结果，分别在result1~5/data/中，算完后取平均值
        arr=[]
        for j in range(lab_times):
            fpath='result'+str(j+1)+'/data/'+case+'/'+case+'-rtt'+'-'+str(20*i-20)+'ms.log'
            f=open(fpath,'r')
            # 文件格式：每一行第一个参数为ID，参数二：deadline之前到的大小，
            # 参数三：为complete time，参数四：BlockSize；
            # 参数五：priority
            arr.append(completion_rate(f))
        #求均值
        y[i-1] = np.mean(arr) * 100 # 为了之后百分比显示
        #求方差
        # err[i-1] = np.var(arr) * 100 # 为了之后百分比显示
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
for i in range(len(case)):
    err=[0]*len(x)#方差
    get_y(case[i],y,err)
    plt.errorbar(x,y,yerr=err,label=case_draw[i],fmt=line_style[i],capsize=4, linewidth=2)
# 设置百分比显示
fmt = '%.0f%%' # Format you want the ticks, e.g. '40%'
yticks = mtick.FormatStrFormatter(fmt)
ax.yaxis.set_major_formatter(yticks)
# ax.set_title('rtt low->high',size=24)
ax.set_xlabel('RTT/ms',size='xx-large')
ax.set_ylabel('Completion rate before deadline',size='xx-large')
ax.legend(fontsize='medium')
ax.set_xticks(x)
plt.xticks(size='xx-large')
plt.yticks(size='xx-large')
plt.grid(linestyle='--')  # 生成网格
plt.savefig('High-Priority.png',bbox_inches='tight') # 核心是设置成tight

