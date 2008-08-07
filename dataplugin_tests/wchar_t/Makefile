all: viewwchar.so wchar

.PHONY: clean
clean:
	rm -f wchar viewwchar.so

wchar: wchar.c
	gcc -o wchar -Wall -O0 -g wchar.c

viewwchar.so: viewwchar.c
	gcc -o viewwchar.so -shared -fPIC -Wall -O0 -g viewwchar.c -I../../src/gdb

# end of Makefile ...

