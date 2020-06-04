import os
n=4 # 4 situation: QUIC, DTP, Priority, Deadline
f_server = open('microbenckmark_server', 'r')
f_client = open('microbenchmark_client','r')
commen_scripts_dir='cd ~/Documents/DTP/evaluation/microbenchmark/commen_scripts;'
os.system("cd ~/Documents/DTP/examples; mkdir result; cd result; mkdir data;mkdir client_log;")
os.system(commen_scripts_dir+"./mkdir.sh;")
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
        os.system("./auto.sh "+line_server+" "+line_client+" 1.0 2"+"\n") # QUIC/Priority: priority weight=1.0
    elif i==1:
        os.system("./auto.sh "+line_server+" "+line_client+" 0.0 2"+"\n") # Deadline: priority weight=0
    else:
        os.system("./auto.sh "+line_server+" "+line_client+" 0.5 2"+"\n") # DTP: priority weight=0.5
f_server.close()
f_client.close()
