#include <string>
#include <wchar.h>
#include "gdb-dataplugins.h"

static void view_std_string(const void *ptr, const GDB_dataplugin_funcs *funcs)
{
    char buf[sizeof (std::string)];  // we don't actually want to construct.

    if (funcs->readmem(ptr, buf, sizeof (buf)) != 0)
        return;

    // the c_str() call only works if it doesn't want to touch anything we
    //  haven't read from the target process, but it's easier than dealing
    //  with private fields.
    const std::string *stdstr = (const std::string *) buf;
    char *cstr = (char *) funcs->readstr(stdstr->c_str(), sizeof (char));
    if (cstr == 0)
        return;

    funcs->print("(std::string) \"%s\"\n", cstr);
    funcs->freemem(cstr);
}

static void view_std_wstring(const void *ptr, const GDB_dataplugin_funcs *funcs)
{
    char buf[sizeof (std::wstring)];  // we don't actually want to construct.

    if (funcs->readmem(ptr, buf, sizeof (buf)) != 0)
        return;

    const std::wstring *stdwstr = (const std::wstring *) buf;
    wchar_t *wcstr = (wchar_t *) funcs->readstr(stdwstr->c_str(), sizeof (wchar_t));
    if (wcstr == 0)
        return;

    funcs->print("(std::wstring) L\"%ls\"\n", wcstr);
    funcs->freemem(wcstr);
}

void GDB_DATAPLUGIN_ENTRY(const GDB_dataplugin_entry_funcs *funcs)
{
    funcs->register_viewer("std::string", view_std_string);
    funcs->register_viewer("std::wstring", view_std_wstring);
    funcs->register_viewer("string", view_std_string);
    funcs->register_viewer("wstring", view_std_wstring);
}

/* end of viewstdstr.cpp ... */


