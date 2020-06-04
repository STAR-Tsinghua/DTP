#!/bin/bash
cd /home/dtp/Documents/DTP/evaluation/microbenchmark/data_bandwidth_high_to_low;
python3 auto_benchmark.py;
cd /home/dtp/Documents/DTP/examples; mv result result1;
ssh jie@192.168.10.2 "cd ~/Documents/DTP/examples; mv result result1"

cd /home/dtp/Documents/DTP/evaluation/microbenchmark/data_bandwidth_high_to_low;
python3 auto_benchmark.py;
cd /home/dtp/Documents/DTP/examples; mv result result2;
ssh jie@192.168.10.2 "cd ~/Documents/DTP/examples; mv result result2"

cd /home/dtp/Documents/DTP/evaluation/microbenchmark/data_bandwidth_high_to_low;
python3 auto_benchmark.py;
cd /home/dtp/Documents/DTP/examples; mv result result3;
ssh jie@192.168.10.2 "cd ~/Documents/DTP/examples; mv result result3"

cd /home/dtp/Documents/DTP/evaluation/microbenchmark/data_bandwidth_high_to_low;
python3 auto_benchmark.py;
cd /home/dtp/Documents/DTP/examples; mv result result4;
ssh jie@192.168.10.2 "cd ~/Documents/DTP/examples; mv result result4"

cd /home/dtp/Documents/DTP/evaluation/microbenchmark/data_bandwidth_high_to_low;
python3 auto_benchmark.py;
cd /home/dtp/Documents/DTP/examples; mv result result5;
ssh jie@192.168.10.2 "cd ~/Documents/DTP/examples; mv result result5"

echo 'end'