/* GLIB - Library of useful routines for C programming
 * Copyright (C) 1995-1997  Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
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

#if defined(G_DISABLE_SINGLE_INCLUDES) && !defined (__GLIB_H_INSIDE__) && !defined (GLIB_COMPILATION)
#error "Only <glib.h> can be included directly."
#endif

#ifndef __G_THREAD_H__
#define __G_THREAD_H__

#include <glib/gerror.h>
#include <glib/gutils.h>        /* for G_INLINE_FUNC */
#include <glib/gatomic.h>       /* for g_atomic_pointer_get */
#include <glib/gmutex.h>

G_BEGIN_DECLS

/* GLib Thread support
 */

extern GQuark g_thread_error_quark (void);
#define G_THREAD_ERROR g_thread_error_quark ()

typedef enum
{
  G_THREAD_ERROR_AGAIN /* Resource temporarily unavailable */
} GThreadError;

typedef gpointer (*GThreadFunc) (gpointer data);

typedef struct _GThread         GThread;
typedef struct _GStaticPrivate  GStaticPrivate;

void     g_thread_init   (gpointer vtable);

gboolean g_thread_get_initialized (void);

GLIB_VAR gboolean g_threads_got_initialized;

#if defined(G_THREADS_MANDATORY)
#define g_thread_supported()     1
#else
#define g_thread_supported()    (g_threads_got_initialized)
#endif

GThread *g_thread_create                 (GThreadFunc   func,
                                          gpointer      data,
                                          gboolean      joinable,
                                          GError      **error);

GThread *g_thread_create_with_stack_size (GThreadFunc   func,
                                          gpointer      data,
                                          gboolean      joinable,
                                          gsize         stack_size,
                                          GError      **error);

GThread* g_thread_self                   (void);
void     g_thread_exit                   (gpointer      retval);
gpointer g_thread_join                   (GThread      *thread);
void     g_thread_yield                  (void);

void     g_thread_foreach                (GFunc         thread_func,
                                          gpointer      user_data);

struct _GStaticPrivate
{
  /*< private >*/
  guint index;
};
#define G_STATIC_PRIVATE_INIT { 0 }
void     g_static_private_init           (GStaticPrivate   *private_key);
gpointer g_static_private_get            (GStaticPrivate   *private_key);
void     g_static_private_set            (GStaticPrivate   *private_key,
					  gpointer          data,
					  GDestroyNotify    notify);
void     g_static_private_free           (GStaticPrivate   *private_key);

typedef enum
{
  G_ONCE_STATUS_NOTCALLED,
  G_ONCE_STATUS_PROGRESS,
  G_ONCE_STATUS_READY
} GOnceStatus;

typedef struct _GOnce GOnce;
struct _GOnce
{
  volatile GOnceStatus status;
  volatile gpointer retval;
};

#define G_ONCE_INIT { G_ONCE_STATUS_NOTCALLED, NULL }

gpointer g_once_impl (GOnce *once, GThreadFunc func, gpointer arg);

#ifdef G_ATOMIC_OP_MEMORY_BARRIER_NEEDED
# define g_once(once, func, arg) g_once_impl ((once), (func), (arg))
#else /* !G_ATOMIC_OP_MEMORY_BARRIER_NEEDED*/
# define g_once(once, func, arg) \
  (((once)->status == G_ONCE_STATUS_READY) ? \
   (once)->retval : \
   g_once_impl ((once), (func), (arg)))
#endif /* G_ATOMIC_OP_MEMORY_BARRIER_NEEDED */

/* initialize-once guards, keyed by value_location */
G_INLINE_FUNC gboolean  g_once_init_enter       (volatile gsize *value_location);
gboolean                g_once_init_enter_impl  (volatile gsize *value_location);
void                    g_once_init_leave       (volatile gsize *value_location,
                                                 gsize           initialization_value);
#if defined (G_CAN_INLINE) || defined (__G_THREAD_C__)
G_INLINE_FUNC gboolean
g_once_init_enter (volatile gsize *value_location)
{
  if G_LIKELY ((gpointer) g_atomic_pointer_get (value_location) != NULL)
    return FALSE;
  else
    return g_once_init_enter_impl (value_location);
}
#endif /* G_CAN_INLINE || __G_THREAD_C__ */

#define G_LOCK_NAME(name)               g__ ## name ## _lock
#define G_LOCK_DEFINE_STATIC(name)    static G_LOCK_DEFINE (name)
#define G_LOCK_DEFINE(name)           \
  GMutex G_LOCK_NAME (name)
#define G_LOCK_EXTERN(name)           extern GMutex G_LOCK_NAME (name)

#ifdef G_DEBUG_LOCKS
#  define G_LOCK(name)                G_STMT_START{             \
      g_log (G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG,                   \
             "file %s: line %d (%s): locking: %s ",             \
             __FILE__,        __LINE__, G_STRFUNC,              \
             #name);                                            \
      g_mutex_lock (&G_LOCK_NAME (name));                       \
   }G_STMT_END
#  define G_UNLOCK(name)              G_STMT_START{             \
      g_log (G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG,                   \
             "file %s: line %d (%s): unlocking: %s ",           \
             __FILE__,        __LINE__, G_STRFUNC,              \
             #name);                                            \
     g_mutex_unlock (&G_LOCK_NAME (name));                      \
   }G_STMT_END
#  define G_TRYLOCK(name)                                       \
      (g_log (G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG,                  \
             "file %s: line %d (%s): try locking: %s ",         \
             __FILE__,        __LINE__, G_STRFUNC,              \
             #name), g_mutex_trylock (&G_LOCK_NAME (name)))
#else  /* !G_DEBUG_LOCKS */
#  define G_LOCK(name) g_mutex_lock       (&G_LOCK_NAME (name))
#  define G_UNLOCK(name) g_mutex_unlock   (&G_LOCK_NAME (name))
#  define G_TRYLOCK(name) g_mutex_trylock (&G_LOCK_NAME (name))
#endif /* !G_DEBUG_LOCKS */

GMutex *                g_mutex_new                                     (void);
void                    g_mutex_free                                    (GMutex         *mutex);

GCond *                 g_cond_new                                      (void);
void                    g_cond_free                                     (GCond          *cond);

GPrivate *              g_private_new                                   (GDestroyNotify  notify);

G_END_DECLS

#endif /* __G_THREAD_H__ */
