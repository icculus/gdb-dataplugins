#include <wchar.h>

int main(void)
{
    wchar_t *x = L"Hello, world!";
    wchar_t *y = L"blah blah blah";
    return (x != y) ? 0 : 1;
}

/* end of wchar.c ... */

