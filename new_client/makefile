TARGET:=client
SOURCE:=client.c start_client.c filetransmit.c
CC:=gcc
CCFLAGS:=-o
LINK:=-lpthread

$(TARGET):$(SOURCE)
	$(CC) $(SOURCE) $(CCFLAGS) $(TARGET) $(LINK)

.PHONY:clean
clean:
	rm -rf $(TARGET)