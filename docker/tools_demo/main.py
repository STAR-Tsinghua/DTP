'''
quick start : python3 main.py --ip {dtp_server ip} --server_name dtp_server --client_name dtp_client --network traces_1.txt
'''
import os, platform, json
import time
import numpy as np
import argparse
from qoe import cal_single_block_qoe

# the numbers that you can control
numbers = 60
server_ip = "127.0.0.1"
port = "5555"

# define parser
parser = argparse.ArgumentParser()
parser.add_argument('--ip', type=str, required=False, help="the ip of container_server_name that required")

parser.add_argument('--port', type=str, default="5555",help="the port of dtp_server that required,default is 5555, and you can randomly choose")

parser.add_argument('--numbers', type=int, default=60, help="the numbers of blocks that you can control")

parser.add_argument('--server_name', type=str, required=True, default="dtp_server", help="the container_server_name ")

parser.add_argument('--client_name', type=str, required=True, default="dtp_client", help="the container_client_name ")

parser.add_argument('--network', type=str, default=None, help="the network trace file ")

parser.add_argument('--block', type=str, default=None, help="the block trace file ")

parser.add_argument('--run_path', type=str, default="/home/aitrans-server/", help="the path of aitrans_server")

parser.add_argument('--solution_files', type=str, default=None, help="the path of solution files")

parser.add_argument('--run_times', type=int, default=1, help="The times that you want run repeatly")

parser.add_argument('--enable_print', type=bool, default=False, help="Whether or not print information of processing")

# parse argument
params                = parser.parse_args()
server_ip             = params.ip
port                  = params.port
numbers               = params.numbers
container_server_name = params.server_name
container_client_name = params.client_name
network_trace         = params.network
block_trace           = params.block
docker_run_path       = params.run_path
solution_files        = params.solution_files
run_times             = params.run_times
enable_print          = params.enable_print

# judge system
order_preffix = " " if "windows" in platform.system().lower() else "sudo "
tc_preffix = "" if network_trace else "# "
cur_path = os.getcwd() + '/'
compile_preffix = ''

# move shell scripts to tmp directory
tmp_shell_preffix = "./tmp"
if not os.path.exists(tmp_shell_preffix):
    os.mkdir(tmp_shell_preffix)

# move logs to log diectory
logs_preffix = "./logs"
if not os.path.exists(logs_preffix):
    os.mkdir(logs_preffix)

# check whether local file path is right
if block_trace and not os.path.exists(block_trace):
    raise ValueError("no such block trace in '%s'" % (cur_path + block_trace))
if network_trace and not os.path.exists(network_trace):
    raise ValueError("no such network trace in '%s'" % (cur_path + network_trace))
if solution_files:
    if not os.path.exists(solution_files):
        raise ValueError("no such solution_files in '%s'" % (cur_path + solution_files))
    tmp = os.listdir(solution_files)
    if not "solution.cxx" in tmp:
        raise ValueError("There is no solution.cxx in your solution path : %s" % (cur_path + solution_files))
    if not "solution.hxx" in tmp:
        raise ValueError("There is no solution.hxx in your solution path : %s" % (cur_path + solution_files))
    # if upload files that already finished compile
    compile_preffix = '#' if "libsolution.so" in tmp else ''

# init trace
if block_trace:
    os.system(order_preffix + "docker cp " + block_trace + ' ' + container_server_name + ":%strace/block_trace/aitrans_block.txt" % (docker_run_path))
if network_trace:
    os.system(order_preffix + "docker cp " + network_trace + ' ' + container_server_name + ":%strace/traces.txt" % (docker_run_path))
    os.system(order_preffix + "docker cp " + network_trace + ' ' + container_client_name + ":%strace/traces.txt" % (docker_run_path))
if solution_files:
    os.system(order_preffix + "docker cp " + solution_files + ' ' + container_server_name + ":%sdemo/." % (docker_run_path))

# prepare shell code
def prepare_shell_code():
    client_run = '''
    #!/bin/bash
    cd {0}
    {3} python3 traffic_control.py -load trace/traces.txt > tc.log 2>&1 &
    rm client.log > tmp.log 2>&1
    sleep 0.2
    ./client --no-verify http://{1}:{2}
    {3} python3 traffic_control.py --reset eth0
    '''.format(docker_run_path, server_ip, port, tc_preffix)

    server_run = '''
    #!/bin/bash
    cd {2}
    {3} python3 traffic_control.py -aft 3.1 -load trace/traces.txt > tc.log 2>&1 &

    cd {2}demo
    {5} rm libsolution.so ../lib/libsolution.so
    {5} g++ -shared -fPIC solution.cxx -I include -o libsolution.so > compile.log 2>&1
    cp libsolution.so ../lib

    # check port
    a=`lsof -i:{4} | awk '/server/ {{print$2}}'`
    if [ $a > 0 ]; then
        kill -9 $a
    fi

    cd {2}
    rm log/server_aitrans.log 
    LD_LIBRARY_PATH=./lib ./bin/server {0} {1} trace/block_trace/aitrans_block.txt &> ./log/server_aitrans.log &
    '''.format(server_ip, port, docker_run_path, tc_preffix, port, compile_preffix)

    with open(tmp_shell_preffix + "/server_run.sh", "w", newline='\n')  as f:
        f.write(server_run)

    with open(tmp_shell_preffix + "/client_run.sh", "w", newline='\n') as f:
        f.write(client_run)

# run shell order
order_list = [
    order_preffix + " docker cp ./traffic_control.py " + container_server_name + ":" + docker_run_path,
    order_preffix + " docker cp ./traffic_control.py " + container_client_name + ":" + docker_run_path,
    order_preffix + " docker cp %s/server_run.sh " %(tmp_shell_preffix) + container_server_name + ":" + docker_run_path,
    order_preffix + " docker cp %s/client_run.sh " %(tmp_shell_preffix) + container_client_name + ":" + docker_run_path,
    order_preffix + " docker exec -itd " + container_server_name + " nohup /bin/bash %sserver_run.sh" % (docker_run_path)
]

qoe_sample = []
run_seq = 0
while run_seq < run_times:
    print("The %d round :" % (run_seq))

    print("--restart docker--")
    os.system("docker restart %s %s" % (container_server_name, container_client_name))
    time.sleep(5)
    # get server ip after restart docker
    if not server_ip:
        out = os.popen("docker inspect %s" % (container_server_name)).read()
        out_dt = json.loads(out)
        server_ip = out_dt[0]["NetworkSettings"]["IPAddress"] 

    prepare_shell_code()
    for idx, order in enumerate(order_list):
        print(idx, " ", order)
        os.system(order)

    # ensure server established succussfully
    time.sleep(3)
    print("run client")
    os.system(order_preffix + " docker exec -it " + container_client_name + "  /bin/bash %sclient_run.sh" % (docker_run_path))
    # ensure connection closed
    time.sleep(1)

    stop_server = '''
    #!/bin/bash
    cd {0}
    a=`lsof -i:{1} | awk '/server/ {{print$2}}'`
    if [ $a > 0 ]; then
        kill -9 $a
    fi
    {2} kill `ps -ef | grep python | awk '/traffic_control/ {{print $2}}'`
    {2} python3 traffic_control.py --reset eth0
    '''.format(docker_run_path, port, tc_preffix)

    with open(tmp_shell_preffix + "/stop_server.sh", "w", newline='\n')  as f:
        f.write(stop_server)

    print("stop server")
    os.system(order_preffix + " docker cp %s/stop_server.sh " %(tmp_shell_preffix) + container_server_name + ":%s" % (docker_run_path))
    os.system(order_preffix + " docker exec -it " + container_server_name + "  /bin/bash %sstop_server.sh" % (docker_run_path))
    # move logs
    os.system(order_preffix + " docker cp " + container_client_name + ":%sclient.log %s/." % (docker_run_path, logs_preffix))
    os.system(order_preffix + " docker cp " + container_server_name + ":%slog/server_aitrans.log %s/." % (docker_run_path, logs_preffix))
    os.system(order_preffix + " docker cp " + container_server_name + ":%sdemo/compile.log %s/compile.log" % (docker_run_path, logs_preffix))
    if network_trace:
        os.system(order_preffix + " docker cp " + container_client_name + ":%stc.log %s/client_tc.log" % (docker_run_path, logs_preffix))
        os.system(order_preffix + " docker cp " + container_server_name + ":%stc.log %s/server_tc.log" % (docker_run_path, logs_preffix))
    # move .so file
    os.system(order_preffix + " docker cp " + container_server_name + ":%slib/libsolution.so %s/." % (docker_run_path, logs_preffix))

    # cal qoe
    now_qoe = cal_single_block_qoe("%s/client.log" % (logs_preffix), 0.9)
    # rerun main.py if server fail to start
    with open("%s/client.log" % (logs_preffix), 'r') as f:
        if len(f.readlines()) <= 5:
            if enable_print:
                print("server run fail, begin restart!")
            continue
    qoe_sample.append(now_qoe)
    print("qoe : ", now_qoe)
    run_seq += 1

if run_times > 1:
    print("qoe_sample : ", qoe_sample)