#!/usr/bin/env python3
import numpy as np
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import matplotlib.ticker as mtick



            


fpath='client.log'
y=[]
i=0
with open(fpath,'r') as f:
    for  line in f.readlines():
        if line.find('RTT:')!=-1:
            arr=line.split()
            bw=float(arr[2])
            # if i > 5000 and i<10000:
            if True:
                y.append(bw)
            i+=1



fig, ax = plt.subplots(figsize=(16, 9), dpi=200)
x=list(range(len(y)))
plt.scatter(x, y)


plt.savefig('rtt.png')