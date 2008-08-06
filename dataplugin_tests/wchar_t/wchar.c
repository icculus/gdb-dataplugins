#include <wchar.h>

int main(void)
{
    wchar_t *x = L"Hello, world!";
    wchar_t *y = L"blah blah blah";
    return (wcslen(x) != wcslen(y)) ? 0 : 1;
}

