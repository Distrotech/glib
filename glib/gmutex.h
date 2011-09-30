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

#if defined(G_DISABLE_SINGLE_INCLUDES) && !defined (__GLIB_H_INSIDE__) && !defined (GLIB_COMPILATION)
#error "Only <glib.h> can be included directly."
#endif

#ifndef __G_MUTEX_H__
#define __G_MUTEX_H__

#include <glib/gtypes.h>

G_BEGIN_DECLS

typedef union
{
  /*< private >*/
  gpointer p;
  guint i[2];
} GMutex;

typedef union
{
  /*< private >*/
  gpointer p;
  guint i[2];
} GRWLock;

typedef union
{
  /*< private >*/
  gpointer p;
  guint i[2];
} GRecMutex;

typedef struct
{
  /*< private >*/
  gpointer p;
  guint i[2];
} GCond;

#define G_PRIVATE_INIT(notify) { NULL, (notify) }
typedef struct
{
  /*< private >*/
  gpointer       p;
  GDestroyNotify notify;
} GPrivate;

void            g_mutex_init                    (GMutex         *mutex);
void            g_mutex_clear                   (GMutex         *mutex);

void            g_mutex_lock                    (GMutex         *mutex);
void            g_mutex_unlock                  (GMutex         *mutex);
gboolean        g_mutex_trylock                 (GMutex         *mutex);

void            g_rw_lock_init                  (GRWLock        *lock);
void            g_rw_lock_clear                 (GRWLock        *lock);
void            g_rw_lock_writer_lock           (GRWLock        *lock);
gboolean        g_rw_lock_writer_trylock        (GRWLock        *lock);
void            g_rw_lock_writer_unlock         (GRWLock        *lock);
void            g_rw_lock_reader_lock           (GRWLock        *lock);
gboolean        g_rw_lock_reader_trylock        (GRWLock        *lock);
void            g_rw_lock_reader_unlock         (GRWLock        *lock);

void            g_rec_mutex_init                (GRecMutex      *rec_mutex);
void            g_rec_mutex_clear               (GRecMutex      *rec_mutex);
void            g_rec_mutex_lock                (GRecMutex      *rec_mutex);
gboolean        g_rec_mutex_trylock             (GRecMutex      *rec_mutex);
void            g_rec_mutex_unlock              (GRecMutex      *rec_mutex);

void            g_cond_init                     (GCond          *cond);
void            g_cond_clear                    (GCond          *cond);

void            g_cond_wait                     (GCond          *cond,
                                                 GMutex         *mutex);
void            g_cond_signal                   (GCond          *cond);
void            g_cond_broadcast                (GCond          *cond);
gboolean        g_cond_timed_wait               (GCond          *cond,
                                                 GMutex         *mutex,
                                                 GTimeVal       *timeval);
gboolean        g_cond_timedwait                (GCond          *cond,
                                                 GMutex         *mutex,
                                                 gint64          abs_time);

gpointer        g_private_get                   (GPrivate       *key);
void            g_private_set                   (GPrivate       *key,
                                                 gpointer        value);

G_END_DECLS

#endif /* __G_MUTEX_H__ */
