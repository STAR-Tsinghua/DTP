sudo docker stop aitrans_server
sudo docker stop aitrans_client
sudo docker rm aitrans_server
sudo docker rm aitrans_client
sudo docker run --privileged -dit --name aitrans_server aitrans_ubuntu:0.0.2
sudo docker run --privileged -dit --name aitrans_client aitrans_ubuntu:0.0.2
cd tools_demo && python main.py --server_name aitrans_server --client_name aitrans_client
