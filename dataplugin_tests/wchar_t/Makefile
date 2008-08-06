all: viewwchar.so

.PHONY: clean
clean:
	rm -f viewwchar.so

viewwchar.so: viewwchar.c
	gcc -o viewwchar.so -shared -fPIC -Wall -O0 -g viewwchar.c -I../../src/gdb

# end of Makefile ...

