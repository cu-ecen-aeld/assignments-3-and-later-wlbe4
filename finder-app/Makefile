.PHONY: all clean
default: all

writer: writer.c
	@echo Compiling $@
	@${CROSS_COMPILE}gcc $< -o $@

all: writer

clean:
	@rm writer