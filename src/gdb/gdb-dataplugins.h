/* Public interfaces for GDB data plugins.

   Copyright (C) 1990, 1991, 1992, 1993, 1994, 1995, 1996, 1997, 1998, 1999,
   2000, 2001, 2002, 2003, 2004, 2005, 2006, 2007, 2008
   Free Software Foundation, Inc.

   Contributed by Ryan C. Gordon.

   This file is part of GDB.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.

   This file has a runtime exception to the GPL: you may use this header
   in a GDB data plugin that is not licensed under the GPL.
*/

#if !defined (GDB_DATAPLUGINS_H)
#define GDB_DATAPLUGINS_H

#ifdef __cplusplus
extern "C" {
#endif

/* callback for outputting data from a data plugin. printf_unfiltered(). */
typedef void (*GDB_dataplugin_printfn)(const char *fmt, ...);

/* callback for reading memory from debuggee address space to debugger. */
typedef int (*GDB_dataplugin_readmemfn)(void *src, void *dst, int len);

/* callback for reading a null-terminated string from debuggee address space to debugger. */
typedef char *(*GDB_dataplugin_readstrfn)(void *src);

/* callback for allocating memory. */
typedef void *(*GDB_dataplugin_mallocfn)(int len);

/* callback for reallocating memory. */
typedef void *(*GDB_dataplugin_reallocfn)(void *ptr, int len);

/* callback for freeing memory allocated by other function pointers. */
typedef void (*GDB_dataplugin_freefn)(void *ptr);

/* callback for reporting a problem inside a data plugin. warning(). */
typedef void (*GDB_dataplugin_warning)(const char *, ...);

typedef struct
{
    GDB_dataplugin_warning warning;
    GDB_dataplugin_printfn print;
    GDB_dataplugin_readmemfn readmem;
    GDB_dataplugin_readstrfn readstr;
    GDB_dataplugin_mallocfn allocmem;
    GDB_dataplugin_reallocfn reallocmem;
    GDB_dataplugin_freefn freemem;
} GDB_dataplugin_funcs;

/* function pointer where data plugins do their work. */
typedef void (*GDB_dataplugin_viewfn)(void *, const GDB_dataplugin_funcs *);

/* callback for registering a viewer for a specific data type. */
typedef void (*GDB_dataplugin_register)(const char *, GDB_dataplugin_viewfn);

/* the entry point into a data plugin shared library. */
typedef void (*GDB_dataplugin_entry)(GDB_dataplugin_register, GDB_dataplugin_warning);

/* name of the function in the data plugin to use for the entry point. */
#define GDB_DATAPLUGIN_ENTRY GDB_dataview_plugin_entry

/* just so this is forced to be extern "C" in the plugin itself... */
void GDB_DATAPLUGIN_ENTRY(GDB_dataplugin_register, GDB_dataplugin_warning);

#ifdef __cplusplus
}
#endif

#endif /* !defined (GDB_DATAPLUGINS_H) */

