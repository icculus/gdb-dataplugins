#include <string>

int main(void)
{
    std::string x("Hello, world!");
    std::string y("blah blah blah");
    return (strlen(x.c_str()) != strlen(y.c_str())) ? 0 : 1;
}

/* end of viewstdstr.cpp ... */


