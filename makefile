.PHONY: all clean
BIN=chatsrv_poll chatcli_poll
all:$(BIN)
%.o:%.cc
clean:
	rm -rf *.o $(BIN)
