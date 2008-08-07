#include "gdb-dataplugins.h"
#include "hello.h"

static void view_hello(const void *ptr, const GDB_dataplugin_funcs *funcs)
{
    Hello hello;

    if (funcs->readmem(ptr, &hello, sizeof (Hello)) != 0)
        return;

    hello.name = (char *) funcs->readstr(hello.name, sizeof (char));
    if (hello.name == 0)
        return;

    funcs->print("This Hello says heya to %s, %d times!\n",
                 hello.name, hello.times);

    funcs->freemem(hello.name);
}

void GDB_DATAPLUGIN_ENTRY(const GDB_dataplugin_entry_funcs *funcs)
{
    funcs->register_viewer("Hello", view_hello);
}

/* end of viewhello.cpp ... */

