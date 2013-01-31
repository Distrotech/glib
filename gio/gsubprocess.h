/* GIO - GLib Input, Output and Streaming Library
 *
 * Copyright (C) 2012 Colin Walters <walters@verbum.org>
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
 * You should have received a copy of the GNU Lesser General
 * Public License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place, Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Author: Colin Walters <walters@verbum.org>
 */

#if !defined (__GIO_GIO_H_INSIDE__) && !defined (GIO_COMPILATION)
#error "Only <gio/gio.h> can be included directly."
#endif

#ifndef __G_SUBPROCESS_H__
#define __G_SUBPROCESS_H__

#include <gio/giotypes.h>

G_BEGIN_DECLS

#define G_TYPE_SUBPROCESS         (g_subprocess_get_type ())
#define G_SUBPROCESS(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), G_TYPE_SUBPROCESS, GSubprocess))
#define G_IS_SUBPROCESS(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), G_TYPE_SUBPROCESS))

GLIB_AVAILABLE_IN_2_36
GType            g_subprocess_get_type                  (void) G_GNUC_CONST;

/**** Core API ****/

GLIB_AVAILABLE_IN_2_36
GSubprocess *    g_subprocess_new                       (GSubprocessFlags        flags,
                                                         GError                **error,
                                                         const gchar            *argv0,
                                                         ...) G_GNUC_NULL_TERMINATED;
GLIB_AVAILABLE_IN_2_36
GSubprocess *    g_subprocess_newv                      (const gchar * const  *argv,
                                                         GSubprocessFlags      flags,
                                                         GError              **error);

GLIB_AVAILABLE_IN_2_36
GOutputStream *  g_subprocess_get_stdin_pipe            (GSubprocess          *self);

GLIB_AVAILABLE_IN_2_36
GInputStream *   g_subprocess_get_stdout_pipe           (GSubprocess          *self);

GLIB_AVAILABLE_IN_2_36
GInputStream *   g_subprocess_get_stderr_pipe           (GSubprocess          *self);

GLIB_AVAILABLE_IN_2_36
const gchar *    g_subprocess_get_identifier            (GSubprocess          *self);

#ifdef G_OS_UNIX
GLIB_AVAILABLE_IN_2_36
void             g_subprocess_send_signal               (GSubprocess          *self,
                                                         gint                  signal_num);
#endif

GLIB_AVAILABLE_IN_2_36
void             g_subprocess_force_exit                (GSubprocess          *self);

GLIB_AVAILABLE_IN_2_36
gboolean         g_subprocess_wait                      (GSubprocess          *self,
                                                         GCancellable         *cancellable,
                                                         GError              **error);

GLIB_AVAILABLE_IN_2_36
void             g_subprocess_wait_async                (GSubprocess          *self,
                                                         GCancellable         *cancellable,
                                                         GAsyncReadyCallback   callback,
                                                         gpointer              user_data);

GLIB_AVAILABLE_IN_2_36
gboolean         g_subprocess_wait_finish               (GSubprocess          *self,
                                                         GAsyncResult         *result,
                                                         GError              **error);

GLIB_AVAILABLE_IN_2_36
gboolean         g_subprocess_wait_check                (GSubprocess          *self,
                                                         GCancellable         *cancellable,
                                                         GError              **error);

GLIB_AVAILABLE_IN_2_36
void             g_subprocess_wait_check_async          (GSubprocess          *self,
                                                         GCancellable         *cancellable,
                                                         GAsyncReadyCallback   callback,
                                                         gpointer              user_data);

GLIB_AVAILABLE_IN_2_36
gboolean         g_subprocess_wait_check_finish         (GSubprocess          *self,
                                                         GAsyncResult         *result,
                                                         GError              **error);


GLIB_AVAILABLE_IN_2_36
gint             g_subprocess_get_status                (GSubprocess          *self);

GLIB_AVAILABLE_IN_2_36
gboolean         g_subprocess_get_successful            (GSubprocess          *self);

GLIB_AVAILABLE_IN_2_36
gboolean         g_subprocess_get_if_exited             (GSubprocess          *self);

GLIB_AVAILABLE_IN_2_36
gint             g_subprocess_get_exit_status           (GSubprocess          *self);

GLIB_AVAILABLE_IN_2_36
gboolean         g_subprocess_get_if_signaled           (GSubprocess          *self);

GLIB_AVAILABLE_IN_2_36
gint             g_subprocess_get_term_sig              (GSubprocess          *self);

GLIB_AVAILABLE_IN_2_36
gboolean         g_subprocess_communicate               (GSubprocess          *subprocess,
                                                         const gchar          *stdin_data,
                                                         gssize                stdin_length,
                                                         GCancellable         *cancellable,
                                                         gchar               **stdout_data,
                                                         gsize                *stdout_length,
                                                         gchar               **stderr_data,
                                                         gsize                *stderr_length,
                                                         GError              **error);
GLIB_AVAILABLE_IN_2_36
void            g_subprocess_communicate_async          (GSubprocess          *subprocess,
                                                         const gchar          *stdin_data,
                                                         gssize                stdin_length,
                                                         GCancellable         *cancellable,
                                                         GAsyncReadyCallback   callback,
                                                         gpointer              user_data);

GLIB_AVAILABLE_IN_2_36
gboolean        g_subprocess_communicate_finish         (GSubprocess          *subprocess,
                                                         GAsyncResult         *result,
                                                         gchar               **stdout_data,
                                                         gsize                *stdout_length,
                                                         gchar               **stderr_data,
                                                         gsize                *stderr_length,
                                                         GError              **error);

GLIB_AVAILABLE_IN_2_36
gboolean        g_subprocess_communicate_bytes          (GSubprocess          *subprocess,
                                                         GBytes               *stdin_bytes,
                                                         GCancellable         *cancellable,
                                                         GBytes              **stdout_bytes,
                                                         GBytes              **stderr_bytes,
                                                         GError              **error);
GLIB_AVAILABLE_IN_2_36
void            g_subprocess_communicate_bytes_async    (GSubprocess          *subprocess,
                                                         GBytes               *stdin_bytes,
                                                         GCancellable         *cancellable,
                                                         GAsyncReadyCallback   callback,
                                                         gpointer              user_data);
GLIB_AVAILABLE_IN_2_36
gboolean        g_subprocess_communicate_bytes_finish   (GSubprocess          *subprocess,
                                                         GAsyncResult         *result,
                                                         GBytes              **stdout_bytes,
                                                         GBytes              **stderr_bytes,
                                                         GError              **error);

G_END_DECLS

#endif /* __G_SUBPROCESS_H__ */
