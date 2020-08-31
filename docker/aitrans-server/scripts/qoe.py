import os, sys, inspect
import numpy as np

currentdir = os.path.dirname(os.path.abspath(inspect.getfile(inspect.currentframe())))
sys.path.insert(0, currentdir)

def is_log(block_file):
    return block_file.endswith(".log")

def cal_single_block_qoe(block_file, a):
    with open(block_file, 'r') as f:
        lines = f.readlines()
    start, end = 0, 0
    for idx, line in enumerate(lines):
        if line.startswith("BlockID  bct"):
            start = idx + 1
        if line.startswith(("connection closed")):
            end = idx
    lines[:] = lines[start : end]
    lines[:] = [line.strip() for line in lines]
    lines[:] = [list(map(float, line.split())) for line in lines]
    satisfied_blocks = [line for line in lines if line[1] <= line[4]]
    qoe = 0
    for block in satisfied_blocks:
        qoe += a * (int(block[3]) + 1)
    return qoe

def cal_player_qoe(a):
    files = list(filter(is_log,os.listdir(currentdir)))
    qoes = [cal_single_block_qoe(file, a) for file in files]
    print(qoes)
    return np.mean(qoes)

if __name__ == "__main__":
    # need to put qoe.py in dir of client's log.
    a = 0.9
    res = cal_player_qoe(a)
    print(res)




