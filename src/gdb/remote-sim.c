/* Generic remote debugging interface for simulators.

   Copyright (C) 1993, 1994, 1995, 1996, 1997, 1998, 1999, 2000, 2001, 2002,
   2004, 2005, 2006, 2007, 2008 Free Software Foundation, Inc.

   Contributed by Cygnus Support.
   Steve Chamberlain (sac@cygnus.com).

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
#include "inferior.h"
#include "value.h"
#include "gdb_string.h"
#include <ctype.h>
#include <fcntl.h>
#include <signal.h>
#include <setjmp.h>
#include <errno.h>
#include "terminal.h"
#include "target.h"
#include "gdbcore.h"
#include "gdb/callback.h"
#include "gdb/remote-sim.h"
#include "command.h"
#include "regcache.h"
#include "gdb_assert.h"
#include "sim-regno.h"
#include "arch-utils.h"
#include "readline/readline.h"
#include "gdbthread.h"

/* Prototypes */

extern void _initialize_remote_sim (void);

static void dump_mem (char *buf, int len);

static void init_callbacks (void);

static void end_callbacks (void);

static int gdb_os_write_stdout (host_callback *, const char *, int);

static void gdb_os_flush_stdout (host_callback *);

static int gdb_os_write_stderr (host_callback *, const char *, int);

static void gdb_os_flush_stderr (host_callback *);

static int gdb_os_poll_quit (host_callback *);

/* printf_filtered is depreciated */
static void gdb_os_printf_filtered (host_callback *, const char *, ...);

static void gdb_os_vprintf_filtered (host_callback *, const char *, va_list);

static void gdb_os_evprintf_filtered (host_callback *, const char *, va_list);

static void gdb_os_error (host_callback *, const char *, ...);

static void gdbsim_fetch_register (struct regcache *regcache, int regno);

static void gdbsim_store_register (struct regcache *regcache, int regno);

static void gdbsim_kill (void);

static void gdbsim_load (char *prog, int fromtty);

static void gdbsim_open (char *args, int from_tty);

static void gdbsim_close (int quitting);

static void gdbsim_detach (char *args, int from_tty);

static void gdbsim_resume (ptid_t ptid, int step, enum target_signal siggnal);

static ptid_t gdbsim_wait (ptid_t ptid, struct target_waitstatus *status);

static void gdbsim_prepare_to_store (struct regcache *regcache);

static void gdbsim_files_info (struct target_ops *target);

static void gdbsim_mourn_inferior (void);

static void gdbsim_stop (ptid_t ptid);

void simulator_command (char *args, int from_tty);

/* Naming convention:

   sim_* are the interface to the simulator (see remote-sim.h).
   gdbsim_* are stuff which is internal to gdb.  */

/* Forward data declarations */
extern struct target_ops gdbsim_ops;

static int program_loaded = 0;

/* We must keep track of whether the simulator has been opened or not because
   GDB can call a target's close routine twice, but sim_close doesn't allow
   this.  We also need to record the result of sim_open so we can pass it
   back to the other sim_foo routines.  */
static SIM_DESC gdbsim_desc = 0;

/* This is the ptid we use while we're connected to the simulator.
   Its value is arbitrary, as the simulator target don't have a notion
   or processes or threads, but we need something non-null to place in
   inferior_ptid.  */
static ptid_t remote_sim_ptid;

static void
dump_mem (char *buf, int len)
{
  if (len <= 8)
    {
      if (len == 8 || len == 4)
	{
	  long l[2];
	  memcpy (l, buf, len);
	  printf_filtered ("\t0x%lx", l[0]);
	  if (len == 8)
	    printf_filtered (" 0x%lx", l[1]);
	  printf_filtered ("\n");
	}
      else
	{
	  int i;
	  printf_filtered ("\t");
	  for (i = 0; i < len; i++)
	    printf_filtered ("0x%x ", buf[i]);
	  printf_filtered ("\n");
	}
    }
}

static host_callback gdb_callback;
static int callbacks_initialized = 0;

/* Initialize gdb_callback.  */

static void
init_callbacks (void)
{
  if (!callbacks_initialized)
    {
      gdb_callback = default_callback;
      gdb_callback.init (&gdb_callback);
      gdb_callback.write_stdout = gdb_os_write_stdout;
      gdb_callback.flush_stdout = gdb_os_flush_stdout;
      gdb_callback.write_stderr = gdb_os_write_stderr;
      gdb_callback.flush_stderr = gdb_os_flush_stderr;
      gdb_callback.printf_filtered = gdb_os_printf_filtered;
      gdb_callback.vprintf_filtered = gdb_os_vprintf_filtered;
      gdb_callback.evprintf_filtered = gdb_os_evprintf_filtered;
      gdb_callback.error = gdb_os_error;
      gdb_callback.poll_quit = gdb_os_poll_quit;
      gdb_callback.magic = HOST_CALLBACK_MAGIC;
      callbacks_initialized = 1;
    }
}

/* Release callbacks (free resources used by them).  */

static void
end_callbacks (void)
{
  if (callbacks_initialized)
    {
      gdb_callback.shutdown (&gdb_callback);
      callbacks_initialized = 0;
    }
}

/* GDB version of os_write_stdout callback.  */

static int
gdb_os_write_stdout (host_callback *p, const char *buf, int len)
{
  int i;
  char b[2];

  ui_file_write (gdb_stdtarg, buf, len);
  return len;
}

/* GDB version of os_flush_stdout callback.  */

static void
gdb_os_flush_stdout (host_callback *p)
{
  gdb_flush (gdb_stdtarg);
}

/* GDB version of os_write_stderr callback.  */

static int
gdb_os_write_stderr (host_callback *p, const char *buf, int len)
{
  int i;
  char b[2];

  for (i = 0; i < len; i++)
    {
      b[0] = buf[i];
      b[1] = 0;
      fputs_unfiltered (b, gdb_stdtargerr);
    }
  return len;
}

/* GDB version of os_flush_stderr callback.  */

static void
gdb_os_flush_stderr (host_callback *p)
{
  gdb_flush (gdb_stdtargerr);
}

/* GDB version of printf_filtered callback.  */

static void
gdb_os_printf_filtered (host_callback * p, const char *format,...)
{
  va_list args;
  va_start (args, format);

  vfprintf_filtered (gdb_stdout, format, args);

  va_end (args);
}

/* GDB version of error vprintf_filtered.  */

static void
gdb_os_vprintf_filtered (host_callback * p, const char *format, va_list ap)
{
  vfprintf_filtered (gdb_stdout, format, ap);
}

/* GDB version of error evprintf_filtered.  */

static void
gdb_os_evprintf_filtered (host_callback * p, const char *format, va_list ap)
{
  vfprintf_filtered (gdb_stderr, format, ap);
}

/* GDB version of error callback.  */

static void
gdb_os_error (host_callback * p, const char *format,...)
{
  if (deprecated_error_hook)
    (*deprecated_error_hook) ();
  else
    {
      va_list args;
      va_start (args, format);
      verror (format, args);
      va_end (args);
    }
}

int
one2one_register_sim_regno (struct gdbarch *gdbarch, int regnum)
{
  /* Only makes sense to supply raw registers.  */
  gdb_assert (regnum >= 0 && regnum < gdbarch_num_regs (gdbarch));
  return regnum;
}

static void
gdbsim_fetch_register (struct regcache *regcache, int regno)
{
  struct gdbarch *gdbarch = get_regcache_arch (regcache);
  if (regno == -1)
    {
      for (regno = 0; regno < gdbarch_num_regs (gdbarch); regno++)
	gdbsim_fetch_register (regcache, regno);
      return;
    }

  switch (gdbarch_register_sim_regno (gdbarch, regno))
    {
    case LEGACY_SIM_REGNO_IGNORE:
      break;
    case SIM_REGNO_DOES_NOT_EXIST:
      {
	/* For moment treat a `does not exist' register the same way
           as an ``unavailable'' register.  */
	char buf[MAX_REGISTER_SIZE];
	int nr_bytes;
	memset (buf, 0, MAX_REGISTER_SIZE);
	regcache_raw_supply (regcache, regno, buf);
	break;
      }
      
    default:
      {
	static int warn_user = 1;
	char buf[MAX_REGISTER_SIZE];
	int nr_bytes;
	gdb_assert (regno >= 0 && regno < gdbarch_num_regs (gdbarch));
	memset (buf, 0, MAX_REGISTER_SIZE);
	nr_bytes = sim_fetch_register (gdbsim_desc,
				       gdbarch_register_sim_regno
					 (gdbarch, regno),
				       buf,
				       register_size (gdbarch, regno));
	if (nr_bytes > 0
	    && nr_bytes != register_size (gdbarch, regno) && warn_user)
	  {
	    fprintf_unfiltered (gdb_stderr,
				"Size of register %s (%d/%d) incorrect (%d instead of %d))",
				gdbarch_register_name (gdbarch, regno),
				regno,
				gdbarch_register_sim_regno
				  (gdbarch, regno),
				nr_bytes, register_size (gdbarch, regno));
	    warn_user = 0;
	  }
	/* FIXME: cagney/2002-05-27: Should check `nr_bytes == 0'
	   indicating that GDB and the SIM have different ideas about
	   which registers are fetchable.  */
	/* Else if (nr_bytes < 0): an old simulator, that doesn't
	   think to return the register size.  Just assume all is ok.  */
	regcache_raw_supply (regcache, regno, buf);
	if (remote_debug)
	  {
	    printf_filtered ("gdbsim_fetch_register: %d", regno);
	    /* FIXME: We could print something more intelligible.  */
	    dump_mem (buf, register_size (gdbarch, regno));
	  }
	break;
      }
    }
}


static void
gdbsim_store_register (struct regcache *regcache, int regno)
{
  struct gdbarch *gdbarch = get_regcache_arch (regcache);
  if (regno == -1)
    {
      for (regno = 0; regno < gdbarch_num_regs (gdbarch); regno++)
	gdbsim_store_register (regcache, regno);
      return;
    }
  else if (gdbarch_register_sim_regno (gdbarch, regno) >= 0)
    {
      char tmp[MAX_REGISTER_SIZE];
      int nr_bytes;
      regcache_cooked_read (regcache, regno, tmp);
      nr_bytes = sim_store_register (gdbsim_desc,
				     gdbarch_register_sim_regno
				       (gdbarch, regno),
				     tmp, register_size (gdbarch, regno));
      if (nr_bytes > 0 && nr_bytes != register_size (gdbarch, regno))
	internal_error (__FILE__, __LINE__,
			_("Register size different to expected"));
      /* FIXME: cagney/2002-05-27: Should check `nr_bytes == 0'
	 indicating that GDB and the SIM have different ideas about
	 which registers are fetchable.  */
      if (remote_debug)
	{
	  printf_filtered ("gdbsim_store_register: %d", regno);
	  /* FIXME: We could print something more intelligible.  */
	  dump_mem (tmp, register_size (gdbarch, regno));
	}
    }
}

/* Kill the running program.  This may involve closing any open files
   and releasing other resources acquired by the simulated program.  */

static void
gdbsim_kill (void)
{
  if (remote_debug)
    printf_filtered ("gdbsim_kill\n");

  /* There is no need to `kill' running simulator - the simulator is
     not running.  Mourning it is enough.  */
  target_mourn_inferior ();
}

/* Load an executable file into the target process.  This is expected to
   not only bring new code into the target process, but also to update
   GDB's symbol tables to match.  */

static void
gdbsim_load (char *args, int fromtty)
{
  char **argv = buildargv (args);
  char *prog;

  if (argv == NULL)
    nomem (0);

  make_cleanup_freeargv (argv);

  prog = tilde_expand (argv[0]);

  if (argv[1] != NULL)
    error (_("GDB sim does not yet support a load offset."));

  if (remote_debug)
    printf_filtered ("gdbsim_load: prog \"%s\"\n", prog);

  /* FIXME: We will print two messages on error.
     Need error to either not print anything if passed NULL or need
     another routine that doesn't take any arguments.  */
  if (sim_load (gdbsim_desc, prog, NULL, fromtty) == SIM_RC_FAIL)
    error (_("unable to load program"));

  /* FIXME: If a load command should reset the targets registers then
     a call to sim_create_inferior() should go here. */

  program_loaded = 1;
}


/* Start an inferior process and set inferior_ptid to its pid.
   EXEC_FILE is the file to run.
   ARGS is a string containing the arguments to the program.
   ENV is the environment vector to pass.  Errors reported with error().
   On VxWorks and various standalone systems, we ignore exec_file.  */
/* This is called not only when we first attach, but also when the
   user types "run" after having attached.  */

static void
gdbsim_create_inferior (char *exec_file, char *args, char **env, int from_tty)
{
  int len;
  char *arg_buf, **argv;

  if (exec_file == 0 || exec_bfd == 0)
    warning (_("No executable file specified."));
  if (!program_loaded)
    warning (_("No program loaded."));

  if (remote_debug)
    printf_filtered ("gdbsim_create_inferior: exec_file \"%s\", args \"%s\"\n",
		     (exec_file ? exec_file : "(NULL)"),
		     args);

  if (ptid_equal (inferior_ptid, remote_sim_ptid))
    gdbsim_kill ();
  remove_breakpoints ();
  init_wait_for_inferior ();

  if (exec_file != NULL)
    {
      len = strlen (exec_file) + 1 + strlen (args) + 1 + /*slop */ 10;
      arg_buf = (char *) alloca (len);
      arg_buf[0] = '\0';
      strcat (arg_buf, exec_file);
      strcat (arg_buf, " ");
      strcat (arg_buf, args);
      argv = buildargv (arg_buf);
      make_cleanup_freeargv (argv);
    }
  else
    argv = NULL;
  sim_create_inferior (gdbsim_desc, exec_bfd, argv, env);

  inferior_ptid = remote_sim_ptid;
  add_thread_silent (inferior_ptid);

  target_mark_running (&gdbsim_ops);
  insert_breakpoints ();	/* Needed to get correct instruction in cache */

  clear_proceed_status ();
}

/* The open routine takes the rest of the parameters from the command,
   and (if successful) pushes a new target onto the stack.
   Targets should supply this routine, if only to provide an error message.  */
/* Called when selecting the simulator. EG: (gdb) target sim name.  */

static void
gdbsim_open (char *args, int from_tty)
{
  int len;
  char *arg_buf;
  char **argv;

  if (remote_debug)
    printf_filtered ("gdbsim_open: args \"%s\"\n", args ? args : "(null)");

  /* Remove current simulator if one exists.  Only do this if the simulator
     has been opened because sim_close requires it.
     This is important because the call to push_target below will cause
     sim_close to be called if the simulator is already open, but push_target
     is called after sim_open!  We can't move the call to push_target before
     the call to sim_open because sim_open may invoke `error'.  */
  if (gdbsim_desc != NULL)
    unpush_target (&gdbsim_ops);

  len = (7 + 1			/* gdbsim */
	 + strlen (" -E little")
	 + strlen (" --architecture=xxxxxxxxxx")
	 + (args ? strlen (args) : 0)
	 + 50) /* slack */ ;
  arg_buf = (char *) alloca (len);
  strcpy (arg_buf, "gdbsim");	/* 7 */
  /* Specify the byte order for the target when it is explicitly
     specified by the user (not auto detected). */
  switch (selected_byte_order ())
    {
    case BFD_ENDIAN_BIG:
      strcat (arg_buf, " -E big");
      break;
    case BFD_ENDIAN_LITTLE:
      strcat (arg_buf, " -E little");
      break;
    case BFD_ENDIAN_UNKNOWN:
      break;
    }
  /* Specify the architecture of the target when it has been
     explicitly specified */
  if (selected_architecture_name () != NULL)
    {
      strcat (arg_buf, " --architecture=");
      strcat (arg_buf, selected_architecture_name ());
    }
  /* finally, any explicit args */
  if (args)
    {
      strcat (arg_buf, " ");	/* 1 */
      strcat (arg_buf, args);
    }
  argv = buildargv (arg_buf);
  if (argv == NULL)
    error (_("Insufficient memory available to allocate simulator arg list."));
  make_cleanup_freeargv (argv);

  init_callbacks ();
  gdbsim_desc = sim_open (SIM_OPEN_DEBUG, &gdb_callback, exec_bfd, argv);

  if (gdbsim_desc == 0)
    error (_("unable to create simulator instance"));

  push_target (&gdbsim_ops);
  printf_filtered ("Connected to the simulator.\n");

  /* There's nothing running after "target sim" or "load"; not until
     "run".  */
  inferior_ptid = null_ptid;
  target_mark_exited (&gdbsim_ops);
}

/* Does whatever cleanup is required for a target that we are no longer
   going to be calling.  Argument says whether we are quitting gdb and
   should not get hung in case of errors, or whether we want a clean
   termination even if it takes a while.  This routine is automatically
   always called just before a routine is popped off the target stack.
   Closing file descriptors and freeing memory are typical things it should
   do.  */
/* Close out all files and local state before this target loses control. */

static void
gdbsim_close (int quitting)
{
  if (remote_debug)
    printf_filtered ("gdbsim_close: quitting %d\n", quitting);

  program_loaded = 0;

  if (gdbsim_desc != NULL)
    {
      sim_close (gdbsim_desc, quitting);
      gdbsim_desc = NULL;
    }

  end_callbacks ();
  generic_mourn_inferior ();
  delete_thread_silent (remote_sim_ptid);
}

/* Takes a program previously attached to and detaches it.
   The program may resume execution (some targets do, some don't) and will
   no longer stop on signals, etc.  We better not have left any breakpoints
   in the program or it'll die when it hits one.  ARGS is arguments
   typed by the user (e.g. a signal to send the process).  FROM_TTY
   says whether to be verbose or not.  */
/* Terminate the open connection to the remote debugger.
   Use this when you want to detach and do something else with your gdb.  */

static void
gdbsim_detach (char *args, int from_tty)
{
  if (remote_debug)
    printf_filtered ("gdbsim_detach: args \"%s\"\n", args);

  pop_target ();		/* calls gdbsim_close to do the real work */
  if (from_tty)
    printf_filtered ("Ending simulator %s debugging\n", target_shortname);
}

/* Resume execution of the target process.  STEP says whether to single-step
   or to run free; SIGGNAL is the signal value (e.g. SIGINT) to be given
   to the target, or zero for no signal.  */

static enum target_signal resume_siggnal;
static int resume_step;

static void
gdbsim_resume (ptid_t ptid, int step, enum target_signal siggnal)
{
  if (!ptid_equal (inferior_ptid, remote_sim_ptid))
    error (_("The program is not being run."));

  if (remote_debug)
    printf_filtered ("gdbsim_resume: step %d, signal %d\n", step, siggnal);

  resume_siggnal = siggnal;
  resume_step = step;
}

/* Notify the simulator of an asynchronous request to stop.

   The simulator shall ensure that the stop request is eventually
   delivered to the simulator.  If the call is made while the
   simulator is not running then the stop request is processed when
   the simulator is next resumed.

   For simulators that do not support this operation, just abort */

static void
gdbsim_stop (ptid_t ptid)
{
  if (!sim_stop (gdbsim_desc))
    {
      quit ();
    }
}

/* GDB version of os_poll_quit callback.
   Taken from gdb/util.c - should be in a library.  */

static int
gdb_os_poll_quit (host_callback *p)
{
  if (deprecated_ui_loop_hook != NULL)
    deprecated_ui_loop_hook (0);

  if (quit_flag)		/* gdb's idea of quit */
    {
      quit_flag = 0;		/* we've stolen it */
      return 1;
    }
  else if (immediate_quit)
    {
      return 1;
    }
  return 0;
}

/* Wait for inferior process to do something.  Return pid of child,
   or -1 in case of error; store status through argument pointer STATUS,
   just as `wait' would. */

static void
gdbsim_cntrl_c (int signo)
{
  gdbsim_stop (remote_sim_ptid);
}

static ptid_t
gdbsim_wait (ptid_t ptid, struct target_waitstatus *status)
{
  static RETSIGTYPE (*prev_sigint) ();
  int sigrc = 0;
  enum sim_stop reason = sim_running;

  if (remote_debug)
    printf_filtered ("gdbsim_wait\n");

#if defined (HAVE_SIGACTION) && defined (SA_RESTART)
  {
    struct sigaction sa, osa;
    sa.sa_handler = gdbsim_cntrl_c;
    sigemptyset (&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction (SIGINT, &sa, &osa);
    prev_sigint = osa.sa_handler;
  }
#else
  prev_sigint = signal (SIGINT, gdbsim_cntrl_c);
#endif
  sim_resume (gdbsim_desc, resume_step, resume_siggnal);
  signal (SIGINT, prev_sigint);
  resume_step = 0;

  sim_stop_reason (gdbsim_desc, &reason, &sigrc);

  switch (reason)
    {
    case sim_exited:
      status->kind = TARGET_WAITKIND_EXITED;
      status->value.integer = sigrc;
      break;
    case sim_stopped:
      switch (sigrc)
	{
	case TARGET_SIGNAL_ABRT:
	  quit ();
	  break;
	case TARGET_SIGNAL_INT:
	case TARGET_SIGNAL_TRAP:
	default:
	  status->kind = TARGET_WAITKIND_STOPPED;
	  status->value.sig = sigrc;
	  break;
	}
      break;
    case sim_signalled:
      status->kind = TARGET_WAITKIND_SIGNALLED;
      status->value.sig = sigrc;
      break;
    case sim_running:
    case sim_polling:
      /* FIXME: Is this correct? */
      break;
    }

  return inferior_ptid;
}

/* Get ready to modify the registers array.  On machines which store
   individual registers, this doesn't need to do anything.  On machines
   which store all the registers in one fell swoop, this makes sure
   that registers contains all the registers from the program being
   debugged.  */

static void
gdbsim_prepare_to_store (struct regcache *regcache)
{
  /* Do nothing, since we can store individual regs */
}

/* Transfer LEN bytes between GDB address MYADDR and target address
   MEMADDR.  If WRITE is non-zero, transfer them to the target,
   otherwise transfer them from the target.  TARGET is unused.

   Returns the number of bytes transferred. */

static int
gdbsim_xfer_inferior_memory (CORE_ADDR memaddr, gdb_byte *myaddr, int len,
			     int write, struct mem_attrib *attrib,
			     struct target_ops *target)
{
  /* If no program is running yet, then ignore the simulator for
     memory.  Pass the request down to the next target, hopefully
     an exec file.  */
  if (!target_has_execution)
    return 0;

  if (!program_loaded)
    error (_("No program loaded."));

  if (remote_debug)
    {
      /* FIXME: Send to something other than STDOUT? */
      printf_filtered ("gdbsim_xfer_inferior_memory: myaddr 0x");
      gdb_print_host_address (myaddr, gdb_stdout);
      printf_filtered (", memaddr 0x%s, len %d, write %d\n",
		       paddr_nz (memaddr), len, write);
      if (remote_debug && write)
	dump_mem (myaddr, len);
    }

  if (write)
    {
      len = sim_write (gdbsim_desc, memaddr, myaddr, len);
    }
  else
    {
      len = sim_read (gdbsim_desc, memaddr, myaddr, len);
      if (remote_debug && len > 0)
	dump_mem (myaddr, len);
    }
  return len;
}

static void
gdbsim_files_info (struct target_ops *target)
{
  char *file = "nothing";

  if (exec_bfd)
    file = bfd_get_filename (exec_bfd);

  if (remote_debug)
    printf_filtered ("gdbsim_files_info: file \"%s\"\n", file);

  if (exec_bfd)
    {
      printf_filtered ("\tAttached to %s running program %s\n",
		       target_shortname, file);
      sim_info (gdbsim_desc, 0);
    }
}

/* Clear the simulator's notion of what the break points are.  */

static void
gdbsim_mourn_inferior (void)
{
  if (remote_debug)
    printf_filtered ("gdbsim_mourn_inferior:\n");

  remove_breakpoints ();
  target_mark_exited (&gdbsim_ops);
  generic_mourn_inferior ();
  delete_thread_silent (remote_sim_ptid);
}

/* Pass the command argument through to the simulator verbatim.  The
   simulator must do any command interpretation work.  */

void
simulator_command (char *args, int from_tty)
{
  if (gdbsim_desc == NULL)
    {

      /* PREVIOUSLY: The user may give a command before the simulator
         is opened. [...] (??? assuming of course one wishes to
         continue to allow commands to be sent to unopened simulators,
         which isn't entirely unreasonable). */

      /* The simulator is a builtin abstraction of a remote target.
         Consistent with that model, access to the simulator, via sim
         commands, is restricted to the period when the channel to the
         simulator is open. */

      error (_("Not connected to the simulator target"));
    }

  sim_do_command (gdbsim_desc, args);

  /* Invalidate the register cache, in case the simulator command does
     something funny. */
  registers_changed ();
}

/* Check to see if a thread is still alive.  */

static int
gdbsim_thread_alive (ptid_t ptid)
{
  if (ptid_equal (ptid, remote_sim_ptid))
    /* The simulators' task is always alive.  */
    return 1;

  return 0;
}

/* Convert a thread ID to a string.  Returns the string in a static
   buffer.  */

static char *
gdbsim_pid_to_str (ptid_t ptid)
{
  static char buf[64];

  if (ptid_equal (remote_sim_ptid, ptid))
    {
      xsnprintf (buf, sizeof buf, "Thread <main>");
      return buf;
    }

  return normal_pid_to_str (ptid);
}

/* Define the target subroutine names */

struct target_ops gdbsim_ops;

static void
init_gdbsim_ops (void)
{
  gdbsim_ops.to_shortname = "sim";
  gdbsim_ops.to_longname = "simulator";
  gdbsim_ops.to_doc = "Use the compiled-in simulator.";
  gdbsim_ops.to_open = gdbsim_open;
  gdbsim_ops.to_close = gdbsim_close;
  gdbsim_ops.to_detach = gdbsim_detach;
  gdbsim_ops.to_resume = gdbsim_resume;
  gdbsim_ops.to_wait = gdbsim_wait;
  gdbsim_ops.to_fetch_registers = gdbsim_fetch_register;
  gdbsim_ops.to_store_registers = gdbsim_store_register;
  gdbsim_ops.to_prepare_to_store = gdbsim_prepare_to_store;
  gdbsim_ops.deprecated_xfer_memory = gdbsim_xfer_inferior_memory;
  gdbsim_ops.to_files_info = gdbsim_files_info;
  gdbsim_ops.to_insert_breakpoint = memory_insert_breakpoint;
  gdbsim_ops.to_remove_breakpoint = memory_remove_breakpoint;
  gdbsim_ops.to_kill = gdbsim_kill;
  gdbsim_ops.to_load = gdbsim_load;
  gdbsim_ops.to_create_inferior = gdbsim_create_inferior;
  gdbsim_ops.to_mourn_inferior = gdbsim_mourn_inferior;
  gdbsim_ops.to_stop = gdbsim_stop;
  gdbsim_ops.to_thread_alive = gdbsim_thread_alive;
  gdbsim_ops.to_pid_to_str = gdbsim_pid_to_str;
  gdbsim_ops.to_stratum = process_stratum;
  gdbsim_ops.to_has_all_memory = 1;
  gdbsim_ops.to_has_memory = 1;
  gdbsim_ops.to_has_stack = 1;
  gdbsim_ops.to_has_registers = 1;
  gdbsim_ops.to_has_execution = 1;
  gdbsim_ops.to_magic = OPS_MAGIC;
}

void
_initialize_remote_sim (void)
{
  init_gdbsim_ops ();
  add_target (&gdbsim_ops);

  add_com ("sim", class_obscure, simulator_command,
	   _("Send a command to the simulator."));

  /* Yes, 42000 is arbitrary.  The only sense out of it, is that it
     isn't 0.  */
  remote_sim_ptid = ptid_build (42000, 0, 42000);
}
