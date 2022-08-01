TARGET:=server
SOURCE:=server.c start_server.c ./threadpool/threadpool.c
CC:=gcc 
CCFLAGS:=-o
LINK:=-lpthread -lsqlite3

$(TARGET):$(SOURCE)
	$(CC) $(SOURCE) $(LINK) $(CCFLAGS) $(TARGET)

.PHONY:clean
clean:
	rm -rf $(TARGET)