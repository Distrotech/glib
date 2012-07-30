/* GIO - GLib Input, Output and Streaming Library
 *
 * Copyright © 2012 Red Hat, Inc.
 * Copyright © 2012 Canonical Limited
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation; either version 2 of the licence or (at
 * your option) any later version.
 *
 * See the included COPYING file for more information.
 *
 * Authors: Colin Walters <walters@verbum.org>
 *          Ryan Lortie <desrt@desrt.ca>
 */

/**
 * SECTION:gsubprocess
 * @title: GSubprocess
 * @short_description: Create child processes and monitor their status
 *
 * This class wraps the lower-level g_spawn_async_with_pipes() API,
 * providing a more modern GIO-style API, such as returning
 * #GInputStream objects for child output pipes.
 *
 * There are quite a variety of ways to spawn processes for different
 * use cases; the primary intent of this class is to act as a
 * fundamental API, upon which a variety of higher-level helpers can
 * be written.
 *
 * One major advantage that GIO brings is comprehensive API for
 * asynchronous I/O, such g_output_stream_splice_async().  This makes
 * GSubprocess significantly more powerful and flexible than
 * equivalent APIs in some other languages such as the
 * <literal>subprocess.py</literal> included with Python.  For
 * example, using #GSubprocess one could create two child processes,
 * reading standard output from the first, processing it, and writing
 * to the input stream of the second, all without blocking the main
 * loop.
 *
 * Since: 2.34
 */

#include "config.h"
#include "gsubprocess.h"
#include "gasyncresult.h"
#include "giostream.h"
#include "gmemoryinputstream.h"
#include "glibintl.h"
#include "glib-private.h"

#include <string.h>
#ifdef G_OS_UNIX
#include <gio/gunixoutputstream.h>
#include <gio/gfiledescriptorbased.h>
#include <gio/gunixinputstream.h>
#include <gstdio.h>
#include <glib-unix.h>
#include <fcntl.h>
#endif
#ifdef G_OS_WIN32
#define _WIN32_WINNT 0x0500
#include <windows.h>
#include "giowin32-priv.h"
#endif

#ifndef O_BINARY
#define O_BINARY 0
#endif

static void initable_iface_init (GInitableIface         *initable_iface);

typedef GObjectClass GSubprocessClass;

#ifdef G_OS_UNIX
static void
g_subprocess_unix_queue_waitpid (GSubprocess  *self);
#endif

typedef struct _GSubprocessStreamDisposition GSubprocessStreamDisposition;
typedef struct _GSubprocessStreamDispositionClass GSubprocessStreamDispositionClass;
struct _GSubprocessStreamDispositionClass {
  GObjectClass parent_class;
};

struct _GSubprocess
{
  GObject parent;

  GSubprocessFlags flags;
  char **argv;
  char **envp;
  char *cwd;
  GPid pid;

  /* These are the objects that are passed in to the constructor for the
   * stdin, stdout and stderr properties (ie: GFileDescriptorBased or
   * GFile).
   */
  GObject *stdin;
  GObject *stdout;
  GObject *stderr;

  gboolean reaped_child;

  /* These are the streams created if a pipe is requested via flags. */
  GOutputStream *stdin_pipe;
  GInputStream  *stdout_pipe;
  GInputStream  *stderr_pipe;
};

G_DEFINE_TYPE_WITH_CODE (GSubprocess, g_subprocess, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (G_TYPE_INITABLE, initable_iface_init));

enum
{
  PROP_0,
  PROP_ARGV,
  PROP_WORKING_DIRECTORY,
  PROP_ENVIRONMENT,
  PROP_FLAGS,
  PROP_STDIN,
  PROP_STDOUT,
  PROP_STDERR,
  N_PROPS
};

static GParamSpec *g_subprocess_pspecs[N_PROPS];

static void
g_subprocess_init (GSubprocess  *self)
{
}

static void
g_subprocess_finalize (GObject *object)
{
  GSubprocess *self = G_SUBPROCESS (object);

#ifdef G_OS_UNIX
  /* Here we need to actually call waitpid() to clean up the
   * zombie.  In case the child hasn't actually exited, defer this
       * cleanup to the worker thread.
       */
  if (!self->reaped_child)
    g_subprocess_unix_queue_waitpid (self);
#endif
  g_spawn_close_pid (self->pid);

  g_clear_object (&self->stdin);
  g_clear_object (&self->stdout);
  g_clear_object (&self->stderr);

  g_clear_object (&self->stdin);
  g_clear_object (&self->stdout);
  g_clear_object (&self->stderr);

  g_strfreev (self->argv);
  g_strfreev (self->envp);
  g_free (self->cwd);

  if (G_OBJECT_CLASS (g_subprocess_parent_class)->finalize != NULL)
    G_OBJECT_CLASS (g_subprocess_parent_class)->finalize (object);
}

static void
g_subprocess_set_property (GObject      *object,
                           guint         prop_id,
                           const GValue *value,
                           GParamSpec   *pspec)
{
  GSubprocess *self = G_SUBPROCESS (object);

  switch (prop_id)
    {
    case PROP_ARGV:
      self->argv = g_value_dup_boxed (value);
      break;

    case PROP_WORKING_DIRECTORY:
      self->cwd = g_value_dup_string (value);
      break;

    case PROP_ENVIRONMENT:
      self->envp = g_value_dup_boxed (value);
      break;

    case PROP_FLAGS:
      self->flags = g_value_get_flags (value);
      break;

    case PROP_STDIN:
      self->stdin = g_value_dup_object (value);
      break;

    case PROP_STDOUT:
      self->stdout = g_value_dup_object (value);
      break;

    case PROP_STDERR:
      self->stderr = g_value_dup_object (value);
      break;

    default:
      g_assert_not_reached ();
    }
}

static void
g_subprocess_get_property (GObject    *object,
                           guint       prop_id,
                           GValue     *value,
                           GParamSpec *pspec)
{
  GSubprocess *self = G_SUBPROCESS (object);

  switch (prop_id)
    {
    case PROP_ARGV:
      g_value_set_boxed (value, self->argv);
      break;

    case PROP_WORKING_DIRECTORY:
      g_value_set_string (value, self->cwd);
      break;

    case PROP_ENVIRONMENT:
      g_value_set_boxed (value, self->envp);
      break;

    case PROP_FLAGS:
      g_value_set_flags (value, self->flags);
      break;

    case PROP_STDIN:
      g_value_set_object (value, self->stdin);
      break;

    case PROP_STDOUT:
      g_value_set_object (value, self->stdout);
      break;

    case PROP_STDERR:
      g_value_set_object (value, self->stderr);
      break;

    default:
      g_assert_not_reached ();
    }
}

static void
g_subprocess_class_init (GSubprocessClass *class)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (class);

  gobject_class->finalize = g_subprocess_finalize;
  gobject_class->get_property = g_subprocess_get_property;
  gobject_class->set_property = g_subprocess_set_property;

  /**
   * GSubprocess:argv:
   *
   * Arguments to for child process; if the given #GSpawnFlags do not
   * include %G_SPAWN_SEARCH_PATH has not been used, then this must be
   * a full path.
   *
   * Since: 2.34
   */
  g_subprocess_pspecs[PROP_ARGV] = g_param_spec_boxed ("argv", P_("Arguments"), P_("Argument list"), G_TYPE_STRV,
                                                       G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY |
                                                       G_PARAM_STATIC_STRINGS);

  /**
   * GSubprocess:working-directory:
   *
   * Used for child's initial current working directory. %NULL will
   * cause the child to inherit the parent's working directory; this
   * is the default.
   *
   * Since: 2.34
   */
  g_subprocess_pspecs[PROP_WORKING_DIRECTORY] = g_param_spec_string ("working-directory", P_("Working Directory"),
                                                                     P_("Path to working directory"), NULL,
                                                                     G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY |
                                                                     G_PARAM_STATIC_STRINGS);

  /**
   * GSubprocess:environment:
   *
   * Used for child's environment. %NULL will cause the child to
   * inherit the parent's environment; this is the default.
   *
   * Since: 2.34
   */
  g_subprocess_pspecs[PROP_ENVIRONMENT] = g_param_spec_boxed ("environment", P_("Environment"),
                                                              P_("Environment for child process"), G_TYPE_STRV,
                                                              G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY |
                                                              G_PARAM_STATIC_STRINGS);

  /**
   * GSubprocess:spawn-flags:
   *
   * Additional flags controlling the subprocess.  See %GSpawnFlags.
   *
   * Since: 2.34
   */
  g_subprocess_pspecs[PROP_FLAGS] = g_param_spec_flags ("flags", P_("Spawn Flags"),
                                                        P_("Additional flags conrolling the subprocess"),
                                                        G_TYPE_SUBPROCESS_FLAGS, 0, G_PARAM_READWRITE |
                                                        G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  /**
   * GSubprocess:stdin:
   *
   * Controls the direction of standard input.
   *
   * Since: 2.34
   */
  g_subprocess_pspecs[PROP_STDIN] = g_param_spec_object ("stdin", P_("Stdin"), P_("Disposition of standard input"),
                                                         G_TYPE_OBJECT, G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY |
                                                         G_PARAM_STATIC_STRINGS);

  /**
   * GSubprocess:stdout:
   *
   * Controls the direction of standard output.
   *
   * Since: 2.34
   */
  g_subprocess_pspecs[PROP_STDOUT] = g_param_spec_object ("stdout", P_("Stdout"), P_("Disposition of standard output"),
                                                          G_TYPE_OBJECT, G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY |
                                                          G_PARAM_STATIC_STRINGS);

  /**
   * GSubprocess:stderr:
   *
   * Controls the direction of standard error.
   *
   * Since: 2.34
   */
  g_subprocess_pspecs[PROP_STDERR] = g_param_spec_object ("stderr", P_("Stderr"), P_("Disposition of standard error"),
                                                          G_TYPE_OBJECT, G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY |
                                                          G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (gobject_class, N_PROPS, g_subprocess_pspecs);
}

#ifdef G_OS_UNIX

static gboolean
g_subprocess_unix_waitpid_dummy (gpointer data)
{
  return FALSE;
}

static void
g_subprocess_unix_queue_waitpid (GSubprocess  *self)
{
  GMainContext *worker_context;
  GSource *waitpid_source;

  worker_context = GLIB_PRIVATE_CALL (g_get_worker_context) ();
  waitpid_source = g_child_watch_source_new (self->pid); 
  g_source_set_callback (waitpid_source, g_subprocess_unix_waitpid_dummy, NULL, NULL);
  g_source_attach (waitpid_source, worker_context);
  g_source_unref (waitpid_source);
}

#endif

static GInputStream *
platform_input_stream_from_spawn_fd (gint fd)
{
  if (fd < 0)
    return NULL;

#ifdef G_OS_UNIX
  return g_unix_input_stream_new (fd, TRUE);
#else
  return g_win32_input_stream_new_from_fd (fd, TRUE);
#endif
}

static GOutputStream *
platform_output_stream_from_spawn_fd (gint fd)
{
  if (fd < 0)
    return NULL;

#ifdef G_OS_UNIX
  return g_unix_output_stream_new (fd, TRUE);
#else
  return g_win32_output_stream_new_from_fd (fd, TRUE);
#endif
}

#ifdef G_OS_UNIX
static gint
unix_open_file (GFile   *file,
                gint     mode,
                GError **error)
{
  gchar *filename;
  gint my_fd;

  filename = g_file_get_path (file);

  if (filename == NULL)
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED, _("File does not have a local path"));
      return -1;
    }

  my_fd = g_open (filename, mode | O_BINARY | O_CLOEXEC, 0666);

  /* If we return -1 we should also set the error */
  if (my_fd < 0)
    {
      gint saved_errno = errno;
      char *display_name;

      display_name = g_filename_display_name (filename);
      g_set_error (error, G_IO_ERROR, g_io_error_from_errno (saved_errno),
                   _("Error opening file '%s': %s"), display_name,
                   g_strerror (saved_errno));
      g_free (display_name);
      /* fall through... */
    }

  g_free (filename);

  return my_fd;
}

static gboolean
unix_get_fd_for_object_read (GObject   *object,
                             gint      *fd,
                             gint      *close_fd,
                             GError   **error)
{
  gint my_fd;

  if (G_IS_FILE_DESCRIPTOR_BASED (object))
    {
      *fd = g_file_descriptor_based_get_fd (G_FILE_DESCRIPTOR_BASED (object));
      return TRUE;
    }

  g_assert (G_IS_FILE (object));

  my_fd = unix_open_file (G_FILE (object), O_RDONLY, error);

  if (my_fd < 0)
    return FALSE;

  *fd = *close_fd = my_fd;

  return TRUE;
}

static gboolean
unix_get_fd_for_object_write (GObject   *object,
                              gint      *fd,
                              gint      *close_fd,
                              GError   **error)
{
  gint my_fd;

  if (G_IS_FILE_DESCRIPTOR_BASED (object))
    {
      *fd = g_file_descriptor_based_get_fd (G_FILE_DESCRIPTOR_BASED (object));
      return TRUE;
    }

  g_assert (G_IS_FILE (object));

  my_fd = unix_open_file (G_FILE (object), O_CREAT | O_WRONLY, error);

  if (my_fd < 0)
    return FALSE;

  *fd = *close_fd = my_fd;

  return TRUE;
}
#endif

typedef struct
{
  gint             fds[3];
  GSubprocessFlags flags;
} ChildData;

static void
child_setup (gpointer user_data)
{
  ChildData *child_data = user_data;
  gint i;

  /* We're on the child side now.  "Rename" the file descriptors in
   * child_data.fds[] to stdin/stdout/stderr.
   *
   * We don't close the originals.  It's possible that the originals
   * should not be closed and if they should be closed then they should
   * have been created O_CLOEXEC.
   */
  for (i = 0; i < 3; i++)
    if (child_data->fds[i] != -1 && child_data->fds[i] != i)
      {
        gint result;

        do
          result = dup2 (child_data->fds[i], i);
        while (result == -1 && errno == EINTR);
      }

  if (child_data->flags & G_SUBPROCESS_FLAGS_TERM_WITH_PARENT)
    /* XXX linux thing... */;

#ifdef G_OS_UNIX
  if (child_data->flags & G_SUBPROCESS_FLAGS_NEW_SESSION)
    setsid ();
#endif
}

static gboolean
initable_init (GInitable     *initable,
               GCancellable  *cancellable,
               GError       **error)
{
  GSubprocess *self = G_SUBPROCESS (initable);
  ChildData child_data = { { -1, -1, -1 } };
  gint *pipe_ptrs[3] = { NULL, NULL, NULL };
  gint pipe_fds[3] = { -1, -1, -1 };
  gint close_fds[3] = { -1, -1, -1 };
  GSpawnFlags spawn_flags = 0;
  gboolean success = FALSE;
  gint i;

  g_return_val_if_fail (self->argv && self->argv[0], FALSE);
  g_return_val_if_fail (!!(self->stdin) +
                        !!(self->flags & G_SUBPROCESS_FLAGS_STDIN_INHERIT) +
                        !!(self->flags & G_SUBPROCESS_FLAGS_STDIN_PIPE) < 2, FALSE);
  g_return_val_if_fail (!!(self->stdout) +
                        !!(self->flags & G_SUBPROCESS_FLAGS_STDOUT_SILENCE) +
                        !!(self->flags & G_SUBPROCESS_FLAGS_STDOUT_PIPE) < 2, FALSE);
  g_return_val_if_fail (!!(self->stderr) +
                        !!(self->flags & G_SUBPROCESS_FLAGS_STDERR_SILENCE) +
                        !!(self->flags & G_SUBPROCESS_FLAGS_STDERR_PIPE) +
                        !!(self->flags & G_SUBPROCESS_FLAGS_STDERR_MERGE) < 2, FALSE);
#ifdef G_OS_UNIX
  g_return_val_if_fail (!self->stdin || G_IS_FILE (self->stdin) || G_IS_FILE_DESCRIPTOR_BASED (self->stdin), FALSE);
  g_return_val_if_fail (!self->stdout || G_IS_FILE (self->stdout) || G_IS_FILE_DESCRIPTOR_BASED (self->stdout), FALSE);
  g_return_val_if_fail (!self->stderr || G_IS_FILE (self->stderr) || G_IS_FILE_DESCRIPTOR_BASED (self->stderr), FALSE);
#else
  g_return_val_if_fail (self->stdin == NULL, FALSE);
  g_return_val_if_fail (self->stdout == NULL, FALSE);
  g_return_val_if_fail (self->stderr == NULL, FALSE);
#endif

  if (g_cancellable_set_error_if_cancelled (cancellable, error))
    return FALSE;

  /* We must setup the three fds that will end up in the child as stdin,
   * stdout and stderr.
   *
   * First, stdin.
   */
  if (self->flags & G_SUBPROCESS_FLAGS_STDIN_INHERIT)
    spawn_flags |= G_SPAWN_CHILD_INHERITS_STDIN;
  else if (self->flags & G_SUBPROCESS_FLAGS_STDIN_PIPE)
    pipe_ptrs[0] = &pipe_fds[0];
#ifdef G_OS_UNIX
  else if (self->stdin)
    if (!unix_get_fd_for_object_read (self->stdin, &child_data.fds[0], &close_fds[0], error))
      goto out;
#endif

  /* Next, stdout. */
  if (self->flags & G_SUBPROCESS_FLAGS_STDOUT_SILENCE)
    spawn_flags |= G_SPAWN_STDOUT_TO_DEV_NULL;
  else if (self->flags & G_SUBPROCESS_FLAGS_STDOUT_PIPE)
    pipe_ptrs[1] = &pipe_fds[1];
#ifdef G_OS_UNIX
  else if (self->stdout)
    if (!unix_get_fd_for_object_write (self->stdout, &child_data.fds[1], &close_fds[1], error))
      goto out;
#endif

  /* Finally, stderr. */
  if (self->flags & G_SUBPROCESS_FLAGS_STDERR_SILENCE)
    spawn_flags |= G_SPAWN_STDERR_TO_DEV_NULL;
  else if (self->flags & G_SUBPROCESS_FLAGS_STDERR_PIPE)
    pipe_ptrs[2] = &pipe_fds[2];
  else if (self->flags & G_SUBPROCESS_FLAGS_STDERR_MERGE)
    /* This will work because stderr gets setup after stdout. */
    child_data.fds[2] = 1;
#ifdef G_OS_UNIX
  else if (self->stderr)
    if (!unix_get_fd_for_object_write (self->stderr, &child_data.fds[2], &close_fds[2], error))
      goto out;
#endif

  spawn_flags |= G_SPAWN_LEAVE_DESCRIPTORS_OPEN;

  if (self->flags & G_SUBPROCESS_FLAGS_SEARCH_PATH)
    spawn_flags |= G_SPAWN_SEARCH_PATH;

  spawn_flags |= G_SPAWN_DO_NOT_REAP_CHILD;
  spawn_flags |= G_SPAWN_CLOEXEC_PIPES;

  child_data.flags = self->flags;
  success = g_spawn_async_with_pipes (self->cwd, self->argv, self->envp,
                                      spawn_flags,
                                      child_setup, &child_data,
                                      &self->pid,
                                      pipe_ptrs[0], pipe_ptrs[1], pipe_ptrs[2],
                                      error);

out:
  for (i = 0; i < 3; i++)
    if (close_fds[i] != -1)
      close (close_fds[i]);

  self->stdin_pipe = platform_output_stream_from_spawn_fd (pipe_fds[0]);
  self->stdout_pipe = platform_input_stream_from_spawn_fd (pipe_fds[1]);
  self->stderr_pipe = platform_input_stream_from_spawn_fd (pipe_fds[2]);

  return success;
}

static void
initable_iface_init (GInitableIface *initable_iface)
{
  initable_iface->init = initable_init;
}

/**
 * g_subprocess_new:
 *
 * Create a new process with the given parameters.
 *
 * Returns: (transfer full): A newly created %GSubprocess, or %NULL on error (and @error will be set)
 *
 * Since: 2.34
 */
GLIB_AVAILABLE_IN_2_34
GSubprocess *
g_subprocess_new (const gchar          *cwd,
                  const gchar * const  *argv,
                  const gchar * const  *env,
                  GSubprocessFlags      flags,
                  GError              **error)
{
  return g_initable_new (G_TYPE_SUBPROCESS,
                         NULL, error,
                         "argv", argv,
                         "working-directory", cwd,
                         "environment", env,
                         "flags", flags,
                         NULL);
}

/**
 * g_subprocess_get_pid:
 * @self: a #GSubprocess
 *
 * The identifier for this child process; it is valid as long as the
 * process @self is referenced.  In particular, do
 * <emphasis>not</emphasis> call g_spawn_close_pid() on this value;
 * that is handled internally.
 *
 * On some Unix versions, it is possible for there to be a race
 * condition where waitpid() may have been called to collect the child
 * before any watches (such as that installed by
 * g_subprocess_add_watch()) have fired.  If you are planning to use
 * native functions such as kill() on the pid, your program should
 * gracefully handle an %ESRCH result to mitigate this.
 *
 * If you want to request process termination, using the high level
 * g_subprocess_request_exit() and g_subprocess_force_exit() API is
 * recommended.
 *
 * Returns: Operating-system specific identifier for child process
 *
 * Since: 2.34
 */
GPid
g_subprocess_get_pid (GSubprocess     *self)
{
  g_return_val_if_fail (G_IS_SUBPROCESS (self), 0);

  return self->pid;
}

GOutputStream *
g_subprocess_get_stdin_pipe (GSubprocess       *self)
{
  g_return_val_if_fail (G_IS_SUBPROCESS (self), NULL);
  g_return_val_if_fail (self->stdin_pipe, NULL);

  return self->stdin_pipe;
}

GInputStream *
g_subprocess_get_stdout_pipe (GSubprocess      *self)
{
  g_return_val_if_fail (G_IS_SUBPROCESS (self), NULL);
  g_return_val_if_fail (self->stdout_pipe, NULL);

  return self->stdout_pipe;
}

GInputStream *
g_subprocess_get_stderr_pipe (GSubprocess      *self)
{
  g_return_val_if_fail (G_IS_SUBPROCESS (self), NULL);
  g_return_val_if_fail (self->stderr_pipe, NULL);

  return self->stderr_pipe;
}

typedef struct {
  GSubprocess *self;
  gboolean have_wnowait;
  GCancellable *cancellable;
  GSimpleAsyncResult *result;
} GSubprocessWatchData;

static gboolean
g_subprocess_on_child_exited (GPid       pid,
			      gint       status_code,
			      gpointer   user_data)
{
  GSubprocessWatchData *data = user_data;
  GError *error = NULL;
  
  if (g_cancellable_set_error_if_cancelled (data->cancellable, &error))
    {
      g_simple_async_result_take_error (data->result, error);
    }
  else
    {
      if (!data->have_wnowait)
	data->self->reaped_child = TRUE;

      g_simple_async_result_set_op_res_gssize (data->result, status_code);
    }

  g_simple_async_result_complete (data->result);

  g_object_unref (data->result);
  g_object_unref (data->self);
  g_free (data);

  return FALSE;
}

/**
 * g_subprocess_wait:
 * @self: a #GSubprocess
 * @cancellable: a #GCancellable
 * @callback: Invoked when process exits, or @cancellable is cancelled
 * @user_data: Data for @callback
 *
 * Start an asynchronous wait for the subprocess @self to exit.
 *
 * Since: 2.34
 */
void
g_subprocess_wait (GSubprocess                *self,
		   GCancellable               *cancellable,
		   GAsyncReadyCallback         callback,
		   gpointer                    user_data)
{
  GSource *source;
  GSubprocessWatchData *data;

  data = g_new0 (GSubprocessWatchData, 1);

  data->self = g_object_ref (self);
  data->result = g_simple_async_result_new ((GObject*)self, callback, user_data,
					    g_subprocess_wait);

  source = GLIB_PRIVATE_CALL (g_child_watch_source_new_with_flags) (self->pid, _G_CHILD_WATCH_FLAGS_WNOWAIT);
  if (source == NULL)
    {
      source = g_child_watch_source_new (self->pid);
      data->have_wnowait = FALSE;
    }
  else
    {
      data->have_wnowait = TRUE;
    }

  g_source_set_callback (source, (GSourceFunc)g_subprocess_on_child_exited,
			 data, NULL);
  if (cancellable)
    {
      GSource *cancellable_source;

      data->cancellable = g_object_ref (cancellable);

      cancellable_source = g_cancellable_source_new (cancellable);
      g_source_add_child_source (source, cancellable_source);
      g_source_unref (cancellable_source);
    }

  g_source_attach (source, g_main_context_get_thread_default ());
  g_source_unref (source);
}

/**
 * g_subprocess_wait_finish:
 * @self: a #GSubprocess
 * @result: a #GAsyncResult
 * @out_exit_status: (out): Exit status of the process encoded in platform-specific way
 * @error: a #GError
 *
 * The exit status of the process will be stored in @out_exit_status.
 * See the documentation of g_spawn_check_exit_status() for more
 * details.
 *
 * Note that @error is not set if the process exits abnormally; you
 * must use g_spawn_check_exit_status() for that.
 *
 * Since: 2.34
 */
gboolean
g_subprocess_wait_finish (GSubprocess                *self,
			  GAsyncResult               *result,
			  int                        *out_exit_status,
			  GError                    **error)
{
  GSimpleAsyncResult *simple;

  simple = G_SIMPLE_ASYNC_RESULT (result);

  if (g_simple_async_result_propagate_error (simple, error))
    return FALSE;

  *out_exit_status = g_simple_async_result_get_op_res_gssize (simple);
  
  return TRUE;
}

typedef struct {
  GMainLoop *loop;
  gint *exit_status_ptr;
  gboolean caught_error;
  GError **error;
} GSubprocessSyncWaitData;

static void
g_subprocess_on_sync_wait_complete (GObject       *object,
				    GAsyncResult  *result,
				    gpointer       user_data)
{
  GSubprocessSyncWaitData *data = user_data;

  if (!g_subprocess_wait_finish ((GSubprocess*)object, result, 
				 data->exit_status_ptr, data->error))
    data->caught_error = TRUE;

  g_main_loop_quit (data->loop);
}

/**
 * g_subprocess_wait_sync:
 * @self: a #GSubprocess
 * @out_exit_status: (out): Platform-specific exit code
 * @cancellable: a #GCancellable
 * @error: a #GError
 *
 * Synchronously wait for the subprocess to terminate, returning the
 * status code in @out_exit_status.  See the documentation of
 * g_spawn_check_exit_status() for how to interpret it.  Note that if
 * @error is set, then @out_exit_status will be left uninitialized.
 * 
 * Returns: %TRUE on success, %FALSE if @cancellable was cancelled
 *
 * Since: 2.34
 */
gboolean
g_subprocess_wait_sync (GSubprocess        *self,
			int                *out_exit_status,
			GCancellable       *cancellable,
			GError            **error)
{
  gboolean ret = FALSE;
  gboolean pushed_thread_default = FALSE;
  GMainContext *context = NULL;
  GSubprocessSyncWaitData data;

  memset (&data, 0, sizeof (data));

  g_return_val_if_fail (G_IS_SUBPROCESS (self), FALSE);

  if (g_cancellable_set_error_if_cancelled (cancellable, error))
    return FALSE;

  context = g_main_context_new ();
  g_main_context_push_thread_default (context);
  pushed_thread_default = TRUE;

  data.exit_status_ptr = out_exit_status;
  data.loop = g_main_loop_new (context, TRUE);
  data.error = error;

  g_subprocess_wait (self, cancellable,
		     g_subprocess_on_sync_wait_complete, &data);

  g_main_loop_run (data.loop);

  if (data.caught_error)
    goto out;

  ret = TRUE;
 out:
  if (pushed_thread_default)
    g_main_context_pop_thread_default (context);
  if (context)
    g_main_context_unref (context);
  if (data.loop)
    g_main_loop_unref (data.loop);

  return ret;
}

/**
 * g_subprocess_wait_sync_check:
 * @self: a #GSubprocess
 * @cancellable: a #GCancellable
 * @error: a #GError
 *
 * Combines g_subprocess_wait_sync() with g_spawn_check_exit_status().
 * 
 * Returns: %TRUE on success, %FALSE if process exited abnormally, or @cancellable was cancelled
 *
 * Since: 2.34
 */
gboolean
g_subprocess_wait_sync_check (GSubprocess        *self,
			      GCancellable       *cancellable,
			      GError            **error)
{
  gboolean ret = FALSE;
  int exit_status;

  if (!g_subprocess_wait_sync (self, &exit_status, cancellable, error))
    goto out;

  if (!g_spawn_check_exit_status (exit_status, error))
    goto out;

  ret = TRUE;
 out:
  return ret;
}

/**
 * g_subprocess_request_exit:
 * @self: a #GSubprocess
 *
 * This API uses an operating-system specific mechanism to request
 * that the subprocess gracefully exit.  This API is not available on
 * all operating systems; for those not supported, it will do nothing
 * and return %FALSE.  Portable code should handle this situation
 * gracefully.  For example, if you are communicating via input or
 * output pipe with the child, many programs will automatically exit
 * when one of their standard input or output are closed.
 *
 * On Unix, this API sends %SIGTERM.
 *
 * A %TRUE return value does <emphasis>not</emphasis> mean the
 * subprocess has exited, merely that an exit request was initiated.
 * You can use g_subprocess_add_watch() to monitor the status of the
 * process after calling this function.
 *
 * This function returns %TRUE if the process has already exited.
 *
 * Returns: %TRUE if the operation is supported, %FALSE otherwise.
 *
 * Since: 2.34
 */
gboolean
g_subprocess_request_exit (GSubprocess *self)
{
  g_return_val_if_fail (G_IS_SUBPROCESS (self), FALSE);

#ifdef G_OS_UNIX
  (void) kill (self->pid, SIGTERM);
  return TRUE;
#else
  return FALSE;
#endif
}

/**
 * g_subprocess_force_exit:
 * @self: a #GSubprocess
 *
 * Use an operating-system specific method to attempt an immediate,
 * forceful termination of the process.  There is no mechanism to
 * determine whether or not the request itself was successful;
 * however, you can use g_subprocess_wait() to monitor the status of
 * the process after calling this function.
 *
 * On Unix, this function sends %SIGKILL.
 */
void
g_subprocess_force_exit (GSubprocess *self)
{
  g_return_if_fail (G_IS_SUBPROCESS (self));

#ifdef G_OS_UNIX
  (void) kill (self->pid, SIGKILL);
#else
  TerminateProcess (self->pid, 1);
#endif
}
