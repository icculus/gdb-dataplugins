/* don't you dare build this with -fshort-wchar. It handles both types. */

#include <wchar.h>
#include <stdint.h>
#include "gdb-dataplugins.h"

/* app was built with -fshort-wchar */
#define VIEW_WCHAR_T_IMPL(bits) \
static void view_wchar_t_##bits(const void *ptr, const GDB_dataplugin_funcs *funcs) \
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
            cvt[i] = (wchar_t) str[i]; \
        cvt[i] = '\0'; \
        funcs->print("(wchar_t *) L\"%ls\"\n", cvt); \
        funcs->freemem(cvt); \
    } \
    funcs->freemem(str); \
}

VIEW_WCHAR_T_IMPL(8)   /* can this happen? */
VIEW_WCHAR_T_IMPL(16)  /* -fshort-wchar, Windows UCS-2. */
VIEW_WCHAR_T_IMPL(32)  /* Normal Unix UCS-4. */
VIEW_WCHAR_T_IMPL(64)  /* still waiting for UCS-8.  :)  */

void GDB_DATAPLUGIN_ENTRY(const GDB_dataplugin_entry_funcs *funcs)
{
    size_t size = 0;
    if (funcs->getsize("wchar_t", &size) == -1)
        return;

    if (size == 1)
        funcs->register_viewer("wchar_t *", view_wchar_t_8);
    else if (size == 2)
        funcs->register_viewer("wchar_t *", view_wchar_t_16);
    else if (size == 4)
        funcs->register_viewer("wchar_t *", view_wchar_t_32);
    else if (size == 8)
        funcs->register_viewer("wchar_t *", view_wchar_t_64);
    else
        funcs->warning("sizeof (wchar_t) is %d ... we only handle 1, 2, 4, 8.", (int) size);
}

/* end of viewwchar.c ... */


