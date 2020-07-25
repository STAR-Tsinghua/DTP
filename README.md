# Usage
### Running
```bash
$ ./server 127.0.0.1 5555 trace/block_trace/aitrans_block.txt &> server_aitrans.log
$ ./client --no-verify http://127.0.0.1:5555 &> client.log
```