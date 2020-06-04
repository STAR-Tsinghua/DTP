#!/usr/bin/env python3
import numpy as np
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import matplotlib.ticker as mtick

'''
mode change
BBR :: mode change StartUp -> Drain
BBR :: mode change Drain -> ProbeBw
BBR :: mode change ProbeBw -> ProbeRtt
BBR :: mode change ProbeRtt -> ProbeBw
'''

            


fpath='client.log'
y=[]
i=0
with open(fpath,'r') as f:
    for  line in f.readlines():
        if line.find('bw:')!=-1:
            arr=line.split()
            bw=float(arr[2])
            # if i > 10000 and i<20000:
            if True:
                y.append(bw)
            i+=1



fig, ax = plt.subplots(figsize=(16, 9), dpi=200)
x=list(range(len(y)))
plt.scatter(x, y)


plt.savefig('bw.png')