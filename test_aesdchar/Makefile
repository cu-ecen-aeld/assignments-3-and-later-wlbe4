.PHONY: all clean
default: all

CFLAGS += -g -O3 -Wall -Werror
LDFLAGS += -lpthread -lrt
aesdsocket: test_aesdchar.c Makefile
	@echo Compiling $@
	@$(CC) $< $(CFLAGS) $(LDFLAGS) -o $@

all: test_aesdchar

clean:
	@rm -f test_aesdchar