#!/usr/bin/python3
import numpy as np
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import matplotlib.ticker as mtick
from new_good_bytes import sum_good_bytes

lab_times = 5  # 重复实验次数
deadline = 200  # deadline 200ms
block_num = 300  # 总共发包量
cases=['QUIC', 'DTP','Deadline','Priority']
case_draw=['QUIC','DTP','DTP-D','DTP-P']
case_hatch=['/', 'x', '-', 'o']
case_color = ['blue','green','red','yellow']

def get_good_bytes(case,bw):
    # 2020.2.2：每种bandwidth跑了5次，5次实验的实验结果，分别在result1~5/data/中，算完后取平均值
    arr=[]
    for j in range(lab_times):
        fpath='result'+str(j+1)+'/server_log/'+case+'-server-'+str(bw)+'M.log'
        arr.append(sum_good_bytes(fpath))
    #求均值
    return np.mean(arr)/1000_000

def payload_send(f):
    sended = 0
    for line in f.readlines():
        if line.find('payload send') != -1:
            s_arr = line.split()
            buf_len = int(s_arr[3])
            # print(line)
            sended += buf_len
    return sended/1000_000


def get_payload_send(case, bw):
    # 2020.2.2：每种bandwidth跑了5次，5次实验的实验结果，分别在result1~5/data/中，算完后取平均值
    average = 0
    for j in range(lab_times):
        # result1/client_log/Deadline-client-8M.txt
        fpath = 'result'+str(j+1)+'/client_log/'+case + \
            '-client'+'-'+str(bw)+'M.log'
        f = open(fpath, 'r')
        average += payload_send(f)
    return average/lab_times


if __name__ == "__main__":
    fig, ax = plt.subplots(dpi=200)  # figsize=(16, 12)

    # 选定3种带宽画图
    bw = [8, 4, 2]

    x = np.arange(1, len(bw)+1)
    y_bad = np.arange(1.0, len(bw)+1)
    y_good = np.arange(1.0, len(bw)+1)
    y_good_rate = np.arange(1.0, len(bw)+1)

    total_width = 0.8
    width = total_width / len(cases)
    x = x - (total_width - width) / 2
    # width = 1
    # total_width = width*len(cases)
    legend_added = False
    # x_ticks = []
    
        
    for i in range(len(cases)):
        for j in range(len(bw)):
            bandwidth=bw[j]
            y_good[j]=get_good_bytes(cases[i],bandwidth)
            y_bad[j]=get_payload_send(cases[i], bandwidth)-y_good[j]
            ##########################
            y_good_rate[j]=y_good[j]/get_payload_send(cases[i], bandwidth)
            ##########################
        # if not legend_added:
        #     plt.bar(x+i*width, y_good, width=width*0.9, hatch='', color='white', edgecolor="#66c2a5", label="good bytes")
        #     legend_added = True
        # else:
        #     plt.bar(x+i*width, y_good, width=width*0.9, hatch='', color='white', edgecolor="#66c2a5")
        # plt.bar(x+i*width, y_bad, width=width*0.9,bottom=y_good, hatch=case_hatch[i], label="bad bytes-"+case_draw[i],edgecolor="#F41F5A", color = 'white' )
        plt.bar(x+i*width, y_good_rate, width=width*0.9, hatch=case_hatch[i], label=case_draw[i],edgecolor=case_color[i], color = 'white' )
        # x_ticks.append(2.5+j*total_width*1.5)
        # if not legend_added:
        #     ax.legend(fontsize='xx-large')
        #     legend_added = True
    
    plt.xticks(x+j*width/2,bw,size='xx-large')
    ax.legend(fontsize='small')


    


    ax.set_xlabel('Bandwidth/MBps',size='xx-large')
    # ax.set_ylabel('Payload size/MB', size='xx-large')
    ax.set_ylabel('Good bytes rate', size='xx-large')
    plt.grid(linestyle='--')  # 生成网格
    plt.yticks(size='xx-large')
    plt.savefig('payload.png', bbox_inches='tight')  # 核心是设置成tight
