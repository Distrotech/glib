/*
 * Copyright © 2011 Codethink Limited
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the licence, or (at your option) any later version.
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
 *
 * Author: Ryan Lortie <desrt@desrt.ca>
 */

#include "config.h"

#include "gmutex.h"

#include "gatomic.h"

#include <linux/futex.h>
#include <sys/syscall.h>
#include <pthread.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>

/* Next to gatomic, this file is one of the lowest-level parts of GLib.
 * All other parts of GLib (messages, memory, slices, etc) assume that
 * they can freely use these facilities without risking recursion.
 *
 * As such, these functions are NOT permitted to call any other part of
 * GLib.
 */

static void
g_mutex_abort (gint         status,
               const gchar *function)
{
  fprintf (stderr, "GLib (gmutex-futex.c): Unexpected error from C library during '%s': %s.  Aborting.\n",
           strerror (status), function);
  abort ();
}

/* {{{1 GMutex */

void
g_mutex_init (GMutex *mutex)
{
  mutex->i[0] = 0;
}

void
g_mutex_clear (GMutex *mutex)
{
}

static void __attribute__((noinline))
g_mutex_lock_slowpath (GMutex *mutex)
{
  gint value;

  value = __sync_lock_test_and_set (mutex->i, 2);

  while (value != 0)
    {
      syscall (__NR_futex, mutex->i, (gsize) FUTEX_WAIT, (gsize) 2, NULL);
      value = __sync_lock_test_and_set (mutex->i, 2);
    }
}

void
g_mutex_lock (GMutex *mutex)
{
  if G_UNLIKELY (!g_atomic_int_compare_and_exchange (mutex->i, 0, 1))
    g_mutex_lock_slowpath (mutex);
}

static void __attribute__((noinline))
g_mutex_unlock_slowpath (GMutex *mutex)
{
  g_atomic_int_set (mutex->i, 0);
  syscall (__NR_futex, mutex->i, (gsize) FUTEX_WAKE, (gsize) 1, NULL);
}

void
g_mutex_unlock (GMutex *mutex)
{
  if G_UNLIKELY (!g_atomic_int_dec_and_test (mutex->i))
    g_mutex_unlock_slowpath (mutex);
}

gboolean
g_mutex_trylock (GMutex *mutex)
{
  return g_atomic_int_compare_and_exchange (mutex->i, 0, 1);
}

/* {{{1 GRecMutex */

void
g_rec_mutex_init (GRecMutex *rec_mutex)
{
  rec_mutex->p = NULL;
  rec_mutex->i[0] = 0;
  rec_mutex->i[1] = 0;
}

void
g_rec_mutex_clear (GRecMutex *rec_mutex)
{
}

static void __attribute__((noinline))
g_rec_mutex_lock_slowpath (GRecMutex *rec_mutex)
{
  gulong tid = pthread_self ();
  gulong value;

start_over:
  value = (gulong) g_atomic_pointer_get (&rec_mutex->p);

  if (value == 0)
    goto try_to_acquire;

mark_contended:
  if (value & 1)
    goto wait;

  if (!__sync_bool_compare_and_swap (&rec_mutex->p, value, value | 1))
    goto start_over;
  value |= 1;

wait:
  syscall (__NR_futex, &rec_mutex->p, (gsize) FUTEX_WAIT, (gsize) (guint) value, NULL);

try_to_acquire:
  value = (gulong) __sync_val_compare_and_swap (&rec_mutex->p, 0, tid | 1);
  if (value != 0)
    goto mark_contended;

  rec_mutex->i[0] = 1;
}

void
g_rec_mutex_lock (GRecMutex *rec_mutex)
{
  gulong tid = pthread_self ();
  gulong prev;

  prev = (gulong) __sync_val_compare_and_swap (&rec_mutex->p, 0, tid);

  if G_LIKELY (prev == 0 || (prev & ~1ul) == tid)
    {
      rec_mutex->i[0]++;
      return;
    }

  g_rec_mutex_lock_slowpath (rec_mutex);
}

void
g_rec_mutex_unlock (GRecMutex *rec_mutex)
{
  if (--rec_mutex->i[0] == 0)
    {
      gulong tid;

      tid = (gulong) rec_mutex->p;
      g_atomic_pointer_set (&rec_mutex->p, NULL);

      if G_LIKELY (~tid & 1)
        return;

      syscall (__NR_futex, &rec_mutex->p, (gsize) FUTEX_WAKE, (gsize) 1, NULL);
    }
}

gboolean
g_rec_mutex_trylock (GRecMutex *rec_mutex)
{
  gulong tid = pthread_self ();
  gulong prev;

  prev = (gulong) __sync_val_compare_and_swap (&rec_mutex->p, 0, tid);

  if G_LIKELY (prev == 0 || (prev & ~1ul) == tid)
    {
      rec_mutex->i[0]++;
      return TRUE;
    }

  return FALSE;
}

/* {{{1 GRWLock */

void
g_rw_lock_init (GRWLock *lock)
{
  lock->i[0] = 0;
  lock->i[1] = 0;
}

void
g_rw_lock_clear (GRWLock *lock)
{
}

void
g_rw_lock_writer_lock (GRWLock *lock)
{
  abort ();
}

gboolean
g_rw_lock_writer_trylock (GRWLock *lock)
{
  abort ();
}

void
g_rw_lock_writer_unlock (GRWLock *lock)
{
  abort ();
}

void
g_rw_lock_reader_lock (GRWLock *lock)
{
  abort ();
}

gboolean
g_rw_lock_reader_trylock (GRWLock *lock)
{
  abort ();
}

void
g_rw_lock_reader_unlock (GRWLock *lock)
{
  abort ();
}

/* {{{1 GCond */

void
g_cond_init (GCond *cond)
{
  cond->i[0] = 0;
}

void
g_cond_clear (GCond *cond)
{
}

void
g_cond_wait (GCond  *cond,
             GMutex *mutex)
{
  guint sampled = cond->i[0];

  g_mutex_unlock (mutex);
  syscall (__NR_futex, cond->i, (gsize) FUTEX_WAIT, (gsize) sampled, NULL);
  g_mutex_lock (mutex);
}

void
g_cond_signal (GCond *cond)
{
  g_atomic_int_inc (cond->i);

  syscall (__NR_futex, cond->i, (gsize) FUTEX_WAKE, (gsize) 1, NULL);
}

void
g_cond_broadcast (GCond *cond)
{
  g_atomic_int_inc (cond->i);

  syscall (__NR_futex, cond->i, (gsize) FUTEX_WAKE, (gsize) INT_MAX, NULL);
}

gboolean
g_cond_timed_wait (GCond    *cond,
                   GMutex   *mutex,
                   GTimeVal *abs_time)
{
  abort ();
}

gboolean
g_cond_timedwait (GCond  *cond,
                  GMutex *mutex,
                  gint64  abs_time)
{
  abort ();
}

/* {{{1 GPrivate */

static pthread_key_t
g_private_get_impl (GPrivate *key)
{
  pthread_key_t impl = (gsize) key->p;

  if G_UNLIKELY (impl == 0)
    {
      gint status;

      status = pthread_key_create (&impl, key->notify);
      if G_UNLIKELY (status != 0)
        g_mutex_abort (status, "pthread_key_create");

      if (!g_atomic_pointer_compare_and_exchange (&key->p, NULL, (gsize) impl))
        {
          pthread_key_delete (impl);
          impl = (gsize) key->p;
        }
    }

  return impl;
}

gpointer
g_private_get (GPrivate *key)
{
  /* quote POSIX: No errors are returned from pthread_getspecific(). */
  return pthread_getspecific (g_private_get_impl (key));
}

void
g_private_set (GPrivate *key,
               gpointer  value)
{
  gint status;

  if G_UNLIKELY ((status = pthread_setspecific (g_private_get_impl (key), value)) != 0)
    g_mutex_abort (status, "pthread_setspecific");
}
/* {{{1 Epilogue */
/* vim:set foldmethod=marker: */
