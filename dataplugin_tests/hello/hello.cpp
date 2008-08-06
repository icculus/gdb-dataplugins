#include <stdio.h>

class Hello
{
public:
	char *name;
	int times;
};

int main(void)
{
	int i;
	struct Hello hello;
	hello.name = (char *) "bob";
	hello.times = 4;

	for (i = 0; i < hello.times; i++)
		printf("Hello, %s!\n", hello.name);

	return 0;
}


