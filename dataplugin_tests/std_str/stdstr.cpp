#include <string>

int main(void)
{
    std::string x("Hello, world!");
    std::string y("blah blah blah");
    std::wstring z(L"This is a wide string.");
    return (wcslen(z.c_str()) != (strlen(x.c_str()) + strlen(y.c_str()))) ? 0 : 1;
}

/* end of viewstdstr.cpp ... */


