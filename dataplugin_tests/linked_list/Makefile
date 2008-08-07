all: viewlinkedlist.so linkedlist

.PHONY: clean
clean:
	rm -f linkedlist viewlinkedlist.so

linkedlist: linkedlist.cpp
	g++ -o linkedlist -Wall -O0 -g linkedlist.cpp

viewlinkedlist.so: viewlinkedlist.cpp
	g++ -o viewlinkedlist.so -shared -fPIC -Wall -O0 -g viewlinkedlist.cpp -I../../src/gdb

# end of Makefile ...

