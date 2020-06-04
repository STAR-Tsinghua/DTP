#!/usr/bin/python3
import os
n = 4  # 4 situation: QUIC, DTP, Priority, Deadline

commen_scripts_dir = 'cd ~/Documents/DTP/evaluation/microbenchmark/commen_scripts;'
# os.system("cd ~/Documents/DTP/examples; mkdir result; cd result; mkdir client_log;")
# os.system(commen_scripts_dir+"./mkdir.sh;")
# Resolutions = ['360p', '480p', '720p', '1080p', '1440p']
# Cases = ['QUIC', 'Deadline', 'Priority', 'DTP']
Resolutions = ['1440p']
Cases = ['DTP']
print('\nbegin\n')
for case in Cases:
    print('\n#', case)
    print('# {360; 480; 720; 1080; 1440}p')
    for resolu in Resolutions:
        # server param
        result_log = 'result/'+case+'-result-'+resolu+'.log'#result/QUIC-result-360p.log
        server_log = 'result/server_log/'+case+'-server-'+resolu+'.log'
        print('./server 192.168.10.2 5555 1> '+ result_log + ' 2> '+server_log)

        # client param
        if case == 'QUIC':
            config_dir = 'config/dash_chunk_size_QUIC/'
        else:
            config_dir = 'config/dash_chunk_size/'
        config = config_dir+resolu+'.txt'
        if case == 'QUIC' or case == 'Priority':
            priority_weight = '1.0'
        elif case == 'Deadline':
            priority_weight = '0.0'
        else:
            priority_weight = '0.5'
        min_piriority = '2'
        client_log = 'result/client_log/'+case+'-client-'+resolu+'.log'
        # print('# ./client_trace 192.168.10.2 5555 '+config,
        #       priority_weight, min_piriority, '&>', client_log)

        # run 
        os.system('./run.sh '+config+' '+ priority_weight +' '+min_piriority+' '+client_log+' '+result_log+' '+server_log)
    print()
print('\nend\n')
# f_server = open('microbenckmark_server', 'r')
# f_client = open('microbenchmark_client','r')
# for i in range(n):
#     while True:
#         line_client=f_client.readline()
#         if line_client != "\n":
#             break
#     while True:
#         line_server=f_server.readline()
#         if line_server != "\n":
#             break
#     line_server=line_server.strip('\n')
#     line_client=line_client.strip('\n')
#     if i==0 or i==2:
#         os.system("./auto.sh "+line_server+" "+line_client+" 1.0 2"+"\n") # QUIC/Priority: priority weight=1.0
#     elif i==1:
#         os.system("./auto.sh "+line_server+" "+line_client+" 0.0 2"+"\n") # Deadline: priority weight=0
#     else:
#         os.system("./auto.sh "+line_server+" "+line_client+" 0.5 2"+"\n") # DTP: priority weight=0.5
# f_server.close()
# f_client.close()
