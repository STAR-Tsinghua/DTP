import os
import time
n=4 # 4 situation: QUIC, DTP, Priority, Deadline
f_tc = open('microbenckmark_tc','r')
f_server = open('microbenckmark_server', 'r')
f_client = open('microbenchmark_client','r')
commen_scripts_dir='cd ~/Documents/DTP/evaluation/microbenchmark/commen_scripts;'
os.system("cd ~/Documents/DTP/examples; mkdir result; cd result; mkdir data;mkdir client_log;")
os.system(commen_scripts_dir+"./mkdir.sh;")
for line in f_tc.readlines():
    os.system(line)
    time.sleep(1)
    for i in range(n):
        while True:
            line_client=f_client.readline()
            if line_client != "\n":
                break
        while True:
            line_server=f_server.readline()
            if line_server != "\n":
                break
        line_server=line_server.strip('\n')
        line_client=line_client.strip('\n')
        if i==0 or i==2:
            os.system(commen_scripts_dir+" ./auto.sh "+line_server+" "+line_client+" 1.0 2"+"\n")
        elif i==1:
            os.system(commen_scripts_dir+" ./auto.sh "+line_server+" "+line_client+" 0.0 2"+"\n")
        else:
            os.system(commen_scripts_dir+" ./auto.sh "+line_server+" "+line_client+" 0.5 2"+"\n")
        time.sleep(1)
f_tc.close()
f_server.close()
f_client.close()
