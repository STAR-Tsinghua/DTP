script_file = 'trace_7.sh'
trace_file = 'raw/traces_7.txt'
with open(script_file, 'w') as script_f:
    with open(trace_file, 'r') as trace_f:
        script_f.write("#!/usr/bin/env sh\n")
        last_time = 0
        for raw in trace_f.readlines():
            strs = raw.split(',')
            this_time = float(strs[0])  # s
            rate = float(strs[1])*10.0/9.0  # 然后0.9倍就是设定值了
            loss = float(strs[2])*100  # %
            delay = float(strs[3])*1000  # ms
            # ./rtt_rate_loss.sh 20ms 20ms 4mbps 1 4kb
            script_f.write('./rtt_rate_loss.sh ' + str(delay)+'ms ' + str(delay) +
                           'ms '+str(rate)+'mpbs '+str(loss)+' '+str(rate)+'kb\n')
            # sleep 1
            if this_time % 1.0 != 0:
                print('time error')
            this_time = int(this_time)
            script_f.write('sleep ' + str(this_time-last_time)+'\n')
            last_time = this_time
