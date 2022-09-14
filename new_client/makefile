TARGET:=client
SOURCE:=client.c client.h start_client.c
CC:=gcc
CCFLAGS:=-o
LINK:=-lpthread

$(TARGET):$(SOURCE)
	$(CC) $(SOURCE) $(LINK) $(CCFLAGS) $(TARGET)

.PHONY:clean
clean:
	rm -rf $(TARGET)