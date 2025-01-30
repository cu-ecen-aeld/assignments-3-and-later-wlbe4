.PHONY: all clean
default: all

CFLAGS += -g -O0 -Wall -Werror
LDFLAGS += -lpthread -lrt
aesdsocket: aesdsocket.c Makefile
	@echo Compiling $@
	@$(CC) $< $(CFLAGS) $(LDFLAGS) -o $@

all: aesdsocket

valgrind: aesdsocket
	@valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes --verbose --log-file=/tmp/valgrind-aesdsocket	./aesdsocket
	@echo "Valgrind log file is written to /tmp/valgrind-aesdsocket"
	@cat /tmp/valgrind-aesdsocket

clean:
	@rm -f aesdsocket