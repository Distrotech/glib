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

#include <pthread.h>
#include <string.h>
#include <stdlib.h>
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
  fprintf (stderr, "GLib (gmutex-posix.c): Unexpected error from C library during '%s': %s.  Aborting.\n",
           strerror (status), function);
  abort ();
}

/* {{{1 GMutex */

static pthread_mutex_t *
g_mutex_impl_new (void)
{
  pthread_mutexattr_t *pattr = NULL;
  pthread_mutex_t *mutex;
  gint status;

  mutex = malloc (sizeof (pthread_mutex_t));
  if G_UNLIKELY (mutex == NULL)
    g_mutex_abort (errno, "malloc");

#ifdef PTHREAD_ADAPTIVE_MUTEX_INITIALIZER_NP
  pthread_mutexattr_t attr;
  pthread_mutexattr_init (&attr);
  pthread_mutexattr_settype (&attr, PTHREAD_MUTEX_ADAPTIVE_NP);
  pattr = &attr;
#endif

  if G_UNLIKELY ((status = pthread_mutex_init (mutex, pattr)) != 0)
    g_mutex_abort (status, "pthread_mutex_init");

#ifdef PTHREAD_ADAPTIVE_MUTEX_NP
  pthread_mutexattr_destroy (&attr);
#endif

  return mutex;
}

static void
g_mutex_impl_free (pthread_mutex_t *mutex)
{
  pthread_mutex_destroy (mutex);
  free (mutex);
}

static pthread_mutex_t *
g_mutex_get_impl (GMutex *mutex)
{
  pthread_mutex_t *impl = mutex->p;

  if G_UNLIKELY (impl == NULL)
    {
      impl = g_mutex_impl_new ();
      if (!g_atomic_pointer_compare_and_exchange (&mutex->p, NULL, impl))
        g_mutex_impl_free (impl);
      impl = mutex->p;
    }

  return impl;
}

/**
 * g_mutex_init:
 * @mutex: an uninitialized #GMutex
 *
 * Initializes a #GMutex so that it can be used.
 *
 * This function is useful to initialize a mutex that has been
 * allocated on the stack, or as part of a larger structure.
 * It is not necessary to initialize a mutex that has been
 * created with g_mutex_new(). Also see #G_MUTEX_INIT for an
 * alternative way to initialize statically allocated mutexes.
 *
 * |[
 *   typedef struct {
 *     GMutex m;
 *     ...
 *   } Blob;
 *
 * Blob *b;
 *
 * b = g_new (Blob, 1);
 * g_mutex_init (&b->m);
 * ]|
 *
 * To undo the effect of g_mutex_init() when a mutex is no longer
 * needed, use g_mutex_clear().
 *
 * Calling g_mutex_init() on an already initialized #GMutex leads
 * to undefined behaviour.
 *
 * Since: 2.32
 */
void
g_mutex_init (GMutex *mutex)
{
  mutex->p = g_mutex_impl_new ();
}

/**
 * g_mutex_clear:
 * @mutex: an initialized #GMutex
 *
 * Frees the resources allocated to a mutex with g_mutex_init().
 *
 * #GMutexes that have have been created with g_mutex_new() should
 * be freed with g_mutex_free() instead.
 *
 * Calling g_mutex_clear() on a locked mutex leads to undefined
 * behaviour.
 *
 * Sine: 2.32
 */
void
g_mutex_clear (GMutex *mutex)
{
  g_mutex_impl_free (mutex->p);
}

/**
 * g_mutex_lock:
 * @mutex: a #GMutex
 *
 * Locks @mutex. If @mutex is already locked by another thread, the
 * current thread will block until @mutex is unlocked by the other
 * thread.
 *
 * This function can be used even if g_thread_init() has not yet been
 * called, and, in that case, will do nothing.
 *
 * <note>#GMutex is neither guaranteed to be recursive nor to be
 * non-recursive, i.e. a thread could deadlock while calling
 * g_mutex_lock(), if it already has locked @mutex. Use
 * #GRecMutex if you need recursive mutexes.</note>
 */
void
g_mutex_lock (GMutex *mutex)
{
  gint status;

  if G_UNLIKELY ((status = pthread_mutex_lock (g_mutex_get_impl (mutex))) != 0)
    g_mutex_abort (status, "pthread_mutex_lock");
}

/**
 * g_mutex_unlock:
 * @mutex: a #GMutex
 *
 * Unlocks @mutex. If another thread is blocked in a g_mutex_lock()
 * call for @mutex, it will become unblocked and can lock @mutex itself.
 *
 * Calling g_mutex_unlock() on a mutex that is not locked by the
 * current thread leads to undefined behaviour.
 *
 * This function can be used even if g_thread_init() has not yet been
 * called, and, in that case, will do nothing.
 */
void
g_mutex_unlock (GMutex *mutex)
{
  gint status;

  if G_UNLIKELY ((status = pthread_mutex_unlock (mutex->p)) != 0)
    g_mutex_abort (status, "pthread_mutex_unlock");
}

/**
 * g_mutex_trylock:
 * @mutex: a #GMutex
 *
 * Tries to lock @mutex. If @mutex is already locked by another thread,
 * it immediately returns %FALSE. Otherwise it locks @mutex and returns
 * %TRUE.
 *
 * This function can be used even if g_thread_init() has not yet been
 * called, and, in that case, will immediately return %TRUE.
 *
 * <note>#GMutex is neither guaranteed to be recursive nor to be
 * non-recursive, i.e. the return value of g_mutex_trylock() could be
 * both %FALSE or %TRUE, if the current thread already has locked
 * @mutex. Use #GRecMutex if you need recursive mutexes.</note>

 * Returns: %TRUE if @mutex could be locked
 */
gboolean
g_mutex_trylock (GMutex *mutex)
{
  gint status;

  if G_LIKELY ((status = pthread_mutex_trylock (g_mutex_get_impl (mutex))) == 0)
    return TRUE;

  if G_UNLIKELY (status != EBUSY)
    g_mutex_abort (status, "pthread_mutex_trylock");

  return FALSE;
}

/* {{{1 GRecMutex */

static pthread_mutex_t *
g_rec_mutex_impl_new (void)
{
  pthread_mutexattr_t attr;
  pthread_mutex_t *mutex;
  gint status;

  mutex = malloc (sizeof (pthread_mutex_t));
  if G_UNLIKELY (mutex == NULL)
    g_mutex_abort (errno, "malloc");
  pthread_mutexattr_init (&attr);
  pthread_mutexattr_settype (&attr, PTHREAD_MUTEX_RECURSIVE);
  if G_UNLIKELY ((status = pthread_mutex_init (mutex, &attr)) != 0)
    g_mutex_abort (status, "pthread_mutex_init");
  pthread_mutexattr_destroy (&attr);

  return mutex;
}

static void
g_rec_mutex_impl_free (pthread_mutex_t *mutex)
{
  pthread_mutex_destroy (mutex);
  free (mutex);
}

static pthread_mutex_t *
g_rec_mutex_get_impl (GRecMutex *mutex)
{
  pthread_mutex_t *impl = mutex->p;

  if G_UNLIKELY (impl == NULL)
    {
      impl = g_rec_mutex_impl_new ();
      if (!g_atomic_pointer_compare_and_exchange (&mutex->p, NULL, impl))
        g_rec_mutex_impl_free (impl);
      impl = mutex->p;
    }

  return impl;
}

/**
 * g_rec_mutex_init:
 * @rec_mutex: an uninitialized #GRecMutex
 *
 * Initializes a #GRecMutex so that it can be used.
 *
 * This function is useful to initialize a recursive mutex
 * that has been allocated on the stack, or as part of a larger
 * structure.
 * It is not necessary to initialize a recursive mutex that has
 * been created with g_rec_mutex_new(). Also see #G_REC_MUTEX_INIT
 * for an alternative way to initialize statically allocated
 * recursive mutexes.
 *
 * |[
 *   typedef struct {
 *     GRecMutex m;
 *     ...
 *   } Blob;
 *
 * Blob *b;
 *
 * b = g_new (Blob, 1);
 * g_rec_mutex_init (&b->m);
 * ]|
 *
 * Calling g_rec_mutex_init() on an already initialized #GRecMutex
 * leads to undefined behaviour.
 *
 * To undo the effect of g_rec_mutex_init() when a recursive mutex
 * is no longer needed, use g_rec_mutex_clear().
 *
 * Since: 2.32
 */
void
g_rec_mutex_init (GRecMutex *rec_mutex)
{
  rec_mutex->p = g_rec_mutex_impl_new ();
}

/**
 * g_rec_mutex_clear:
 * @rec_mutex: an initialized #GRecMutex
 *
 * Frees the resources allocated to a recursive mutex with
 * g_rec_mutex_init().
 *
 * #GRecMutexes that have have been created with g_rec_mutex_new()
 * should be freed with g_rec_mutex_free() instead.
 *
 * Calling g_rec_mutex_clear() on a locked recursive mutex leads
 * to undefined behaviour.
 *
 * Sine: 2.32
 */
void
g_rec_mutex_clear (GRecMutex *rec_mutex)
{
  if (rec_mutex->p)
    g_rec_mutex_impl_free (rec_mutex->p);
}

/**
 * g_rec_mutex_lock:
 * @rec_mutex: a #GRecMutex
 *
 * Locks @rec_mutex. If @rec_mutex is already locked by another
 * thread, the current thread will block until @rec_mutex is
 * unlocked by the other thread. If @rec_mutex is already locked
 * by the current thread, the 'lock count' of @rec_mutex is increased.
 * The mutex will only become available again when it is unlocked
 * as many times as it has been locked.
 *
 * Since: 2.32
 */
void
g_rec_mutex_lock (GRecMutex *mutex)
{
  pthread_mutex_lock (g_rec_mutex_get_impl (mutex));
}

/**
 * g_rec_mutex_unlock:
 * @rec_mutex: a #RecGMutex
 *
 * Unlocks @rec_mutex. If another thread is blocked in a
 * g_rec_mutex_lock() call for @rec_mutex, it will become unblocked
 * and can lock @rec_mutex itself.
 *
 * Calling g_rec_mutex_unlock() on a recursive mutex that is not
 * locked by the current thread leads to undefined behaviour.
 *
 * Since: 2.32
 */
void
g_rec_mutex_unlock (GRecMutex *rec_mutex)
{
  pthread_mutex_unlock (rec_mutex->p);
}

/**
 * g_rec_mutex_trylock:
 * @rec_mutex: a #GRecMutex
 *
 * Tries to lock @rec_mutex. If @rec_mutex is already locked
 * by another thread, it immediately returns %FALSE. Otherwise
 * it locks @rec_mutex and returns %TRUE.
 *
 * Returns: %TRUE if @rec_mutex could be locked
 *
 * Since: 2.32
 */
gboolean
g_rec_mutex_trylock (GRecMutex *rec_mutex)
{
  if (pthread_mutex_trylock (g_rec_mutex_get_impl (rec_mutex)) != 0)
    return FALSE;

  return TRUE;
}

/* {{{1 GRWLock */

static pthread_rwlock_t *
g_rw_lock_impl_new (void)
{
  pthread_rwlock_t *rwlock;
  gint status;

  rwlock = malloc (sizeof (pthread_rwlock_t));
  if G_UNLIKELY (rwlock == NULL)
    g_mutex_abort (errno, "malloc");

  if G_UNLIKELY ((status = pthread_rwlock_init (rwlock, NULL)) != 0)
    g_mutex_abort (status, "pthread_rwlock_init");

  return rwlock;
}

static void
g_rw_lock_impl_free (pthread_rwlock_t *rwlock)
{
  pthread_rwlock_destroy (rwlock);
  free (rwlock);
}

static pthread_rwlock_t *
g_rw_lock_get_impl (GRWLock *lock)
{
  pthread_rwlock_t *impl = lock->p;

  if G_UNLIKELY (impl == NULL)
    {
      impl = g_rw_lock_impl_new ();
      if (!g_atomic_pointer_compare_and_exchange (&lock->p, NULL, impl))
        g_rw_lock_impl_free (impl);
      impl = lock->p;
    }

  return impl;
}


/**
 * g_rw_lock_init:
 * @lock: an uninitialized #GRWLock
 *
 * Initializes a #GRWLock so that it can be used.
 *
 * This function is useful to initialize a lock that has been
 * allocated on the stack, or as part of a larger structure.
 * Also see #G_RW_LOCK_INIT for an alternative way to initialize
 * statically allocated locks.
 *
 * |[
 *   typedef struct {
 *     GRWLock l;
 *     ...
 *   } Blob;
 *
 * Blob *b;
 *
 * b = g_new (Blob, 1);
 * g_rw_lock_init (&b->l);
 * ]|
 *
 * To undo the effect of g_rw_lock_init() when a lock is no longer
 * needed, use g_rw_lock_clear().
 *
 * Calling g_rw_lock_init() on an already initialized #GRWLock leads
 * to undefined behaviour.
 *
 * Since: 2.32
 */
void
g_rw_lock_init (GRWLock *lock)
{
  lock->p = g_rw_lock_impl_new ();
}

/**
 * g_rw_lock_clear:
 * @lock: an initialized #GRWLock
 *
 * Frees the resources allocated to a lock with g_rw_lock_init().
 *
 * Calling g_rw_lock_clear() when any thread holds the lock
 * leads to undefined behaviour.
 *
 * Sine: 2.32
 */
void
g_rw_lock_clear (GRWLock *lock)
{
  g_rw_lock_impl_free (lock->p);
}

/**
 * g_rw_lock_writer_lock:
 * @lock: a #GRWLock
 *
 * Obtain a write lock on @lock. If any thread already holds
 * a read or write lock on @lock, the current thread will block
 * until all other threads have dropped their locks on @lock.
 *
 * Since: 2.32
 */
void
g_rw_lock_writer_lock (GRWLock *lock)
{
  pthread_rwlock_wrlock (g_rw_lock_get_impl (lock));
}

/**
 * g_rw_lock_writer_trylock:
 * @lock: a #GRWLock
 *
 * Tries to obtain a write lock on @lock. If any other thread holds
 * a read or write lock on @lock, it immediately returns %FALSE.
 * Otherwise it locks @lock and returns %TRUE.
 *
 * Returns: %TRUE if @lock could be locked
 *
 * Since: 2.32
 */
gboolean
g_rw_lock_writer_trylock (GRWLock *lock)
{
  if (pthread_rwlock_trywrlock (g_rw_lock_get_impl (lock)) != 0)
    return FALSE;

  return TRUE;
}

/**
 * g_rw_lock_writer_unlock:
 * @lock: a #GRWLock
 *
 * Release a write lock on @lock.
 *
 * Calling g_rw_lock_writer_unlock() on a lock that is not held
 * by the current thread leads to undefined behaviour.
 *
 * Since: 2.32
 */
void
g_rw_lock_writer_unlock (GRWLock *lock)
{
  pthread_rwlock_unlock (lock->p);
}

/**
 * g_rw_lock_reader_lock:
 * @lock: a #GRWLock
 *
 * Obtain a read lock on @lock. If another thread currently holds
 * the write lock on @lock or blocks waiting for it, the current
 * thread will block. Read locks can be taken recursively.
 *
 * It is implementation-defined how many threads are allowed to
 * hold read locks on the same lock simultaneously.
 *
 * Since: 2.32
 */
void
g_rw_lock_reader_lock (GRWLock *lock)
{
  pthread_rwlock_rdlock (g_rw_lock_get_impl (lock));
}

/**
 * g_rw_lock_reader_trylock:
 * @lock: a #GRWLock
 *
 * Tries to obtain a read lock on @lock and returns %TRUE if
 * the read lock was successfully obtained. Otherwise it
 * returns %FALSE.
 *
 * Returns: %TRUE if @lock could be locked
 *
 * Since: 2.32
 */
gboolean
g_rw_lock_reader_trylock (GRWLock *lock)
{
  if (pthread_rwlock_tryrdlock (g_rw_lock_get_impl (lock)) != 0)
    return FALSE;

  return TRUE;
}

/**
 * g_rw_lock_reader_unlock:
 * @lock: a #GRWLock
 *
 * Release a read lock on @lock.
 *
 * Calling g_rw_lock_reader_unlock() on a lock that is not held
 * by the current thread leads to undefined behaviour.
 *
 * Since: 2.32
 */
void
g_rw_lock_reader_unlock (GRWLock *lock)
{
  pthread_rwlock_unlock (lock->p);
}

/* {{{1 GCond */

static pthread_cond_t *
g_cond_impl_new (void)
{
  pthread_cond_t *cond;
  gint status;

  cond = malloc (sizeof (pthread_cond_t));
  if G_UNLIKELY (cond == NULL)
    g_mutex_abort (errno, "malloc");

  if G_UNLIKELY ((status = pthread_cond_init (cond, NULL)) != 0)
    g_mutex_abort (status, "pthread_cond_init");

  return cond;
}

static void
g_cond_impl_free (pthread_cond_t *cond)
{
  pthread_cond_destroy (cond);
  free (cond);
}

static pthread_cond_t *
g_cond_get_impl (GCond *cond)
{
  pthread_cond_t *impl = cond->p;

  if G_UNLIKELY (impl == NULL)
    {
      impl = g_cond_impl_new ();
      if (!g_atomic_pointer_compare_and_exchange (&cond->p, NULL, impl))
        g_cond_impl_free (impl);
      impl = cond->p;
    }

  return impl;
}

/**
 * g_cond_init:
 * @cond: an uninitialized #GCond
 *
 * Initialized a #GCond so that it can be used.
 *
 * This function is useful to initialize a #GCond that has been
 * allocated on the stack, or as part of a larger structure.
 * It is not necessary to initialize a #GCond that has been
 * created with g_cond_new(). Also see #G_COND_INIT for an
 * alternative way to initialize statically allocated #GConds.
 *
 * To undo the effect of g_cond_init() when a #GCond is no longer
 * needed, use g_cond_clear().
 *
 * Calling g_cond_init() on an already initialized #GCond leads
 * to undefined behaviour.
 *
 * Since: 2.32
 */
void
g_cond_init (GCond *cond)
{
  cond->p = g_cond_impl_new ();
}

/**
 * g_cond_clear:
 * @cond: an initialized #GCond
 *
 * Frees the resources allocated to a #GCond with g_cond_init().
 *
 * #GConds that have been created with g_cond_new() should
 * be freed with g_cond_free() instead.
 *
 * Calling g_cond_clear() for a #GCond on which threads are
 * blocking leads to undefined behaviour.
 *
 * Since: 2.32
 */
void
g_cond_clear (GCond *cond)
{
  g_cond_impl_free (cond->p);
}

/**
 * g_cond_wait:
 * @cond: a #GCond
 * @mutex: a #GMutex that is currently locked
 *
 * Waits until this thread is woken up on @cond. The @mutex is unlocked
 * before falling asleep and locked again before resuming.
 *
 * This function can be used even if g_thread_init() has not yet been
 * called, and, in that case, will immediately return.
 */
void
g_cond_wait (GCond  *cond,
             GMutex *mutex)
{
  gint status;

  /* the mutex is locked so ->p is set */
  if G_UNLIKELY ((status = pthread_cond_wait (g_cond_get_impl (cond), mutex->p)) != 0)
    g_mutex_abort (status, "pthread_cond_wait");
}

/**
 * g_cond_signal:
 * @cond: a #GCond
 *
 * If threads are waiting for @cond, at least one of them is unblocked.
 * If no threads are waiting for @cond, this function has no effect.
 * It is good practice to hold the same lock as the waiting thread
 * while calling this function, though not required.
 *
 * This function can be used even if g_thread_init() has not yet been
 * called, and, in that case, will do nothing.
 */
void
g_cond_signal (GCond *cond)
{
  gint status;

  if G_UNLIKELY ((status = pthread_cond_signal (g_cond_get_impl (cond))) != 0)
    g_mutex_abort (status, "pthread_cond_signal");
}

/**
 * g_cond_broadcast:
 * @cond: a #GCond
 *
 * If threads are waiting for @cond, all of them are unblocked.
 * If no threads are waiting for @cond, this function has no effect.
 * It is good practice to lock the same mutex as the waiting threads
 * while calling this function, though not required.
 *
 * This function can be used even if g_thread_init() has not yet been
 * called, and, in that case, will do nothing.
 */
void
g_cond_broadcast (GCond *cond)
{
  gint status;

  if G_UNLIKELY ((status = pthread_cond_broadcast (g_cond_get_impl (cond))) != 0)
    g_mutex_abort (status, "pthread_cond_broadcast");
}

/**
 * g_cond_timed_wait:
 * @cond: a #GCond
 * @mutex: a #GMutex that is currently locked
 * @abs_time: a #GTimeVal, determining the final time
 *
 * Waits until this thread is woken up on @cond, but not longer than
 * until the time specified by @abs_time. The @mutex is unlocked before
 * falling asleep and locked again before resuming.
 *
 * If @abs_time is %NULL, g_cond_timed_wait() acts like g_cond_wait().
 *
 * This function can be used even if g_thread_init() has not yet been
 * called, and, in that case, will immediately return %TRUE.
 *
 * To easily calculate @abs_time a combination of g_get_current_time()
 * and g_time_val_add() can be used.
 *
 * Returns: %TRUE if @cond was signalled, or %FALSE on timeout
 */
gboolean
g_cond_timed_wait (GCond    *cond,
                   GMutex   *mutex,
                   GTimeVal *abs_time)
{
  struct timespec end_time;
  gint status;

  if (abs_time == NULL)
    {
      g_cond_wait (cond, mutex);
      return TRUE;
    }

  end_time.tv_sec = abs_time->tv_sec;
  end_time.tv_nsec = abs_time->tv_usec * 1000;

  if ((status = pthread_cond_timedwait (g_cond_get_impl (cond), mutex->p, &end_time)) == 0)
    return TRUE;

  if G_UNLIKELY (status != ETIMEDOUT)
    g_mutex_abort (status, "pthread_cond_timedwait");

  return FALSE;
}

/**
 * g_cond_timedwait:
 * @cond: a #GCond
 * @mutex: a #GMutex that is currently locked
 * @abs_time: the final time, in microseconds
 *
 * A variant of g_cond_timed_wait() that takes @abs_time
 * as a #gint64 instead of a #GTimeVal.
 * See g_cond_timed_wait() for details.
 *
 * Returns: %TRUE if @cond was signalled, or %FALSE on timeout
 *
 * Since: 2.32
 */
gboolean
g_cond_timedwait (GCond  *cond,
                  GMutex *mutex,
                  gint64  abs_time)
{
  struct timespec end_time;
  gint status;

  end_time.tv_sec = abs_time / 1000000;
  end_time.tv_nsec = (abs_time % 1000000) * 1000;

  if ((status = pthread_cond_timedwait (g_cond_get_impl (cond), mutex->p, &end_time)) == 0)
    return TRUE;

  if G_UNLIKELY (status != ETIMEDOUT)
    g_mutex_abort (status, "pthread_cond_timedwait");

  return FALSE;
}

/* {{{1 GPrivate */
static pthread_key_t *
g_private_impl_new (GDestroyNotify notify)
{
  pthread_key_t *key;
  gint status;

  key = malloc (sizeof (pthread_key_t));
  if G_UNLIKELY (key == NULL)
    g_mutex_abort (errno, "malloc");
  status = pthread_key_create (key, notify);
  if G_UNLIKELY (status != 0)
    g_mutex_abort (status, "pthread_key_create");

  return key;
}

static void
g_private_impl_free (pthread_key_t *key)
{
  gint status;

  status = pthread_key_delete (*key);
  if G_UNLIKELY (status != 0)
    g_mutex_abort (status, "pthread_key_delete");
  free (key);
}

static pthread_key_t *
g_private_get_impl (GPrivate *key)
{
  pthread_key_t *impl = key->p;

  if G_UNLIKELY (impl == NULL)
    {
      impl = g_private_impl_new (key->notify);
      if (!g_atomic_pointer_compare_and_exchange (&key->p, NULL, impl))
        {
          g_private_impl_free (impl);
          impl = key->p;
        }
    }

  return impl;
}

/**
 * g_private_get:
 * @private_key: a #GPrivate
 *
 * Returns the pointer keyed to @private_key for the current thread. If
 * g_private_set() hasn't been called for the current @private_key and
 * thread yet, this pointer will be %NULL.
 *
 * This function can be used even if g_thread_init() has not yet been
 * called, and, in that case, will return the value of @private_key
 * casted to #gpointer. Note however, that private data set
 * <emphasis>before</emphasis> g_thread_init() will
 * <emphasis>not</emphasis> be retained <emphasis>after</emphasis> the
 * call. Instead, %NULL will be returned in all threads directly after
 * g_thread_init(), regardless of any g_private_set() calls issued
 * before threading system initialization.
 *
 * Returns: the corresponding pointer
 */
gpointer
g_private_get (GPrivate *key)
{
  /* quote POSIX: No errors are returned from pthread_getspecific(). */
  return pthread_getspecific (*g_private_get_impl (key));
}

/**
 * g_private_set:
 * @private_key: a #GPrivate
 * @data: the new pointer
 *
 * Sets the pointer keyed to @private_key for the current thread.
 *
 * This function can be used even if g_thread_init() has not yet been
 * called, and, in that case, will set @private_key to @data casted to
 * #GPrivate*. See g_private_get() for resulting caveats.
 */
void
g_private_set (GPrivate *key,
               gpointer  value)
{
  gint status;

  if G_UNLIKELY ((status = pthread_setspecific (*g_private_get_impl (key), value)) != 0)
    g_mutex_abort (status, "pthread_setspecific");
}
/* {{{1 Epilogue */
/* vim:set foldmethod=marker: */
