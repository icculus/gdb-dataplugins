/* don't you dare build this with -fshort-wchar. It handles both types. */

#include <wchar.h>
#include <stdint.h>
#include "gdb-dataplugins.h"

/* app was built with -fshort-wchar */
#define VIEW_WCHAR_T_IMPL(bits) \
static void view_wchar_t_##bits(void *ptr, const GDB_dataplugin_funcs *funcs) \
{ \
    wchar_t *wcptr = 0; \
    uint##bits##_t *str = 0; \
    if (funcs->readmem(ptr, &wcptr, sizeof (wchar_t *)) != 0) return; \
    str = (uint##bits##_t *) funcs->readstr(wcptr, bits / 8); \
    if (str == 0) return; \
    if (sizeof (wchar_t) == sizeof (*str)) { \
        funcs->print("(wchar_t *) L\"%ls\"\n", str); \
    } else { \
        int i; \
        int len = 0; \
        while (str[len]) \
            len++; \
        wchar_t *cvt = funcs->allocmem(sizeof (wchar_t) * (len+1)); \
        for (i = 0; i < len; i++) \
            cvt[i] = (wchar_t) str[len]; \
        cvt[i] = '\0'; \
        funcs->print("(wchar_t *) L\"%ls\"\n", cvt); \
        funcs->freemem(cvt); \
    } \
    funcs->freemem(str); \
}

//VIEW_WCHAR_T_IMPL(16)
VIEW_WCHAR_T_IMPL(32)
//VIEW_WCHAR_T_IMPL(64)

void GDB_DATAPLUGIN_ENTRY(const GDB_dataplugin_entry_funcs *funcs)
{
//    size_t size = 0;
//    if (!funcs->get_type_geometry("wchar_t", &size, NULL))
//        return;

//    if (size == 2)
//        funcs->register_viewer("wchar_t *", view_wchar_t_16);
//    else if (size == 4)
        funcs->register_viewer("wchar_t *", view_wchar_t_32);
//    else if (size == 8)
//        funcs->register_viewer("wchar_t *", view_wchar_t_64);
//    else
//        funcs->warning("sizeof (wchar_t) is %d ... we only handle 2, 4, 8.");
}

/* end of viewwchar.c ... */


