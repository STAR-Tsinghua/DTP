#!/bin/bash
# prepare aitrans-server folder

# demo sourse files
sudo cp ../src/aitrans/solution.cxx aitrans-server/demo 
sudo cp ../src/aitrans/include/solution.hxx aitrans-server/demo
sudo trash aitrans-server/log/*
#client
    # follow the readme to get client execuatble file
#server
sudo mkdir aitrans-server/bin
sudo cp ../examples/server aitrans-server/bin
#trace
cd ./block_trace
python3 get_block_trace.py
sudo mkdir ../aitrans-server/trace
sudo mkdir ../aitrans-server/trace/block_trace
sudo mv ./aitrans_block.txt ../aitrans-server/trace/block_trace
cd ..
#certs
sudo cp ../examples/cert.crt aitrans-server
sudo cp ../examples/cert.key aitrans-server
# lib
# Although the lib file is recompiled during the test by testing tool in tools_demo
# It's better to copy it into the directory to avoid something bad from happening
sudo mkdir aitrans-server/lib
sudo cp ../examples/build/release/libsolution.so aitrans-server/lib/libsolution.so