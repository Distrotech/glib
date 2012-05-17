/* GIO - GLib Input, Output and Streaming Library
 *
 * Copyright © 2012 Red Hat, Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation; either version 2 of the licence or (at
 * your option) any later version.
 *
 * See the included COPYING file for more information.
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

typedef enum {
  G_SUBPROCESS_STREAM_DISPOSITION_DEVNULL = 0,
  G_SUBPROCESS_STREAM_DISPOSITION_INHERIT = 1,
  G_SUBPROCESS_STREAM_DISPOSITION_PIPE = 2,
  G_SUBPROCESS_STREAM_DISPOSITION_MERGE_STDOUT
#define G_SUBPROCESS_STREAM_DISPOSITION_LAST (G_SUBPROCESS_STREAM_DISPOSITION_MERGE_STDOUT)
} GSubprocessStreamDispositionKind;

static GObject * get_disposition (GSubprocessStreamDispositionKind kind);

static void initable_iface_init (GInitableIface         *initable_iface);

typedef struct _GSubprocessClass GSubprocessClass;

#ifdef G_OS_UNIX
static void
g_subprocess_unix_queue_waitpid (GSubprocess  *self);
#endif

typedef struct _GSubprocessStreamDisposition GSubprocessStreamDisposition;
typedef struct _GSubprocessStreamDispositionClass GSubprocessStreamDispositionClass;
struct _GSubprocessStreamDispositionClass {
  GObjectClass parent_class;
};

struct _GSubprocessClass {
  GObjectClass parent_class;
};

struct _GSubprocess
{
  GObject parent;

  char **argv;
  GSpawnFlags spawn_flags;
  char **envp;
  char *cwd;
  GSpawnChildSetupFunc child_setup;
  gpointer child_setup_user_data;
  GObject *stdin_disposition;
  GObject *stdout_disposition;
  GObject *stderr_disposition;

  GPid pid;
  guint reaped_child : 1;
  guint merge_stderr_to_stdout : 1;
  guint reserved : 30;

  int internal_stdin_fd;
  int internal_stdout_fd;
  int internal_stderr_fd;

  GOutputStream *stdin_stream;
  GInputStream *stdout_stream;
  GInputStream *stderr_stream;
};

struct _GSubprocessStreamDisposition
{
  GObject parent;
  GSubprocessStreamDispositionKind kind;
};

G_DEFINE_TYPE_WITH_CODE (GSubprocess, g_subprocess, G_TYPE_OBJECT,
			 G_IMPLEMENT_INTERFACE (G_TYPE_INITABLE, initable_iface_init));

G_DEFINE_TYPE (GSubprocessStreamDisposition, g_subprocess_stream_disposition, G_TYPE_OBJECT);

enum
{
  PROP_0,
  PROP_ARGV,
  PROP_WORKING_DIRECTORY,
  PROP_ENVIRONMENT,
  PROP_SPAWN_FLAGS,
  PROP_CHILD_SETUP_FUNC,
  PROP_CHILD_SETUP_USER_DATA,
  PROP_STDIN_DISPOSITION,
  PROP_STDOUT_DISPOSITION,
  PROP_STDERR_DISPOSITION
};

static void
g_subprocess_init (GSubprocess  *self)
{
  self->stdin_disposition = get_disposition (G_SUBPROCESS_STREAM_DISPOSITION_DEVNULL);
  self->stdout_disposition = get_disposition (G_SUBPROCESS_STREAM_DISPOSITION_INHERIT);
  self->stderr_disposition = get_disposition (G_SUBPROCESS_STREAM_DISPOSITION_INHERIT);
  self->internal_stdin_fd = -1;
  self->internal_stdout_fd = -1;
  self->internal_stderr_fd = -1;
}

static void
g_subprocess_stream_disposition_init (GSubprocessStreamDisposition *self)
{
  self->kind = G_SUBPROCESS_STREAM_DISPOSITION_INHERIT;
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

  g_clear_object (&self->stdin_disposition);
  g_clear_object (&self->stdout_disposition);
  g_clear_object (&self->stderr_disposition);

  g_clear_object (&self->stdin_stream);
  g_clear_object (&self->stdout_stream);
  g_clear_object (&self->stderr_stream);

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

    case PROP_SPAWN_FLAGS:
      self->spawn_flags = g_value_get_flags (value);
      break;

    case PROP_CHILD_SETUP_FUNC:
      self->child_setup = g_value_get_pointer (value);
      break;

    case PROP_CHILD_SETUP_USER_DATA:
      self->child_setup_user_data = g_value_get_pointer (value);
      break;

    case PROP_STDIN_DISPOSITION:
      self->stdin_disposition = g_value_dup_object (value);
      break;

    case PROP_STDOUT_DISPOSITION:
      self->stdout_disposition = g_value_dup_object (value);
      break;

    case PROP_STDERR_DISPOSITION:
      self->stderr_disposition = g_value_dup_object (value);
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
      g_value_set_boxed (value, (gpointer)self->argv);
      break;

    case PROP_WORKING_DIRECTORY:
      g_value_set_string (value, self->cwd);
      break;

    case PROP_ENVIRONMENT:
      g_value_set_boxed (value, (gpointer)self->envp);
      break;

    case PROP_SPAWN_FLAGS:
      g_value_set_flags (value, self->spawn_flags);
      break;

    case PROP_CHILD_SETUP_FUNC:
      g_value_set_pointer (value, self->child_setup);
      break;

    case PROP_CHILD_SETUP_USER_DATA:
      g_value_set_pointer (value, self->child_setup_user_data);
      break;

    case PROP_STDIN_DISPOSITION:
      g_value_set_object (value, self->stdin_disposition);
      break;

    case PROP_STDOUT_DISPOSITION:
      g_value_set_object (value, self->stdout_disposition);
      break;

    case PROP_STDERR_DISPOSITION:
      g_value_set_object (value, self->stderr_disposition);
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
  g_object_class_install_property (gobject_class, PROP_ARGV,
				   g_param_spec_boxed ("argv",
						       P_("Arguments"),
						       P_("Argument list"),
						       G_TYPE_STRV,
						       G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS));

  /**
   * GSubprocess:working-directory:
   *
   * Used for child's initial current working directory. %NULL will
   * cause the child to inherit the parent's working directory; this
   * is the default.
   *
   * Since: 2.34
   */
  g_object_class_install_property (gobject_class, PROP_WORKING_DIRECTORY,
				   g_param_spec_string ("working-directory",
							P_("Working Directory"),
							P_("Path to working directory"),
							NULL,
							G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS));

  /**
   * GSubprocess:environment:
   *
   * Used for child's environment. %NULL will cause the child to
   * inherit the parent's environment; this is the default.
   *
   * Since: 2.34
   */
  g_object_class_install_property (gobject_class, PROP_ENVIRONMENT,
				   g_param_spec_boxed ("environment",
						       P_("Environment"),
						       P_("Environment for child process"),
						       G_TYPE_STRV,
						       G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS));

  /**
   * GSubprocess:spawn-flags:
   *
   * Additional flags controlling the subprocess.  See %GSpawnFlags.
   *
   * Since: 2.34
   */
  g_object_class_install_property (gobject_class, PROP_SPAWN_FLAGS,
				   g_param_spec_flags ("spawn-flags",
						       P_("Spawn Flags"),
						       P_("Additional flags conrolling the subprocess"),
						       G_TYPE_SPAWN_FLAGS, 0,
						       G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS));


  /**
   * GSubprocess:child-setup-func:
   *
   * Only available on Unix; this function will be invoked between
   * <literal>fork()</literal> and <literal>exec()</literal>.  See the
   * documentation of g_spawn_async_with_pipes() for more details.
   *
   * Since: 2.34
   */
  g_object_class_install_property (gobject_class, PROP_CHILD_SETUP_FUNC,
				   g_param_spec_pointer ("child-setup-func",
							 P_("Setup"),
							 P_("Child setup function"),
							 G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS));

  /**
   * GSubprocess:child-setup-user-data:
   *
   * User data passed to the %GSubprocess:child-setup-func.
   *
   * Since: 2.34
   */
  g_object_class_install_property (gobject_class, PROP_CHILD_SETUP_USER_DATA,
				   g_param_spec_pointer ("child-setup-user-data",
							 P_("Data"),
							 P_("Child setup user data"),
							 G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS));

  /**
   * GSubprocess:stdin-disposition:
   *
   * Controls the direction of standard input.
   *
   * Since: 2.34
   */
  g_object_class_install_property (gobject_class, PROP_STDIN_DISPOSITION,
				   g_param_spec_object ("stdin-disposition",
							P_("Stdin"),
							P_("Disposition of standard input"),
							G_TYPE_OBJECT,
							G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS));

  /**
   * GSubprocess:stdout-disposition:
   *
   * Controls the direction of standard output.
   *
   * Since: 2.34
   */
  g_object_class_install_property (gobject_class, PROP_STDOUT_DISPOSITION,
				   g_param_spec_object ("stdout-disposition",
							P_("Stdout"),
							P_("Disposition of standard output"),
							G_TYPE_OBJECT,
							G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS));

  /**
   * GSubprocess:stderr-disposition:
   *
   * Controls the direction of standard error.
   *
   * Since: 2.34
   */
  g_object_class_install_property (gobject_class, PROP_STDERR_DISPOSITION,
				   g_param_spec_object ("stderr-disposition",
							P_("Stderr"),
							P_("Disposition of standard error"),
							G_TYPE_OBJECT,
							G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS));

}

static void
g_subprocess_stream_disposition_class_init (GSubprocessStreamDispositionClass  *klass)
{
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

static void
safe_dup2 (gint   a,
	   gint   b)
{
  gint ecode;

  if (a == b)
    return;

  do
    ecode = dup2 (a, b);
  while (ecode == -1 && errno == EINTR);
}

static void
g_subprocess_internal_child_setup (gpointer        user_data)
{
  GSubprocess *self = user_data;

  if (self->internal_stdin_fd >= 0)
    safe_dup2 (self->internal_stdin_fd, 0);

  if (self->internal_stdout_fd >= 0)
    safe_dup2 (self->internal_stdout_fd, 1);

  if (self->internal_stderr_fd >= 0)
    safe_dup2 (self->internal_stderr_fd, 2);

  if (self->merge_stderr_to_stdout)
    safe_dup2 (1, 2);

  if (self->child_setup)
    self->child_setup (self->child_setup_user_data);
}

#endif

static GInputStream *
platform_input_stream_from_spawn_fd (gint         fd)
{
#ifdef G_OS_UNIX
  return g_unix_input_stream_new (fd, TRUE);
#else
  return g_win32_input_stream_new_from_fd (fd, TRUE);
#endif  
}

static GOutputStream *
platform_output_stream_from_spawn_fd (gint         fd)
{
#ifdef G_OS_UNIX
  return g_unix_output_stream_new (fd, TRUE);
#else
  return g_win32_output_stream_new_from_fd (fd, TRUE);
#endif  
}

static void
set_open_error (const char *filename,
		GError    **error)
{
  int errsv = errno;
  char *display_name = g_filename_display_name (filename);
  g_set_error (error, G_IO_ERROR,
	       g_io_error_from_errno (errsv),
	       _("Error opening file '%s': %s"),
	       display_name, g_strerror (errsv));
  g_free (display_name);

}

static int
open_file_write_append (const char *filename,
			GError    **error)
{
  int fd;

  fd = g_open (filename, O_WRONLY | O_CREAT | O_BINARY, 0666);
  if (fd == -1)
    {
      set_open_error (filename, error);
      return -1;
    }
  return fd;
}

static gboolean
get_file_path_or_fail (GFile   *f,
		       char   **out_path,
		       GError **error)
{
  *out_path = g_file_get_path (f);
  if (!*out_path)
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED,
		   "File does not have a local path");
      return FALSE;
    }
  return TRUE;
}

static void
set_unsupported_stream_error (GObject  *object,
			      GError  **error)
{
  g_set_error (error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED,
	       "Unsupported stream type %s", g_type_name (G_OBJECT_TYPE (object)));
}

static gboolean
unix_handle_open_fd_for_write (GObject   *disposition,
			       gboolean  *out_supported,
			       gint      *out_fd,
			       gboolean  *out_close_fd,
			       GError   **error)
{
  gboolean ret = FALSE;
  char *temp_path = NULL;

#ifdef G_OS_UNIX
  if (G_IS_FILE_DESCRIPTOR_BASED (disposition))
    {
      *out_fd = g_file_descriptor_based_get_fd ((GFileDescriptorBased*) disposition);
      *out_close_fd = FALSE;
      *out_supported = TRUE;
    }
  else if (G_IS_FILE (disposition))
    {
      if (!get_file_path_or_fail ((GFile*)disposition, &temp_path, error))
	goto out;
	
      *out_fd = open_file_write_append (temp_path, error);
      if (*out_fd == -1)
	goto out;
      *out_close_fd = TRUE;
      *out_supported = TRUE;
    }
  else
    *out_supported = FALSE;

  ret = TRUE;
 out:
  g_clear_pointer (&temp_path, g_free);
  return ret;
#else
  *out_supported = FALSE;
  return TRUE;
#endif
}

static gboolean
initable_init (GInitable     *initable,
	       GCancellable  *cancellable,
	       GError       **error)
{
  GSubprocess *self = G_SUBPROCESS (initable);
  gboolean ret = FALSE;
  char *temp_path = NULL;
  gint stdin_pipe_fd = -1;
  gint *stdin_arg = NULL;
  gint stdout_pipe_fd = -1;
  gint *stdout_arg = NULL;
  gint stderr_pipe_fd = -1;
  gint *stderr_arg = NULL;
  gboolean disposition_supported;
  GSpawnChildSetupFunc child_setup;
  gpointer child_setup_user_data;
  gboolean close_stdin_fd = FALSE;
  gboolean close_stdout_fd = FALSE;
  gboolean close_stderr_fd = FALSE;

  g_return_val_if_fail (self->argv && self->argv[0], FALSE);

  if (self->stdin_disposition == NULL)
    self->stdin_disposition = g_object_ref (g_subprocess_stream_devnull ());
  if (self->stdout_disposition == NULL)
    self->stdout_disposition = g_object_ref (g_subprocess_stream_inherit ());
  if (self->stderr_disposition == NULL)
    self->stderr_disposition = g_object_ref (g_subprocess_stream_inherit ());

  if (self->spawn_flags & G_SPAWN_CHILD_INHERITS_STDIN)
    {
      g_clear_object (&self->stdin_disposition);
      self->stdin_disposition = g_subprocess_stream_inherit ();
    }
  if (self->spawn_flags & G_SPAWN_STDOUT_TO_DEV_NULL)
    {
      g_clear_object (&self->stdout_disposition);
      self->stdout_disposition = g_subprocess_stream_devnull ();
    }
  if (self->spawn_flags & G_SPAWN_STDERR_TO_DEV_NULL)
    {
      g_clear_object (&self->stderr_disposition);
      self->stderr_disposition = g_subprocess_stream_devnull ();
    }

  /* We always use this one */
  self->spawn_flags |= G_SPAWN_DO_NOT_REAP_CHILD;

#ifdef G_OS_UNIX
  child_setup = g_subprocess_internal_child_setup;
  child_setup_user_data = self;
#else
  child_setup = NULL;
  child_setup_user_data = NULL;
#endif

#ifdef G_OS_UNIX
  if (G_IS_FILE_DESCRIPTOR_BASED (self->stdin_disposition))
    {
      self->internal_stdin_fd = g_file_descriptor_based_get_fd ((GFileDescriptorBased*) self->stdin_disposition);
    }
  else if (G_IS_FILE (self->stdin_disposition))
    {
      if (!get_file_path_or_fail ((GFile*)self->stdin_disposition, &temp_path, error))
	goto out;
	
      self->internal_stdin_fd = g_open (temp_path, O_RDONLY | O_BINARY, 0);
      if (self->internal_stdin_fd == -1)
	{
	  set_open_error (temp_path, error);
	  goto out;
	}
      close_stdin_fd = TRUE;

      g_clear_pointer (&temp_path, g_free);
    }
  else
#endif
  if (self->stdin_disposition == g_subprocess_stream_devnull ())
    {
      /* Default */
    }
  else if (self->stdin_disposition == g_subprocess_stream_inherit ())
    {
      self->spawn_flags |= G_SPAWN_CHILD_INHERITS_STDIN;
    }
  else if (self->stdin_disposition == g_subprocess_stream_pipe ())
    {
      stdin_arg = &stdin_pipe_fd;
    }
  else
    {
      set_unsupported_stream_error (self->stdin_disposition, error);
      goto out;
    }

  /* Handle stdout */

  if (!unix_handle_open_fd_for_write (self->stdout_disposition,
				      &disposition_supported,
				      &self->internal_stdout_fd,
				      &close_stdout_fd,
				      error))
    goto out;
  else if (!disposition_supported)
    {
      if (self->stdout_disposition == g_subprocess_stream_devnull ())
	{
	  self->spawn_flags |= G_SPAWN_STDOUT_TO_DEV_NULL;
	}
      else if (self->stdout_disposition == g_subprocess_stream_inherit ())
	{
	  /* Default */
	}
      else if (self->stdout_disposition == g_subprocess_stream_pipe ())
	{
	  stdout_arg = &stdout_pipe_fd;
	}
      else
	{
	  set_unsupported_stream_error (self->stdout_disposition, error);
	  goto out;
	}
    }

  /* Handle stderr */

  if (!unix_handle_open_fd_for_write (self->stderr_disposition,
				      &disposition_supported,
				      &self->internal_stderr_fd,
				      &close_stderr_fd,
				      error))
    goto out;
  else if (!disposition_supported)
    {
      if (self->stderr_disposition == g_subprocess_stream_devnull ())
	{
	  self->spawn_flags |= G_SPAWN_STDERR_TO_DEV_NULL;
	}
      else if (self->stderr_disposition == g_subprocess_stream_inherit ())
	{
	  /* Default */
	}
      else if (self->stderr_disposition == g_subprocess_stream_pipe ())
	{
	  stderr_arg = &stderr_pipe_fd;
	}
      else if (self->stderr_disposition == g_subprocess_stream_merge_stdout ())
	{
	  self->merge_stderr_to_stdout = TRUE;
	}
      else
	{
	  set_unsupported_stream_error (self->stderr_disposition, error);
	  goto out;
	}
    }

  if (!g_spawn_async_with_pipes (self->cwd,
				 self->argv,
				 self->envp,
				 self->spawn_flags,
				 child_setup, child_setup_user_data,
				 &self->pid,
				 stdin_arg, stdout_arg, stderr_arg,
				 error))
    goto out;

  ret = TRUE;

  if (stdin_pipe_fd != -1)
    self->stdin_stream = platform_output_stream_from_spawn_fd (stdin_pipe_fd);
  if (stdout_pipe_fd != -1)
    self->stdout_stream = platform_input_stream_from_spawn_fd (stdout_pipe_fd);
  if (stderr_pipe_fd != -1)
    self->stderr_stream = platform_input_stream_from_spawn_fd (stderr_pipe_fd);

 out:
  g_free (temp_path);
#ifdef G_OS_UNIX
  if (close_stdin_fd)
    (void) close (self->internal_stdin_fd);
  if (close_stdout_fd)
    (void) close (self->internal_stdout_fd);
  if (close_stderr_fd)
    (void) close (self->internal_stderr_fd);
#endif
  return ret;
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
GSubprocess *    g_subprocess_new (gchar                **argv,
				   const gchar           *cwd,
				   gchar                **env,
				   GSpawnFlags            spawn_flags,
				   GSpawnChildSetupFunc   child_setup,
				   gpointer               user_data,
				   GObject               *stdin_disposition,
				   GObject               *stdout_disposition,
				   GObject               *stderr_disposition,
				   GError               **error)
{
  return g_initable_new (G_TYPE_SUBPROCESS,
			 NULL,
			 error,
			 "argv", argv,
			 "working-directory", cwd,
			 "environment", env,
			 "spawn-flags", spawn_flags,
			 "child-setup-func", child_setup,
			 "child-setup-user-data", user_data,
			 "stdin-disposition", stdin_disposition,
			 "stdout-disposition", stdout_disposition,
			 "stderr-disposition", stderr_disposition,
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

static GObject *
get_disposition (GSubprocessStreamDispositionKind kind)
{
  static gsize initialized = 0;
  static GPtrArray* dispositions = NULL;
  
  if (g_once_init_enter (&initialized))
    {
      int i;
      dispositions = g_ptr_array_new_with_free_func ((GDestroyNotify)g_object_unref);
      for (i = 0; i < G_SUBPROCESS_STREAM_DISPOSITION_LAST+1; i++)
	{
	  GSubprocessStreamDisposition *disposition = g_object_new (g_subprocess_stream_disposition_get_type (), NULL);
	  disposition->kind = (GSubprocessStreamDispositionKind)i;
	  g_ptr_array_add (dispositions, disposition);
	}
      g_once_init_leave (&initialized, TRUE);
    }
  return dispositions->pdata[(guint)kind];
}

GObject *
g_subprocess_stream_devnull (void)
{
  return get_disposition (G_SUBPROCESS_STREAM_DISPOSITION_DEVNULL);
}

GObject *
g_subprocess_stream_inherit (void)
{
  return get_disposition (G_SUBPROCESS_STREAM_DISPOSITION_INHERIT);
}

GObject *
g_subprocess_stream_pipe (void)
{
  return get_disposition (G_SUBPROCESS_STREAM_DISPOSITION_PIPE);
}

GObject *
g_subprocess_stream_merge_stdout (void)
{
  return get_disposition (G_SUBPROCESS_STREAM_DISPOSITION_MERGE_STDOUT);
}

GOutputStream *
g_subprocess_get_stdin_pipe (GSubprocess       *self)
{
  g_return_val_if_fail (G_IS_SUBPROCESS (self), NULL);
  g_return_val_if_fail (self->stdin_disposition == g_subprocess_stream_pipe (), NULL);

  return self->stdin_stream;
}

GInputStream *
g_subprocess_get_stdout_pipe (GSubprocess      *self)
{
  g_return_val_if_fail (G_IS_SUBPROCESS (self), NULL);
  g_return_val_if_fail (self->stdout_disposition == g_subprocess_stream_pipe (), NULL);

  return self->stdout_stream;
}

GInputStream *
g_subprocess_get_stderr_pipe (GSubprocess      *self)
{
  g_return_val_if_fail (G_IS_SUBPROCESS (self), NULL);
  g_return_val_if_fail (self->stderr_disposition == g_subprocess_stream_pipe (), NULL);

  return self->stderr_stream;
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
g_subprocess_request_exit (GSubprocess       *self)
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
g_subprocess_force_exit (GSubprocess       *self)
{
  g_return_if_fail (G_IS_SUBPROCESS (self));

#ifdef G_OS_UNIX
  (void) kill (self->pid, SIGKILL);
#else
  TerminateProcess (self->pid, 1);
#endif
}
