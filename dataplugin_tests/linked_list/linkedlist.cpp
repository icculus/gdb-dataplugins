#include <string.h>
#include "linkedlist.h"

static LinkedList *list = NULL;

static char *xstrdup(const char *str)
{
    const size_t len = strlen(str);
    char *retval = new char[len+1];
    strcpy(retval, str);
    return retval;
}

static void AddToList(const char *first, const char *last, int office)
{
    LinkedList *item = new LinkedList;
    item->first = xstrdup(first);
    item->last = xstrdup(last);
    item->office_number = office;
    item->next = list;
    list = item;
}

int main(void)
{
    AddToList("Ryan", "Gordon", 4923);
    AddToList("Greg", "Read", 227);
    AddToList("Linus", "Torvalds", 732);
    AddToList("Bill", "Gates", 666);  // I kid! I kid!
    AddToList("Richard", "Stallman", 1135);
    AddToList("Andrew", "Tannenbaum", 9986);

    // now leak all that memory.  :)
    return 0;
}

/* end of linkedlist.cpp ... */


