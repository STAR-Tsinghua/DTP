#!/usr/bin/python3
block_interval = 50 #50ms
MAX_SEND_TIMES = 200 #每个block 50ms，每秒实验发20个block
def get_config(chunk_size_fpath,config_fpath):
    with open(config_fpath,'w') as config_f:
        config_f.write(str(MAX_SEND_TIMES)+'\n')
        with open(chunk_size_fpath,'r') as f:
            block_num = 0
            for line in f.readlines():
                chunk_size=int(line.strip())
                block_size = int(10*chunk_size/(4000/block_interval))
                for _ in range(int(4000/block_interval/2)):
                    config_f.write('200    0     '+str(block_size)+'\n')
                    config_f.write('200    1     '+str(block_size)+'\n')
                    block_num+=2
                    if block_num>=MAX_SEND_TIMES:
                        return

resolution=['240', '360', '480', '720' ,'1080','1440']
for i in range(len(resolution)):
    chunk_size_fpath='video_size_'+str(i)
    config_fpath = resolution[i]+'p.txt' #{240; 360; 480; 720; 1080; 1440}p
    get_config(chunk_size_fpath,config_fpath)


