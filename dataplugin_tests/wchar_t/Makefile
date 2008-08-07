all: viewwchar.so wchar32 wchar16

.PHONY: clean
clean:
	rm -f wchar32 wchar16 viewwchar.so

wchar32: wchar.c
	gcc -o wchar32 -Wall -O0 -g wchar.c

wchar16: wchar.c
	gcc -o wchar16 -Wall -O0 -g wchar.c -fshort-wchar

viewwchar.so: viewwchar.c
	gcc -o viewwchar.so -shared -fPIC -Wall -O0 -g viewwchar.c -I../../src/gdb

# end of Makefile ...

