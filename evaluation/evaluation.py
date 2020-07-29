#!/usr/bin/env python3
'''
evaluate players' solution, output the score.
'''

import os

team_name = 'demo'
Traces = ['traces_7'] # traces_17, traces_27
print('evaluation of ', team_name)
# 1.(TODO) compile with this team's solution

# 2. run new server
os.system('./commen_scripts/server_begin.sh')

# for every trace in Traces
# 3. run trace
os.system('./commen_scripts/trace_begin.sh trace_7.sh')

# 4. run client

# 5. kill server
os.system('./commen_scripts/server_end.sh') 

# 6. end trace
os.system('./commen_scripts/trace_end.sh trace_7.sh')