/* Helper routines for C++ support in GDB.
   Copyright (C) 2002, 2003, 2004, 2005, 2007, 2008
   Free Software Foundation, Inc.

   Contributed by MontaVista Software.
   Namespace support contributed by David Carlton.

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
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */

#ifndef CP_SUPPORT_H
#define CP_SUPPORT_H

/* We need this for 'domain_enum', alas...  */

#include "symtab.h"

/* Opaque declarations.  */

struct symbol;
struct obstack;
struct block;
struct objfile;
struct type;
struct demangle_component;

/* This struct is designed to store data from using directives.  It
   says that names from namespace INNER should be visible within
   namespace OUTER.  OUTER should always be a strict initial substring
   of INNER.  These form a linked list; NEXT is the next element of
   the list.  */

struct using_direct
{
  char *inner;
  char *outer;
  struct using_direct *next;
};


/* Functions from cp-support.c.  */

extern char *cp_canonicalize_string (const char *string);

extern char *cp_class_name_from_physname (const char *physname);

extern char *method_name_from_physname (const char *physname);

extern unsigned int cp_find_first_component (const char *name);

extern unsigned int cp_entire_prefix_len (const char *name);

extern char *cp_func_name (const char *full_name);

extern struct symbol **make_symbol_overload_list (const char *,
						  const char *);

extern struct type *cp_lookup_rtti_type (const char *name,
					 struct block *block);

/* Functions/variables from cp-namespace.c.  */

extern unsigned char processing_has_namespace_info;

extern const char *processing_current_prefix;

extern int cp_is_anonymous (const char *namespace);

extern void cp_add_using_directive (const char *name,
				    unsigned int outer_length,
				    unsigned int inner_length);

extern void cp_initialize_namespace (void);

extern void cp_finalize_namespace (struct block *static_block,
				   struct obstack *obstack);

extern void cp_set_block_scope (const struct symbol *symbol,
				struct block *block,
				struct obstack *obstack);

extern void cp_scan_for_anonymous_namespaces (const struct symbol *symbol);

extern struct symbol *cp_lookup_symbol_nonlocal (const char *name,
						 const char *linkage_name,
						 const struct block *block,
						 const domain_enum domain);

extern struct symbol *cp_lookup_symbol_namespace (const char *namespace,
						  const char *name,
						  const char *linkage_name,
						  const struct block *block,
						  const domain_enum domain);

extern struct type *cp_lookup_nested_type (struct type *parent_type,
					   const char *nested_name,
					   const struct block *block);

extern void cp_check_possible_namespace_symbols (const char *name,
						 struct objfile *objfile);

struct type *cp_lookup_transparent_type (const char *name);

/* Functions from cp-name-parser.y.  */

extern struct demangle_component *cp_demangled_name_to_comp
  (const char *demangled_name, const char **errmsg);

extern char *cp_comp_to_string (struct demangle_component *result,
				int estimated_len);

/* The list of "maint cplus" commands.  */

extern struct cmd_list_element *maint_cplus_cmd_list;

#endif /* CP_SUPPORT_H */
