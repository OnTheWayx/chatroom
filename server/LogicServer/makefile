TARGET:=server
SOURCE:=./threadpool/threadpool.c ./serverCs/*.c
CC:=gcc 
CCFLAGS:=-o
LINK:=-lpthread -lsqlite3

$(TARGET):$(SOURCE)
	$(CC) $(SOURCE) $(LINK) $(CCFLAGS) $(TARGET)

.PHONY:clean
clean:
	rm -rf $(TARGET)