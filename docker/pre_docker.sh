#!/bin/bash
# prepare aitrans-server folder

# demo sourse files
cp ../src/aitrans/solution.cxx aitrans-server/demo 
cp ../src/aitrans/include/solution.hxx aitrans-server/demo
trash aitrans-server/log/*
#client
cp ../examples/client aitrans-server
#server
cp ../examples/server aitrans-server/bin/
#trace