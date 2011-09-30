/* GLIB - Library of useful routines for C programming
 * Copyright (C) 1995-1997  Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * gthread.c: posix thread system implementation
 * Copyright 1998 Sebastian Wilhelmi; University of Karlsruhe
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/*
 * Modified by the GLib Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GLib Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GLib at ftp://ftp.gtk.org/pub/gtk/.
 */

#include "config.h"

#include "gthread.h"

#include "gthreadprivate.h"
#include "gslice.h"
#include "gmessages.h"
#include "gstrfuncs.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>

#ifdef HAVE_SYS_TIME_H
# include <sys/time.h>
#endif
#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif
#ifdef HAVE_SCHED_H
#include <sched.h>
#endif

#define posix_check_err(err, name) G_STMT_START{			\
  int error = (err); 							\
  if (error)	 		 		 			\
    g_error ("file %s: line %d (%s): error '%s' during '%s'",		\
           __FILE__, __LINE__, G_STRFUNC,				\
           g_strerror (error), name);					\
  }G_STMT_END

#define posix_check_cmd(cmd) posix_check_err (cmd, #cmd)

void
g_system_thread_create (GThreadFunc       thread_func,
                        gpointer          arg,
                        gulong            stack_size,
                        gboolean          joinable,
                        gpointer          thread,
                        GError          **error)
{
  pthread_attr_t attr;
  gint ret;

  g_return_if_fail (thread_func);

  posix_check_cmd (pthread_attr_init (&attr));

#ifdef HAVE_PTHREAD_ATTR_SETSTACKSIZE
  if (stack_size)
    {
#ifdef _SC_THREAD_STACK_MIN
      stack_size = MAX (sysconf (_SC_THREAD_STACK_MIN), stack_size);
#endif /* _SC_THREAD_STACK_MIN */
      /* No error check here, because some systems can't do it and
       * we simply don't want threads to fail because of that. */
      pthread_attr_setstacksize (&attr, stack_size);
    }
#endif /* HAVE_PTHREAD_ATTR_SETSTACKSIZE */

  posix_check_cmd (pthread_attr_setdetachstate (&attr,
          joinable ? PTHREAD_CREATE_JOINABLE : PTHREAD_CREATE_DETACHED));

  ret = pthread_create (thread, &attr, (void* (*)(void*))thread_func, arg);

  posix_check_cmd (pthread_attr_destroy (&attr));

  if (ret == EAGAIN)
    {
      g_set_error (error, G_THREAD_ERROR, G_THREAD_ERROR_AGAIN, 
		   "Error creating thread: %s", g_strerror (ret));
      return;
    }

  posix_check_err (ret, "pthread_create");
}

/**
 * g_thread_yield:
 *
 * Gives way to other threads waiting to be scheduled.
 *
 * This function is often used as a method to make busy wait less evil.
 * But in most cases you will encounter, there are better methods to do
 * that. So in general you shouldn't use this function.
 */
void
g_thread_yield (void)
{
  sched_yield ();
}

void
g_system_thread_join (gpointer thread)
{
  gpointer ignore;
  posix_check_cmd (pthread_join (*(pthread_t*)thread, &ignore));
}

void
g_system_thread_exit (void)
{
  pthread_exit (NULL);
}

void
g_system_thread_self (gpointer thread)
{
  *(pthread_t*)thread = pthread_self();
}

gboolean
g_system_thread_equal (gpointer thread1,
                       gpointer thread2)
{
  return (pthread_equal (*(pthread_t*)thread1, *(pthread_t*)thread2) != 0);
}
