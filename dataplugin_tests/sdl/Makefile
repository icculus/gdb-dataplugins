all: viewsdl.so

SDLFLAGS=$(strip $(shell sdl-config --cflags --libs))

.PHONY: clean
clean:
	rm -f viewsdl.so

viewsdl.so: viewsdl.c
	gcc -o viewsdl.so -shared -fPIC -Wall -O0 -g viewsdl.c -I../../src/gdb $(SDLFLAGS)

# end of Makefile ...

