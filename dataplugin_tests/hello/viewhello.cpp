#include "gdb-dataplugins.h"
#include "hello.h"

static void view_hello(void *ptr, const GDB_dataplugin_funcs *funcs)
{
    Hello hello;

    funcs->warning("warning from inside %s, %s:%d ...",
                    __FUNCTION__, __FILE__, __LINE__);

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
    funcs->warning("just testing the warning callback");
    funcs->warning("another warning, ignore");
    funcs->register_viewer("Hello", view_hello);
}

/* end of viewhello.cpp ... */

