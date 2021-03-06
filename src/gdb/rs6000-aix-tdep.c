/* Native support code for PPC AIX, for GDB the GNU debugger.

   Copyright (C) 2006, 2007, 2008 Free Software Foundation, Inc.

   Free Software Foundation, Inc.

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

#include "defs.h"
#include "gdb_string.h"
#include "gdb_assert.h"
#include "osabi.h"
#include "regcache.h"
#include "regset.h"
#include "gdbtypes.h"
#include "gdbcore.h"
#include "target.h"
#include "value.h"
#include "infcall.h"
#include "objfiles.h"
#include "breakpoint.h"
#include "rs6000-tdep.h"
#include "ppc-tdep.h"

/* Hook for determining the TOC address when calling functions in the
   inferior under AIX. The initialization code in rs6000-nat.c sets
   this hook to point to find_toc_address.  */

CORE_ADDR (*rs6000_find_toc_address_hook) (CORE_ADDR) = NULL;

/* If the kernel has to deliver a signal, it pushes a sigcontext
   structure on the stack and then calls the signal handler, passing
   the address of the sigcontext in an argument register. Usually
   the signal handler doesn't save this register, so we have to
   access the sigcontext structure via an offset from the signal handler
   frame.
   The following constants were determined by experimentation on AIX 3.2.  */
#define SIG_FRAME_PC_OFFSET 96
#define SIG_FRAME_LR_OFFSET 108
#define SIG_FRAME_FP_OFFSET 284


/* Core file support.  */

static struct ppc_reg_offsets rs6000_aix32_reg_offsets =
{
  /* General-purpose registers.  */
  208, /* r0_offset */
  4,  /* gpr_size */
  4,  /* xr_size */
  24, /* pc_offset */
  28, /* ps_offset */
  32, /* cr_offset */
  36, /* lr_offset */
  40, /* ctr_offset */
  44, /* xer_offset */
  48, /* mq_offset */

  /* Floating-point registers.  */
  336, /* f0_offset */
  56, /* fpscr_offset */
  4,  /* fpscr_size */

  /* AltiVec registers.  */
  -1, /* vr0_offset */
  -1, /* vscr_offset */
  -1 /* vrsave_offset */
};

static struct ppc_reg_offsets rs6000_aix64_reg_offsets =
{
  /* General-purpose registers.  */
  0, /* r0_offset */
  8,  /* gpr_size */
  4,  /* xr_size */
  264, /* pc_offset */
  256, /* ps_offset */
  288, /* cr_offset */
  272, /* lr_offset */
  280, /* ctr_offset */
  292, /* xer_offset */
  -1, /* mq_offset */

  /* Floating-point registers.  */
  312, /* f0_offset */
  296, /* fpscr_offset */
  4,  /* fpscr_size */

  /* AltiVec registers.  */
  -1, /* vr0_offset */
  -1, /* vscr_offset */
  -1 /* vrsave_offset */
};


/* Supply register REGNUM in the general-purpose register set REGSET
   from the buffer specified by GREGS and LEN to register cache
   REGCACHE.  If REGNUM is -1, do this for all registers in REGSET.  */

static void
rs6000_aix_supply_regset (const struct regset *regset,
			  struct regcache *regcache, int regnum,
			  const void *gregs, size_t len)
{
  ppc_supply_gregset (regset, regcache, regnum, gregs, len);
  ppc_supply_fpregset (regset, regcache, regnum, gregs, len);
}

/* Collect register REGNUM in the general-purpose register set
   REGSET. from register cache REGCACHE into the buffer specified by
   GREGS and LEN.  If REGNUM is -1, do this for all registers in
   REGSET.  */

static void
rs6000_aix_collect_regset (const struct regset *regset,
			   const struct regcache *regcache, int regnum,
			   void *gregs, size_t len)
{
  ppc_collect_gregset (regset, regcache, regnum, gregs, len);
  ppc_collect_fpregset (regset, regcache, regnum, gregs, len);
}

/* AIX register set.  */

static struct regset rs6000_aix32_regset =
{
  &rs6000_aix32_reg_offsets,
  rs6000_aix_supply_regset,
  rs6000_aix_collect_regset,
};

static struct regset rs6000_aix64_regset =
{
  &rs6000_aix64_reg_offsets,
  rs6000_aix_supply_regset,
  rs6000_aix_collect_regset,
};

/* Return the appropriate register set for the core section identified
   by SECT_NAME and SECT_SIZE.  */

static const struct regset *
rs6000_aix_regset_from_core_section (struct gdbarch *gdbarch,
				     const char *sect_name, size_t sect_size)
{
  if (gdbarch_tdep (gdbarch)->wordsize == 4)
    {
      if (strcmp (sect_name, ".reg") == 0 && sect_size >= 592)
        return &rs6000_aix32_regset;
    }
  else
    {
      if (strcmp (sect_name, ".reg") == 0 && sect_size >= 576)
        return &rs6000_aix64_regset;
    }

  return NULL;
}


/* Pass the arguments in either registers, or in the stack. In RS/6000,
   the first eight words of the argument list (that might be less than
   eight parameters if some parameters occupy more than one word) are
   passed in r3..r10 registers.  float and double parameters are
   passed in fpr's, in addition to that.  Rest of the parameters if any
   are passed in user stack.  There might be cases in which half of the
   parameter is copied into registers, the other half is pushed into
   stack.

   Stack must be aligned on 64-bit boundaries when synthesizing
   function calls.

   If the function is returning a structure, then the return address is passed
   in r3, then the first 7 words of the parameters can be passed in registers,
   starting from r4.  */

static CORE_ADDR
rs6000_push_dummy_call (struct gdbarch *gdbarch, struct value *function,
			struct regcache *regcache, CORE_ADDR bp_addr,
			int nargs, struct value **args, CORE_ADDR sp,
			int struct_return, CORE_ADDR struct_addr)
{
  struct gdbarch_tdep *tdep = gdbarch_tdep (gdbarch);
  int ii;
  int len = 0;
  int argno;			/* current argument number */
  int argbytes;			/* current argument byte */
  gdb_byte tmp_buffer[50];
  int f_argno = 0;		/* current floating point argno */
  int wordsize = gdbarch_tdep (gdbarch)->wordsize;
  CORE_ADDR func_addr = find_function_addr (function, NULL);

  struct value *arg = 0;
  struct type *type;

  ULONGEST saved_sp;

  /* The calling convention this function implements assumes the
     processor has floating-point registers.  We shouldn't be using it
     on PPC variants that lack them.  */
  gdb_assert (ppc_floating_point_unit_p (gdbarch));

  /* The first eight words of ther arguments are passed in registers.
     Copy them appropriately.  */
  ii = 0;

  /* If the function is returning a `struct', then the first word
     (which will be passed in r3) is used for struct return address.
     In that case we should advance one word and start from r4
     register to copy parameters.  */
  if (struct_return)
    {
      regcache_raw_write_unsigned (regcache, tdep->ppc_gp0_regnum + 3,
				   struct_addr);
      ii++;
    }

/* 
   effectively indirect call... gcc does...

   return_val example( float, int);

   eabi: 
   float in fp0, int in r3
   offset of stack on overflow 8/16
   for varargs, must go by type.
   power open:
   float in r3&r4, int in r5
   offset of stack on overflow different 
   both: 
   return in r3 or f0.  If no float, must study how gcc emulates floats;
   pay attention to arg promotion.  
   User may have to cast\args to handle promotion correctly 
   since gdb won't know if prototype supplied or not.
 */

  for (argno = 0, argbytes = 0; argno < nargs && ii < 8; ++ii)
    {
      int reg_size = register_size (gdbarch, ii + 3);

      arg = args[argno];
      type = check_typedef (value_type (arg));
      len = TYPE_LENGTH (type);

      if (TYPE_CODE (type) == TYPE_CODE_FLT)
	{

	  /* Floating point arguments are passed in fpr's, as well as gpr's.
	     There are 13 fpr's reserved for passing parameters. At this point
	     there is no way we would run out of them.  */

	  gdb_assert (len <= 8);

	  regcache_cooked_write (regcache,
	                         tdep->ppc_fp0_regnum + 1 + f_argno,
	                         value_contents (arg));
	  ++f_argno;
	}

      if (len > reg_size)
	{

	  /* Argument takes more than one register.  */
	  while (argbytes < len)
	    {
	      gdb_byte word[MAX_REGISTER_SIZE];
	      memset (word, 0, reg_size);
	      memcpy (word,
		      ((char *) value_contents (arg)) + argbytes,
		      (len - argbytes) > reg_size
		        ? reg_size : len - argbytes);
	      regcache_cooked_write (regcache,
	                            tdep->ppc_gp0_regnum + 3 + ii,
				    word);
	      ++ii, argbytes += reg_size;

	      if (ii >= 8)
		goto ran_out_of_registers_for_arguments;
	    }
	  argbytes = 0;
	  --ii;
	}
      else
	{
	  /* Argument can fit in one register.  No problem.  */
	  int adj = gdbarch_byte_order (gdbarch)
		    == BFD_ENDIAN_BIG ? reg_size - len : 0;
	  gdb_byte word[MAX_REGISTER_SIZE];

	  memset (word, 0, reg_size);
	  memcpy (word, value_contents (arg), len);
	  regcache_cooked_write (regcache, tdep->ppc_gp0_regnum + 3 +ii, word);
	}
      ++argno;
    }

ran_out_of_registers_for_arguments:

  regcache_cooked_read_unsigned (regcache,
				 gdbarch_sp_regnum (gdbarch),
				 &saved_sp);

  /* Location for 8 parameters are always reserved.  */
  sp -= wordsize * 8;

  /* Another six words for back chain, TOC register, link register, etc.  */
  sp -= wordsize * 6;

  /* Stack pointer must be quadword aligned.  */
  sp &= -16;

  /* If there are more arguments, allocate space for them in 
     the stack, then push them starting from the ninth one.  */

  if ((argno < nargs) || argbytes)
    {
      int space = 0, jj;

      if (argbytes)
	{
	  space += ((len - argbytes + 3) & -4);
	  jj = argno + 1;
	}
      else
	jj = argno;

      for (; jj < nargs; ++jj)
	{
	  struct value *val = args[jj];
	  space += ((TYPE_LENGTH (value_type (val))) + 3) & -4;
	}

      /* Add location required for the rest of the parameters.  */
      space = (space + 15) & -16;
      sp -= space;

      /* This is another instance we need to be concerned about
         securing our stack space. If we write anything underneath %sp
         (r1), we might conflict with the kernel who thinks he is free
         to use this area.  So, update %sp first before doing anything
         else.  */

      regcache_raw_write_signed (regcache,
				 gdbarch_sp_regnum (gdbarch), sp);

      /* If the last argument copied into the registers didn't fit there 
         completely, push the rest of it into stack.  */

      if (argbytes)
	{
	  write_memory (sp + 24 + (ii * 4),
			value_contents (arg) + argbytes,
			len - argbytes);
	  ++argno;
	  ii += ((len - argbytes + 3) & -4) / 4;
	}

      /* Push the rest of the arguments into stack.  */
      for (; argno < nargs; ++argno)
	{

	  arg = args[argno];
	  type = check_typedef (value_type (arg));
	  len = TYPE_LENGTH (type);


	  /* Float types should be passed in fpr's, as well as in the
             stack.  */
	  if (TYPE_CODE (type) == TYPE_CODE_FLT && f_argno < 13)
	    {

	      gdb_assert (len <= 8);

	      regcache_cooked_write (regcache,
				     tdep->ppc_fp0_regnum + 1 + f_argno,
				     value_contents (arg));
	      ++f_argno;
	    }

	  write_memory (sp + 24 + (ii * 4), value_contents (arg), len);
	  ii += ((len + 3) & -4) / 4;
	}
    }

  /* Set the stack pointer.  According to the ABI, the SP is meant to
     be set _before_ the corresponding stack space is used.  On AIX,
     this even applies when the target has been completely stopped!
     Not doing this can lead to conflicts with the kernel which thinks
     that it still has control over this not-yet-allocated stack
     region.  */
  regcache_raw_write_signed (regcache, gdbarch_sp_regnum (gdbarch), sp);

  /* Set back chain properly.  */
  store_unsigned_integer (tmp_buffer, wordsize, saved_sp);
  write_memory (sp, tmp_buffer, wordsize);

  /* Point the inferior function call's return address at the dummy's
     breakpoint.  */
  regcache_raw_write_signed (regcache, tdep->ppc_lr_regnum, bp_addr);

  /* Set the TOC register, get the value from the objfile reader
     which, in turn, gets it from the VMAP table.  */
  if (rs6000_find_toc_address_hook != NULL)
    {
      CORE_ADDR tocvalue = (*rs6000_find_toc_address_hook) (func_addr);
      regcache_raw_write_signed (regcache, tdep->ppc_toc_regnum, tocvalue);
    }

  target_store_registers (regcache, -1);
  return sp;
}

static enum return_value_convention
rs6000_return_value (struct gdbarch *gdbarch, struct type *func_type,
		     struct type *valtype, struct regcache *regcache,
		     gdb_byte *readbuf, const gdb_byte *writebuf)
{
  struct gdbarch_tdep *tdep = gdbarch_tdep (gdbarch);
  gdb_byte buf[8];

  /* The calling convention this function implements assumes the
     processor has floating-point registers.  We shouldn't be using it
     on PowerPC variants that lack them.  */
  gdb_assert (ppc_floating_point_unit_p (gdbarch));

  /* AltiVec extension: Functions that declare a vector data type as a
     return value place that return value in VR2.  */
  if (TYPE_CODE (valtype) == TYPE_CODE_ARRAY && TYPE_VECTOR (valtype)
      && TYPE_LENGTH (valtype) == 16)
    {
      if (readbuf)
	regcache_cooked_read (regcache, tdep->ppc_vr0_regnum + 2, readbuf);
      if (writebuf)
	regcache_cooked_write (regcache, tdep->ppc_vr0_regnum + 2, writebuf);

      return RETURN_VALUE_REGISTER_CONVENTION;
    }

  /* If the called subprogram returns an aggregate, there exists an
     implicit first argument, whose value is the address of a caller-
     allocated buffer into which the callee is assumed to store its
     return value. All explicit parameters are appropriately
     relabeled.  */
  if (TYPE_CODE (valtype) == TYPE_CODE_STRUCT
      || TYPE_CODE (valtype) == TYPE_CODE_UNION
      || TYPE_CODE (valtype) == TYPE_CODE_ARRAY)
    return RETURN_VALUE_STRUCT_CONVENTION;

  /* Scalar floating-point values are returned in FPR1 for float or
     double, and in FPR1:FPR2 for quadword precision.  Fortran
     complex*8 and complex*16 are returned in FPR1:FPR2, and
     complex*32 is returned in FPR1:FPR4.  */
  if (TYPE_CODE (valtype) == TYPE_CODE_FLT
      && (TYPE_LENGTH (valtype) == 4 || TYPE_LENGTH (valtype) == 8))
    {
      struct type *regtype = register_type (gdbarch, tdep->ppc_fp0_regnum);
      gdb_byte regval[8];

      /* FIXME: kettenis/2007-01-01: Add support for quadword
	 precision and complex.  */

      if (readbuf)
	{
	  regcache_cooked_read (regcache, tdep->ppc_fp0_regnum + 1, regval);
	  convert_typed_floating (regval, regtype, readbuf, valtype);
	}
      if (writebuf)
	{
	  convert_typed_floating (writebuf, valtype, regval, regtype);
	  regcache_cooked_write (regcache, tdep->ppc_fp0_regnum + 1, regval);
	}

      return RETURN_VALUE_REGISTER_CONVENTION;
  }

  /* Values of the types int, long, short, pointer, and char (length
     is less than or equal to four bytes), as well as bit values of
     lengths less than or equal to 32 bits, must be returned right
     justified in GPR3 with signed values sign extended and unsigned
     values zero extended, as necessary.  */
  if (TYPE_LENGTH (valtype) <= tdep->wordsize)
    {
      if (readbuf)
	{
	  ULONGEST regval;

	  /* For reading we don't have to worry about sign extension.  */
	  regcache_cooked_read_unsigned (regcache, tdep->ppc_gp0_regnum + 3,
					 &regval);
	  store_unsigned_integer (readbuf, TYPE_LENGTH (valtype), regval);
	}
      if (writebuf)
	{
	  /* For writing, use unpack_long since that should handle any
	     required sign extension.  */
	  regcache_cooked_write_unsigned (regcache, tdep->ppc_gp0_regnum + 3,
					  unpack_long (valtype, writebuf));
	}

      return RETURN_VALUE_REGISTER_CONVENTION;
    }

  /* Eight-byte non-floating-point scalar values must be returned in
     GPR3:GPR4.  */

  if (TYPE_LENGTH (valtype) == 8)
    {
      gdb_assert (TYPE_CODE (valtype) != TYPE_CODE_FLT);
      gdb_assert (tdep->wordsize == 4);

      if (readbuf)
	{
	  gdb_byte regval[8];

	  regcache_cooked_read (regcache, tdep->ppc_gp0_regnum + 3, regval);
	  regcache_cooked_read (regcache, tdep->ppc_gp0_regnum + 4,
				regval + 4);
	  memcpy (readbuf, regval, 8);
	}
      if (writebuf)
	{
	  regcache_cooked_write (regcache, tdep->ppc_gp0_regnum + 3, writebuf);
	  regcache_cooked_write (regcache, tdep->ppc_gp0_regnum + 4,
				 writebuf + 4);
	}

      return RETURN_VALUE_REGISTER_CONVENTION;
    }

  return RETURN_VALUE_STRUCT_CONVENTION;
}

/* Support for CONVERT_FROM_FUNC_PTR_ADDR (ARCH, ADDR, TARG).

   Usually a function pointer's representation is simply the address
   of the function. On the RS/6000 however, a function pointer is
   represented by a pointer to an OPD entry. This OPD entry contains
   three words, the first word is the address of the function, the
   second word is the TOC pointer (r2), and the third word is the
   static chain value.  Throughout GDB it is currently assumed that a
   function pointer contains the address of the function, which is not
   easy to fix.  In addition, the conversion of a function address to
   a function pointer would require allocation of an OPD entry in the
   inferior's memory space, with all its drawbacks.  To be able to
   call C++ virtual methods in the inferior (which are called via
   function pointers), find_function_addr uses this function to get the
   function address from a function pointer.  */

/* Return real function address if ADDR (a function pointer) is in the data
   space and is therefore a special function pointer.  */

static CORE_ADDR
rs6000_convert_from_func_ptr_addr (struct gdbarch *gdbarch,
				   CORE_ADDR addr,
				   struct target_ops *targ)
{
  struct obj_section *s;

  s = find_pc_section (addr);

  /* Normally, functions live inside a section that is executable.
     So, if ADDR points to a non-executable section, then treat it
     as a function descriptor and return the target address iff
     the target address itself points to a section that is executable.  */
  if (s && (s->the_bfd_section->flags & SEC_CODE) == 0)
    {
      CORE_ADDR pc =
        read_memory_unsigned_integer (addr, gdbarch_tdep (gdbarch)->wordsize);
      struct obj_section *pc_section = find_pc_section (pc);

      if (pc_section && (pc_section->the_bfd_section->flags & SEC_CODE))
        return pc;
    }

  return addr;
}


/* Calculate the destination of a branch/jump.  Return -1 if not a branch.  */

static CORE_ADDR
branch_dest (struct frame_info *frame, int opcode, int instr,
	     CORE_ADDR pc, CORE_ADDR safety)
{
  struct gdbarch_tdep *tdep = gdbarch_tdep (get_frame_arch (frame));
  CORE_ADDR dest;
  int immediate;
  int absolute;
  int ext_op;

  absolute = (int) ((instr >> 1) & 1);

  switch (opcode)
    {
    case 18:
      immediate = ((instr & ~3) << 6) >> 6;	/* br unconditional */
      if (absolute)
	dest = immediate;
      else
	dest = pc + immediate;
      break;

    case 16:
      immediate = ((instr & ~3) << 16) >> 16;	/* br conditional */
      if (absolute)
	dest = immediate;
      else
	dest = pc + immediate;
      break;

    case 19:
      ext_op = (instr >> 1) & 0x3ff;

      if (ext_op == 16)		/* br conditional register */
	{
          dest = get_frame_register_unsigned (frame, tdep->ppc_lr_regnum) & ~3;

	  /* If we are about to return from a signal handler, dest is
	     something like 0x3c90.  The current frame is a signal handler
	     caller frame, upon completion of the sigreturn system call
	     execution will return to the saved PC in the frame.  */
	  if (dest < AIX_TEXT_SEGMENT_BASE)
	    dest = read_memory_unsigned_integer
		     (get_frame_base (frame) + SIG_FRAME_PC_OFFSET,
		      tdep->wordsize);
	}

      else if (ext_op == 528)	/* br cond to count reg */
	{
          dest = get_frame_register_unsigned (frame, tdep->ppc_ctr_regnum) & ~3;

	  /* If we are about to execute a system call, dest is something
	     like 0x22fc or 0x3b00.  Upon completion the system call
	     will return to the address in the link register.  */
	  if (dest < AIX_TEXT_SEGMENT_BASE)
            dest = get_frame_register_unsigned (frame, tdep->ppc_lr_regnum) & ~3;
	}
      else
	return -1;
      break;

    default:
      return -1;
    }
  return (dest < AIX_TEXT_SEGMENT_BASE) ? safety : dest;
}

/* AIX does not support PT_STEP.  Simulate it.  */

static int
rs6000_software_single_step (struct frame_info *frame)
{
  int ii, insn;
  CORE_ADDR loc;
  CORE_ADDR breaks[2];
  int opcode;

  loc = get_frame_pc (frame);

  insn = read_memory_integer (loc, 4);

  if (ppc_deal_with_atomic_sequence (frame))
    return 1;
  
  breaks[0] = loc + PPC_INSN_SIZE;
  opcode = insn >> 26;
  breaks[1] = branch_dest (frame, opcode, insn, loc, breaks[0]);

  /* Don't put two breakpoints on the same address. */
  if (breaks[1] == breaks[0])
    breaks[1] = -1;

  for (ii = 0; ii < 2; ++ii)
    {
      /* ignore invalid breakpoint. */
      if (breaks[ii] == -1)
	continue;
      insert_single_step_breakpoint (breaks[ii]);
    }

  errno = 0;			/* FIXME, don't ignore errors! */
  /* What errors?  {read,write}_memory call error().  */
  return 1;
}

static enum gdb_osabi
rs6000_aix_osabi_sniffer (bfd *abfd)
{
  
  if (bfd_get_flavour (abfd) == bfd_target_xcoff_flavour);
    return GDB_OSABI_AIX;

  return GDB_OSABI_UNKNOWN;
}

static void
rs6000_aix_init_osabi (struct gdbarch_info info, struct gdbarch *gdbarch)
{
  struct gdbarch_tdep *tdep = gdbarch_tdep (gdbarch);

  /* RS6000/AIX does not support PT_STEP.  Has to be simulated.  */
  set_gdbarch_software_single_step (gdbarch, rs6000_software_single_step);

  set_gdbarch_push_dummy_call (gdbarch, rs6000_push_dummy_call);
  set_gdbarch_return_value (gdbarch, rs6000_return_value);
  set_gdbarch_long_double_bit (gdbarch, 8 * TARGET_CHAR_BIT);

  /* Handle RS/6000 function pointers (which are really function
     descriptors).  */
  set_gdbarch_convert_from_func_ptr_addr
    (gdbarch, rs6000_convert_from_func_ptr_addr);

  /* Core file support.  */
  set_gdbarch_regset_from_core_section
    (gdbarch, rs6000_aix_regset_from_core_section);

  if (tdep->wordsize == 8)
    tdep->lr_frame_offset = 16;
  else
    tdep->lr_frame_offset = 8;

  if (tdep->wordsize == 4)
    /* PowerOpen / AIX 32 bit.  The saved area or red zone consists of
       19 4 byte GPRS + 18 8 byte FPRs giving a total of 220 bytes.
       Problem is, 220 isn't frame (16 byte) aligned.  Round it up to
       224.  */
    set_gdbarch_frame_red_zone_size (gdbarch, 224);
  else
    set_gdbarch_frame_red_zone_size (gdbarch, 0);
}

void
_initialize_rs6000_aix_tdep (void)
{
  gdbarch_register_osabi_sniffer (bfd_arch_rs6000,
                                  bfd_target_xcoff_flavour,
                                  rs6000_aix_osabi_sniffer);
  gdbarch_register_osabi_sniffer (bfd_arch_powerpc,
                                  bfd_target_xcoff_flavour,
                                  rs6000_aix_osabi_sniffer);

  gdbarch_register_osabi (bfd_arch_rs6000, 0, GDB_OSABI_AIX,
                          rs6000_aix_init_osabi);
  gdbarch_register_osabi (bfd_arch_powerpc, 0, GDB_OSABI_AIX,
                          rs6000_aix_init_osabi);
}

