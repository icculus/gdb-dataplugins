all: hello viewhello.so

.PHONY: clean
clean:
	rm -f hello viewhello.so

hello: hello.cpp
	g++ -o hello -g hello.cpp

viewhello.so: viewhello.cpp
	g++ -o viewhello.so -shared -fPIC -Wall -O0 -g viewhello.cpp -I../../src/gdb

