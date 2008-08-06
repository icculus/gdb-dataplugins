#include "gdb-dataplugins.h"
#include "hello.h"

static void view_hello(void *ptr, const GDB_dataplugin_funcs *f)
{
    Hello hello;
    f->warning("warning from inside %s:%d ...", __FUNCTION__, __LINE__);
    if (f->readmem(ptr, &hello, sizeof (Hello)) != 0) return;
    if ((hello.name = f->readstr(hello.name)) == 0) return;
    f->print("This Hello says heya to %s, %d times!\n", hello.name, hello.times);
    f->freemem(hello.name);
}

void GDB_DATAPLUGIN_ENTRY(GDB_dataplugin_register r, GDB_dataplugin_warning w)
{
    w("just testing the warning callback");
    w("another warning, ignore");
    r("Hello", view_hello);
}

