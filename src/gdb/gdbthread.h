/* Multi-process/thread control defs for GDB, the GNU debugger.
   Copyright (C) 1987, 1988, 1989, 1990, 1991, 1992, 1993, 1997, 1998, 1999,
   2000, 2007, 2008 Free Software Foundation, Inc.
   Contributed by Lynx Real-Time Systems, Inc.  Los Gatos, CA.
   

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

#ifndef GDBTHREAD_H
#define GDBTHREAD_H

struct symtab;

#include "breakpoint.h"
#include "frame.h"
#include "ui-out.h"
#include "inferior.h"

struct thread_info
{
  struct thread_info *next;
  ptid_t ptid;			/* "Actual process id";
				    In fact, this may be overloaded with 
				    kernel thread id, etc.  */
  int num;			/* Convenient handle (GDB thread id) */

  /* Non-zero means the thread is executing.  Note: this is different
     from saying that there is an active target and we are stopped at
     a breakpoint, for instance.  This is a real indicator whether the
     thread is off and running.  */
  /* This field is internal to thread.c.  Never access it directly,
     use is_executing instead.  */
  int executing_;

  /* Frontend view of the thread state.  Note that the RUNNING/STOPPED
     states are different from EXECUTING.  When the thread is stopped
     internally while handling an internal event, like a software
     single-step breakpoint, EXECUTING will be false, but running will
     still be true.  As a possible future extension, this could turn
     into enum { stopped, exited, stepping, finishing, until(ling),
     running ... }  */
  /* This field is internal to thread.c.  Never access it directly,
     use is_running instead.  */
  int state_;

  /* If this is > 0, then it means there's code out there that relies
     on this thread being listed.  Don't delete it from the lists even
     if we detect it exiting.  */
  int refcount;

  /* State from wait_for_inferior */
  CORE_ADDR prev_pc;
  struct breakpoint *step_resume_breakpoint;
  CORE_ADDR step_range_start;
  CORE_ADDR step_range_end;
  struct frame_id step_frame_id;
  int current_line;
  struct symtab *current_symtab;
  int trap_expected;
  int stepping_over_breakpoint;

  /* This is set TRUE when a catchpoint of a shared library event
     triggers.  Since we don't wish to leave the inferior in the
     solib hook when we report the event, we step the inferior
     back to user code before stopping and reporting the event.  */
  int stepping_through_solib_after_catch;

  /* When stepping_through_solib_after_catch is TRUE, this is a
     list of the catchpoints that should be reported as triggering
     when we finally do stop stepping.  */
  bpstat stepping_through_solib_catchpoints;

  /* The below are only per-thread in non-stop mode.  */
  /* Per-thread command support.  */
  struct continuation *continuations;
  struct continuation *intermediate_continuations;
  int proceed_to_finish;
  enum step_over_calls_kind step_over_calls;
  int stop_step;
  int step_multi;

  enum target_signal stop_signal;
  /* Used in continue_command to set the proceed count of the
     breakpoint the thread stopped at.  */
  bpstat stop_bpstat;

  /* Private data used by the target vector implementation.  */
  struct private_thread_info *private;
};

/* Create an empty thread list, or empty the existing one.  */
extern void init_thread_list (void);

/* Add a thread to the thread list, print a message
   that a new thread is found, and return the pointer to
   the new thread.  Caller my use this pointer to 
   initialize the private thread data.  */
extern struct thread_info *add_thread (ptid_t ptid);

/* Same as add_thread, but does not print a message
   about new thread.  */
extern struct thread_info *add_thread_silent (ptid_t ptid);

/* Same as add_thread, and sets the private info.  */
extern struct thread_info *add_thread_with_info (ptid_t ptid,
						 struct private_thread_info *);

/* Delete an existing thread list entry.  */
extern void delete_thread (ptid_t);

/* Delete an existing thread list entry, and be quiet about it.  Used
   after the process this thread having belonged to having already
   exited, for example.  */
extern void delete_thread_silent (ptid_t);

/* Delete a step_resume_breakpoint from the thread database. */
extern void delete_step_resume_breakpoint (void *);

/* Translate the integer thread id (GDB's homegrown id, not the system's)
   into a "pid" (which may be overloaded with extra thread information).  */
extern ptid_t thread_id_to_pid (int);

/* Translate a 'pid' (which may be overloaded with extra thread information) 
   into the integer thread id (GDB's homegrown id, not the system's).  */
extern int pid_to_thread_id (ptid_t ptid);

/* Boolean test for an already-known pid (which may be overloaded with
   extra thread information).  */
extern int in_thread_list (ptid_t ptid);

/* Boolean test for an already-known thread id (GDB's homegrown id, 
   not the system's).  */
extern int valid_thread_id (int thread);

/* Search function to lookup a thread by 'pid'.  */
extern struct thread_info *find_thread_pid (ptid_t ptid);

/* Find thread by GDB user-visible thread number.  */
struct thread_info *find_thread_id (int num);

/* Iterator function to call a user-provided callback function
   once for each known thread.  */
typedef int (*thread_callback_func) (struct thread_info *, void *);
extern struct thread_info *iterate_over_threads (thread_callback_func, void *);

extern int thread_count (void);

/* infrun context switch: save the debugger state for the given thread.  */
extern void save_infrun_state (ptid_t ptid,
			       CORE_ADDR prev_pc,
			       int       trap_expected,
			       struct breakpoint *step_resume_breakpoint,
			       CORE_ADDR step_range_start,
			       CORE_ADDR step_range_end,
			       const struct frame_id *step_frame_id,
			       int       another_trap,
			       int       stepping_through_solib_after_catch,
			       bpstat    stepping_through_solib_catchpoints,
			       int       current_line,
			       struct symtab *current_symtab,
			       struct continuation *continuations,
			       struct continuation *intermediate_continuations,
			       int proceed_to_finish,
			       enum step_over_calls_kind step_over_calls,
			       int stop_step,
			       int step_multi,
			       enum target_signal stop_signal,
			       bpstat stop_bpstat);

/* infrun context switch: load the debugger state previously saved
   for the given thread.  */
extern void load_infrun_state (ptid_t ptid,
			       CORE_ADDR *prev_pc,
			       int       *trap_expected,
			       struct breakpoint **step_resume_breakpoint,
			       CORE_ADDR *step_range_start,
			       CORE_ADDR *step_range_end,
			       struct frame_id *step_frame_id,
			       int       *another_trap,
			       int       *stepping_through_solib_after_catch,
			       bpstat    *stepping_through_solib_catchpoints,
			       int       *current_line,
			       struct symtab **current_symtab,
			       struct continuation **continuations,
			       struct continuation **intermediate_continuations,
			       int *proceed_to_finish,
			       enum step_over_calls_kind *step_over_calls,
			       int *stop_step,
			       int *step_multi,
			       enum target_signal *stop_signal,
			       bpstat *stop_bpstat);

/* Switch from one thread to another.  */
extern void switch_to_thread (ptid_t ptid);

/* Marks thread PTID is running, or stopped. 
   If PIDGET (PTID) is -1, marks all threads.  */
extern void set_running (ptid_t ptid, int running);

/* Reports if thread PTID is known to be running right now.  */
extern int is_running (ptid_t ptid);

/* Reports if any thread is known to be running right now.  */
extern int any_running (void);

/* Is this thread listed, but known to have exited?  We keep it listed
   (but not visible) until it's safe to delete.  */
extern int is_exited (ptid_t ptid);

/* Is this thread stopped?  */
extern int is_stopped (ptid_t ptid);

/* Marks thread PTID as executing, or as stopped.
   If PIDGET (PTID) is -1, marks all threads.  */
extern void set_executing (ptid_t ptid, int executing);

/* Reports if thread PTID is executing.  */
extern int is_executing (ptid_t ptid);

/* Commands with a prefix of `thread'.  */
extern struct cmd_list_element *thread_cmd_list;

/* Print notices on thread events (attach, detach, etc.), set with
   `set print thread-events'.  */
extern int print_thread_events;

extern void print_thread_info (struct ui_out *uiout, int thread);

extern struct cleanup *make_cleanup_restore_current_thread (void);


#endif /* GDBTHREAD_H */
