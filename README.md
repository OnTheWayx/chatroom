# chatroom
CS
服务器分为两个服务进程
server进程和fileserver进程
server进程负责处理客户端的逻辑业务及逻辑业务数据包
fileserver进程负责处理客户端与服务器之间的文件传输


server进程基于 epoll + 线程池架构编写，采用高并发的方式
fileserver进程基于多进程架构编写，单个进程处理单个客户端的文件传输


client为1.0版本客户端
new_client2.0版本为2.0客户端

更新版本性能更高，支持后台下载文件
