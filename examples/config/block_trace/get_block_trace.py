audio_data_file="data_audio.csv"
video_data_file="data_video.csv"
trace_file="aitrans_block.txt"
last_time = 0.0


# 根据data_audio/data_video读入的line，计算创建时间
def get_time(block_line):
    create_time=block_line.split(',')[0]
    # print(create_time)
    return float(create_time)


# 把line转化成决赛系统的config格式：1.空格隔开各个参数 2.第一个参数改为时间间隔
def write_trace(line):
    global last_time
    strs = line.split(',')
    res=""
    i=0
    this_time = float(strs[0])
    time_gap = str(this_time-last_time)
    strs[0] = time_gap
    last_time = this_time
    for s in strs:
        res += s
        if i==0:
            res += '    '+'200'
        if i<2:
            res += '    '
        i+=1
    return res

with open(audio_data_file,'r') as audio_f:
    with open(video_data_file,'r') as video_f:
        with open(trace_file,'w') as trace_f:
            video_line = video_f.readline()
            for audio_line in audio_f.readlines():
                # if len(audio_line)==0:
                #     audio_create_time = 100000
                #     print("audio_create_time = 100000")
                # else:
                #     audio_create_time = get_time(audio_line)
                audio_create_time = get_time(audio_line)
                # video输出完后，len(video_line)==0，故会跳过；audio先结束，则video会有剩余
                while len(video_line)>0 and get_time(video_line)<audio_create_time:
                    trace_f.write(write_trace(video_line))
                    video_line = video_f.readline()
                trace_f.write(write_trace(audio_line))
            #若video有剩余，全部输出
            while len(video_line)>0:
                trace_f.write(write_trace(video_line))
                video_line = video_f.readline()
