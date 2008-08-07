all: viewstdstr.so stdstr

.PHONY: clean
clean:
	rm -f stdstr viewstdstr.so

stdstr: stdstr.cpp
	g++ -o stdstr -Wall -O0 -g stdstr.cpp

viewstdstr.so: viewstdstr.cpp
	g++ -o viewstdstr.so -shared -fPIC -Wall -O0 -g viewstdstr.cpp -I../../src/gdb

# end of Makefile ...

