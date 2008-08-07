
#include "linkedlist.h"
#include "gdb-dataplugins.h"

static void view_linkedlist(const void *ptr, const GDB_dataplugin_funcs *funcs)
{
    LinkedList item;

    while (ptr)
    {
        if (funcs->readmem(ptr, &item, sizeof (item)) != 0)
            return;

        char *first = (char *) funcs->readstr(item.first, sizeof (char));
        if (first)
        {
            char *last = (char *) funcs->readstr(item.last, sizeof (char));
            if (last)
            {
                funcs->print("item (%p) { \"%s\", \"%s\", %d }%s\n",
                              ptr, first, last, item.office_number,
                              item.next ? "," : "");
                funcs->freemem(last);
            }
            funcs->freemem(first);
        }
        
        ptr = item.next;
    }
}

void GDB_DATAPLUGIN_ENTRY(const GDB_dataplugin_entry_funcs *funcs)
{
    funcs->register_viewer("LinkedList", view_linkedlist);
}

/* end of viewlinkedlist.cpp ... */

