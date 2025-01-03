.PHONY: all clean
default: all

writer: writer.c
	@echo Compiling $@
	@$(CC) $< -o $@

all: writer

clean:
	@rm -f writer