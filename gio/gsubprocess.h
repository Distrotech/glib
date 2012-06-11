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

GLIB_AVAILABLE_IN_2_34
GType            g_subprocess_get_type (void) G_GNUC_CONST;

/**** Creation ****/

GLIB_AVAILABLE_IN_2_34
GSubprocess *    g_subprocess_new (const gchar *executable);

GLIB_AVAILABLE_IN_2_34
GSubprocess *    g_subprocess_new_with_args (const gchar     *executable,
					     ...) G_GNUC_NULL_TERMINATED;

/**** Argument control ****/

GLIB_AVAILABLE_IN_2_34
void             g_subprocess_set_argv (GSubprocess          *self,
					gchar               **argv);

GLIB_AVAILABLE_IN_2_34
void             g_subprocess_set_argv0 (GSubprocess         *self,
					 const gchar         *argv0);

GLIB_AVAILABLE_IN_2_34
void             g_subprocess_append_arg (GSubprocess       *self,
					  const gchar       *arg);

GLIB_AVAILABLE_IN_2_34
void             g_subprocess_append_args (GSubprocess       *self,
					   const gchar       *first,
					   ...) G_GNUC_NULL_TERMINATED;

GLIB_AVAILABLE_IN_2_34
void             g_subprocess_append_args_va (GSubprocess       *self,
					      va_list            args);

/**** Environment control ****/

GLIB_AVAILABLE_IN_2_34
void             g_subprocess_set_detached (GSubprocess     *self,
					    gboolean         detached);

GLIB_AVAILABLE_IN_2_34
void             g_subprocess_set_use_search_path (GSubprocess     *self,
						   gboolean         do_search_path);

GLIB_AVAILABLE_IN_2_34
void             g_subprocess_set_use_search_path_from_envp (GSubprocess     *self,
							     gboolean         do_search_path);

GLIB_AVAILABLE_IN_2_34
void             g_subprocess_set_leave_descriptors_open (GSubprocess     *self,
							  gboolean         do_leave_descriptors_open);

GLIB_AVAILABLE_IN_2_34
void             g_subprocess_setenv (GSubprocess       *self,
				      gchar             *variable,
				      gchar             *value,
				      gboolean           overwrite);

GLIB_AVAILABLE_IN_2_34
void             g_subprocess_unsetenv (GSubprocess       *self,
					const gchar       *variable);

GLIB_AVAILABLE_IN_2_34
void             g_subprocess_set_environment (GSubprocess       *self,
					       gchar            **new_envp);

GLIB_AVAILABLE_IN_2_34
void             g_subprocess_set_working_directory (GSubprocess       *self,
        					     const gchar       *working_directory);

GLIB_AVAILABLE_IN_2_34
void             g_subprocess_set_child_setup (GSubprocess           *self,
					       GSpawnChildSetupFunc   child_setup,
					       gpointer               user_data);

/**** Input and Output ****/

GLIB_AVAILABLE_IN_2_34
void             g_subprocess_set_io_priority (GSubprocess       *self,
					       gint               io_priority);

GLIB_AVAILABLE_IN_2_34
void             g_subprocess_set_standard_input_file_path (GSubprocess       *self,
							    const gchar       *file_path);

#ifdef G_OS_UNIX
GLIB_AVAILABLE_IN_2_34
void             g_subprocess_set_standard_input_unix_fd (GSubprocess       *self,
							  gint               fd);
#endif

GLIB_AVAILABLE_IN_2_34
void             g_subprocess_set_standard_input_to_devnull (GSubprocess       *self,
							     gboolean           to_devnull);

GLIB_AVAILABLE_IN_2_34
void             g_subprocess_set_standard_input_stream (GSubprocess       *self,
							 GInputStream      *stream);

GLIB_AVAILABLE_IN_2_34
void             g_subprocess_set_standard_input_bytes (GSubprocess       *self,
							GBytes            *buf);

GLIB_AVAILABLE_IN_2_34
void             g_subprocess_set_standard_input_str (GSubprocess       *self,
						      const gchar       *str);

GLIB_AVAILABLE_IN_2_34
void             g_subprocess_set_standard_output_to_devnull (GSubprocess       *self,
							      gboolean           to_devnull);

#ifdef G_OS_UNIX
GLIB_AVAILABLE_IN_2_34
void             g_subprocess_set_standard_output_unix_fd (GSubprocess       *self,
							   gint               fd);
#endif

GLIB_AVAILABLE_IN_2_34
void             g_subprocess_set_standard_error_to_devnull (GSubprocess       *self,
							     gboolean           to_devnull);

GLIB_AVAILABLE_IN_2_34
void             g_subprocess_set_standard_error_to_stdout (GSubprocess       *self,
							    gboolean           to_stdout);

#ifdef G_OS_UNIX
void             g_subprocess_set_standard_error_unix_fd (GSubprocess       *self,
							  gint               fd);
#endif

/**** Running ****/

GLIB_AVAILABLE_IN_2_34
gboolean         g_subprocess_start (GSubprocess       *self,
				     GCancellable      *cancellable,
				     GError           **error);

GLIB_AVAILABLE_IN_2_34
gboolean         g_subprocess_start_with_pipes (GSubprocess       *self,
						GOutputStream    **out_stdin_stream,
						GInputStream     **out_stdout_stream,
						GInputStream     **out_stderr_stream,
						GCancellable      *cancellable,
						GError           **error);

GLIB_AVAILABLE_IN_2_34
gboolean         g_subprocess_run_sync (GSubprocess   *self,
					GCancellable  *cancellable,
					GError       **error);

GLIB_AVAILABLE_IN_2_34
GPid             g_subprocess_get_pid (GSubprocess     *self);

typedef void  (*GSubprocessWatchFunc)    (GSubprocess      *subprocess,
					  gpointer          user_data);

GLIB_AVAILABLE_IN_2_34
GSource *        g_subprocess_add_watch (GSubprocess                  *self,
					 GSubprocessWatchFunc          function,
					 gpointer                      user_data);

GLIB_AVAILABLE_IN_2_34
GSource *        g_subprocess_add_watch_full (GSubprocess                  *self,
					      gint                          priority,
					      GSubprocessWatchFunc          function,
					      gpointer                      user_data,
					      GDestroyNotify                notify);

GLIB_AVAILABLE_IN_2_34
GSource *        g_subprocess_create_source (GSubprocess                  *self,
					     gint                          priority,
					     GSubprocessWatchFunc          function,
					     gpointer                      user_data,
					     GDestroyNotify                notify);

GLIB_AVAILABLE_IN_2_34
gboolean         g_subprocess_query_success (GSubprocess   *self,
					     GError       **error);

GLIB_AVAILABLE_IN_2_34
gint             g_subprocess_get_status_code (GSubprocess   *self);

GLIB_AVAILABLE_IN_2_34
gboolean         g_subprocess_wait_sync (GSubprocess   *self,
					 GCancellable  *cancellable,
					 GError       **error);

GLIB_AVAILABLE_IN_2_34
gboolean         g_subprocess_request_exit (GSubprocess       *self);

GLIB_AVAILABLE_IN_2_34
void             g_subprocess_force_exit (GSubprocess       *self);

/**** High level wrappers ****/

GLIB_AVAILABLE_IN_2_34
gboolean         g_subprocess_run_sync_get_output_bytes (GSubprocess          *self,
							 GBytes              **out_stdout_bytes,
							 GBytes              **out_stderr_bytes,
							 GCancellable         *cancellable,
							 GError              **error);

GLIB_AVAILABLE_IN_2_34
gboolean         g_subprocess_run_sync_get_stdout_utf8 (GSubprocess   *self,
							gchar        **output_utf8,
							GCancellable  *cancellable,
							GError       **error);

G_END_DECLS

#endif /* __G_SUBPROCESS_H__ */
