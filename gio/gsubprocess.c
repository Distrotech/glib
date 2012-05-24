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
 * @short_description: Create child processes
 *
 * This class is primarily convenience API on top of the lower-level
 * g_spawn_async_with_pipes() and related functions provided by GLib.
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

typedef struct _GSubprocessClass GSubprocessClass;

#ifdef G_OS_UNIX
static void
g_subprocess_unix_queue_waitpid (GSubprocess  *self);
#endif

struct _GSubprocessClass {
  GObjectClass parent_class;
};

typedef enum {
  G_SUBPROCESS_STATE_BUILDING,
  G_SUBPROCESS_STATE_RUNNING,
  G_SUBPROCESS_STATE_TERMINATED
} GSubprocessState;

struct _GSubprocess
{
  GObject parent;
  GSubprocessState state;

  gchar *child_argv0;
  GPtrArray *child_argv;
  gchar **child_envp;

  guint detached : 1;
  guint search_path : 1;
  guint search_path_from_envp : 1;
  guint leave_descriptors_open : 1;
  guint stdin_to_devnull : 1;
  guint stdout_to_devnull : 1;
  guint stderr_to_devnull : 1;
  guint stderr_to_stdout : 1;

  guint reaped_child : 1;

  gint io_priority;

  gchar *working_directory;

  GSpawnChildSetupFunc child_setup;
  gpointer             child_setup_user_data;

  gint stdin_fd;
  gint internal_stdin_fd;
  gchar *stdin_path;
  GInputStream *stdin_stream;

  gint stdout_fd;
  gint internal_stdout_fd;
  gint stderr_fd;
  gint internal_stderr_fd;

  GError *internal_error;

  /* Used when we're writing input to the child via a pipe. */
  GOutputStream *child_input_pipe_stream;

  GPid pid;

  gint status_code;
};

G_DEFINE_TYPE (GSubprocess, g_subprocess, G_TYPE_OBJECT);

enum
{
  PROP_0,
  PROP_EXECUTABLE
};

static void
g_subprocess_init (GSubprocess  *self)
{
  self->state = G_SUBPROCESS_STATE_BUILDING;
  self->child_argv = g_ptr_array_new_with_free_func (g_free);
  self->stdin_to_devnull = TRUE;
  self->stdin_fd = -1;
  self->internal_stdin_fd = -1;
  self->stdout_fd = -1;
  self->internal_stdout_fd = -1;
  self->stderr_fd = -1;
  self->internal_stderr_fd = -1;
  self->io_priority = G_PRIORITY_DEFAULT;
}

static void
g_subprocess_dispose (GObject *object)
{
  GSubprocess *self = G_SUBPROCESS (object);

  g_clear_object (&self->stdin_stream);
  g_clear_object (&self->child_input_pipe_stream);
  g_clear_error (&self->internal_error);

  if (G_OBJECT_CLASS (g_subprocess_parent_class)->dispose != NULL)
    G_OBJECT_CLASS (g_subprocess_parent_class)->dispose (object);
}

static void
g_subprocess_finalize (GObject *object)
{
  GSubprocess *self = G_SUBPROCESS (object);

  if (self->state > G_SUBPROCESS_STATE_BUILDING
      && !self->detached
      && !self->reaped_child)
    {
#ifdef G_OS_UNIX
      /* Here we need to actually call waitpid() to clean up the
       * zombie.  In case the child hasn't actually exited, defer this
       * cleanup to the worker thread.
       */
      g_subprocess_unix_queue_waitpid (self);
#endif
      g_spawn_close_pid (self->pid);
    }

  if (self->child_argv)
    g_ptr_array_unref (self->child_argv);

  g_strfreev (self->child_envp);

  g_free (self->working_directory);

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
    case PROP_EXECUTABLE:
      g_assert (self->child_argv->len == 0);
      g_ptr_array_add (self->child_argv, g_value_dup_string (value));
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
    case PROP_EXECUTABLE:
      if (self->child_argv->len > 0)
	g_value_set_string (value,
			    self->child_argv->pdata[0]);
      else
	g_value_set_string (value, NULL);
      break;

    default:
      g_assert_not_reached ();
    }
}

static void
g_subprocess_class_init (GSubprocessClass *class)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (class);

  gobject_class->dispose = g_subprocess_dispose;
  gobject_class->finalize = g_subprocess_finalize;
  gobject_class->get_property = g_subprocess_get_property;
  gobject_class->set_property = g_subprocess_set_property;

  /**
   * GSubprocess:executable:
   *
   * Argument to use as executable; if
   * g_subprocess_set_use_search_path() has not been used, then this
   * must be a full path.
   *
   * Since: 2.34
   */
  g_object_class_install_property (gobject_class, PROP_EXECUTABLE,
				   g_param_spec_string ("executable",
							P_("Executable"),
							P_("Path to executable"),
							NULL, G_PARAM_READWRITE | G_PARAM_CONSTRUCT |
							G_PARAM_STATIC_STRINGS));
}

/**** Creation ****/

/**
 * g_subprocess_new:
 * @executable: (type filename): Executable path, in GLib filename encoding
 *
 * Creates a new subprocess, with @executable as the initial argument
 * vector.  You can append further arguments via
 * e.g. g_subprocess_append_args().
 * 
 * Returns: A new #GSubprocess
 * Since: 2.34
 */
GSubprocess *
g_subprocess_new (const char *executable)
{
  return g_object_new (G_TYPE_SUBPROCESS, "executable", executable, NULL);
}

/**
 * g_subprocess_new_with_args:
 * @executable: (type filename): Executable path
 * @...: a %NULL-terminated list of arguments, in the GLib filename encoding
 *
 * See the documentation of g_subprocess_append_args_va() for details
 * about child process arguments.
 *
 * After calling this function, you may append further arguments with
 * e.g. g_subprocess_append_args().
 * 
 * Returns: A new #GSubprocess with the provided arguments
 * Since: 2.34
 */
GSubprocess *
g_subprocess_new_with_args (const gchar     *executable,
			    ...)
{
  va_list args;
  GSubprocess *ret;

  ret = g_subprocess_new (executable);
  
  va_start (args, executable);
  g_subprocess_append_args_va (ret, args);
  va_end (args);
  
  return ret;
}

/**** Argument control ****/

/**
 * g_subprocess_set_argv:
 * @self: a #GSubprocess:
 * @argv: (array zero-terminated=1) (element-type filename): Argument array, %NULL-terminated
 *
 * Set the arguments to be used by the child process.  This will
 * overwrite the entire argument vector, including the
 * #GSubprocess:executable property, any previous calls to
 * g_subprocess_append_args(), as well as g_subprocess_set_argv0().
 *
 * For more information, see g_subprocess_append_args_va().
 *
 * It is invalid to call this function after g_subprocess_start() has
 * been called.
 *
 * Since: 2.34
 */
void
g_subprocess_set_argv (GSubprocess          *self,
		       gchar               **argv)
{
  g_return_if_fail (G_IS_SUBPROCESS (self));
  g_return_if_fail (argv != NULL);
  g_return_if_fail (*argv != NULL);
  g_return_if_fail (self->state == G_SUBPROCESS_STATE_BUILDING);

  g_subprocess_set_argv0 (self, NULL);

  g_ptr_array_set_size (self->child_argv, 0);
  
  for (; *argv; argv++)
    g_ptr_array_add (self->child_argv, g_strdup (*argv));
}

/**
 * g_subprocess_set_argv0:
 * @self: a #GSubprocess:
 * @argv0: (type filename): First argument
 *
 * On Unix, child processes receive a bytestring provided by the
 * parent process as the first argument (i.e. argv[0]).  By default,
 * GLib will set this to whatever executable is run, but you may override
 * it with this function.
 *
 * For example, some implementations of Unix have just one 'grep'
 * executable which behaves differently when executed as 'grep' or
 * 'fgrep'.
 *
 * It is invalid to call this function after g_subprocess_start() has
 * been called.
 *
 * Since: 2.34
 */
void
g_subprocess_set_argv0 (GSubprocess         *self,
			const gchar         *argv0)
{
  g_return_if_fail (G_IS_SUBPROCESS (self));
  g_return_if_fail (self->state == G_SUBPROCESS_STATE_BUILDING);

  g_free (self->child_argv0);
  self->child_argv0 = g_strdup (argv0);
}

/**
 * g_subprocess_append_arg:
 * @self: a #GSubprocess:
 * @arg: (type filename): Argument
 *
 * Append an argument to the child process.  On Unix, this argument is
 * a bytestring.  For more information, see
 * g_subprocess_append_args_va().
 *
 * It is invalid to call this function after g_subprocess_start() has
 * been called.
 *
 * Since: 2.34
 */
void
g_subprocess_append_arg (GSubprocess       *self,
			 const gchar       *arg)
{
  g_return_if_fail (G_IS_SUBPROCESS (self));
  g_return_if_fail (self->state == G_SUBPROCESS_STATE_BUILDING);

  g_ptr_array_add (self->child_argv, g_strdup (arg));
}


/**
 * g_subprocess_append_args:
 * @self: a #GSubprocess:
 * @first: (type filename): First argument to be appended
 * @...: a %NULL-terminated list of arguments
 *
 * Appends arguments to the child process.  For more information, see
 * g_subprocess_append_args_va().
 *
 * It is invalid to call this function after g_subprocess_start() has
 * been called.
 *
 * Since: 2.34
 */
void
g_subprocess_append_args (GSubprocess       *self,
			  const gchar       *first,
			  ...) 
{
  va_list args;

  g_return_if_fail (G_IS_SUBPROCESS (self));
  g_return_if_fail (first != NULL);
  g_return_if_fail (self->state == G_SUBPROCESS_STATE_BUILDING);
  
  g_subprocess_append_arg (self, first);

  va_start (args, first);
  g_subprocess_append_args_va (self, args);
  va_end (args);
}

/**
 * g_subprocess_append_args_va:
 * @self: a #GSubprocess
 * @args: %NULL-terminated list of arguments, in GLib filename encoding
 *
 * Append the provided @args to the child argument vector.  Each
 * element should be in the GLib filename encoding.
 *
 * On Unix, the arguments are bytestrings.  Note though that while
 * it's possible to pass arbitrary bytestrings to subprocesses on
 * Unix, not all utility programs are written to handle this
 * correctly.
 *
 * On Windows, the filename encoding is UTF-8, so the arguments here
 * should be as well.  For more information about how arguments are
 * processed on Windows, see the documentation of
 * g_spawn_async_with_pipes().
 *
 * It is invalid to call this function after g_subprocess_start() has
 * been called.
 *
 * Since: 2.34
 */
void
g_subprocess_append_args_va (GSubprocess       *self,
			     va_list            args)
{
  const char *arg;

  g_return_if_fail (G_IS_SUBPROCESS (self));
  g_return_if_fail (self->state == G_SUBPROCESS_STATE_BUILDING);

  while ((arg = va_arg (args, const char *)) != NULL)
    {
      g_ptr_array_add (self->child_argv, g_strdup (arg));
    }
}

/**** GSpawnFlags wrappers ****/

/**
 * g_subprocess_set_detached:
 * @self: a #GSubprocess
 * @detached: %TRUE if the child shouldn't be watched
 *
 * Unlike g_spawn_async_with_pipes(), the default for #GSubprocess is
 * to assume that child exit status should be available; this
 * corresponds to the %G_SPAWN_DO_NOT_REAP_CHILD flag.
 *
 * If you don't plan to monitor the child status via a function like
 * g_subprocess_add_watch() or g_subprocess_wait_sync(), you should
 * set this flag.
 *
 * It is invalid to call this function after g_subprocess_start() has
 * been called.
 *
 * Since: 2.34
 */
void
g_subprocess_set_detached (GSubprocess     *self,
			   gboolean         detached)
{
  g_return_if_fail (G_IS_SUBPROCESS (self));
  g_return_if_fail (self->state == G_SUBPROCESS_STATE_BUILDING);
  
  self->detached = detached;
}

/**
 * g_subprocess_set_use_search_path:
 * @self: a #GSubprocess
 * @do_search_path: %TRUE if the system path should be searched
 *
 * By default, the name of the program must be a full path; the
 * <envar>PATH</envar> shell variable will only be searched if you set
 * @do_search_path to %TRUE.  If the program name is not a full path
 * and this flag is not set, then the program will be run from the
 * current directory (or @working_directory, if specified); this might
 * be unexpected or even dangerous in some cases when the current
 * directory is world-writable.
 *
 * This option corresponds to the %G_SPAWN_SEARCH_PATH flag.
 *
 * It is invalid to call this function after g_subprocess_start() has
 * been called.
 *
 * Since: 2.34
 */
void
g_subprocess_set_use_search_path (GSubprocess     *self,
				  gboolean         do_search_path)
{
  g_return_if_fail (G_IS_SUBPROCESS (self));
  g_return_if_fail (self->state == G_SUBPROCESS_STATE_BUILDING);
  
  self->search_path = do_search_path;
}

/**
 * g_subprocess_set_use_search_path_from_envp:
 * @self: a #GSubprocess
 * @do_search_path_from_envp: %TRUE if the PATH environment variable should be searched
 *
 * This option corresponds to the %G_SPAWN_SEARCH_PATH_FROM_ENVP flag;
 * the uses for this are fairly specialized, such as when you are
 * proxying execution from another context.
 *
 * It is invalid to call this function after g_subprocess_start() has
 * been called.
 *
 * Since: 2.34
 */
void
g_subprocess_set_use_search_path_from_envp (GSubprocess     *self,
					    gboolean         do_search_path_from_envp)
{
  g_return_if_fail (G_IS_SUBPROCESS (self));
  g_return_if_fail (self->state == G_SUBPROCESS_STATE_BUILDING);
  
  self->search_path_from_envp = do_search_path_from_envp;

}

/**
 * g_subprocess_set_leave_descriptors_open:
 * @self: a #GSubprocess
 * @do_leave_descriptors_open: %TRUE if file descriptors should be left open
 *
 * Setting @do_leave_descriptors_open to %TRUE means that the parent's
 * open file descriptors will be inherited by the child; otherwise all
 * descriptors except stdin/stdout/stderr will be closed before
 * calling exec() in the child.  The default is %FALSE.
 *
 * Note that on Unix, this option is completely independent of file
 * descriptors which have the close-on-exec flag set.  In other words,
 * if you set @do_leave_descriptors_open to %FALSE, descriptors which
 * are close-on-exec will still be closed by the operating system.
 *
 * This option corresponds to the %G_SPAWN_LEAVE_DESCRIPTORS_OPEN flag.
 *
 * It is invalid to call this function after g_subprocess_start() has
 * been called.
 *
 * Since: 2.34
 */
void
g_subprocess_set_leave_descriptors_open (GSubprocess     *self,
					 gboolean         do_leave_descriptors_open)
{
  g_return_if_fail (G_IS_SUBPROCESS (self));
  g_return_if_fail (self->state == G_SUBPROCESS_STATE_BUILDING);
  
  self->leave_descriptors_open = do_leave_descriptors_open;
}

/**** Environment control ****/

/**
 * g_subprocess_setenv:
 * @self: a #GSubprocess
 * @variable: the environment variable to set, must not contain '='
 * @value: the value for to set the variable to
 * @overwrite: whether to change the variable if it already exists
 *
 * By default, the child process will inherit a copy of the
 * environment of the current process.  This function, upon the first
 * call, will take a snapshot of the current environment via
 * g_get_environ(), then update it.  Further calls manipulate the
 * snapshot.
 *
 * This has no effect on the current process.  See the documentation
 * of g_environ_setenv() for more information about environment
 * variables.
 *
 * It is invalid to call this function after g_subprocess_start() has
 * been called.
 *
 * Since: 2.34
 */
void
g_subprocess_setenv (GSubprocess       *self,
		     gchar             *variable,
		     gchar             *value,
		     gboolean           overwrite)
{
  g_return_if_fail (G_IS_SUBPROCESS (self));
  g_return_if_fail (self->state == G_SUBPROCESS_STATE_BUILDING);
  g_return_if_fail (variable != NULL);

  if (self->child_envp == NULL)
    self->child_envp = g_get_environ ();
  
  self->child_envp = g_environ_setenv (self->child_envp, variable, value,
				       overwrite);
}

/**
 * g_subprocess_unsetenv:
 * @self: a #GSubprocess
 * @variable: the environment variable to set, must not contain '='
 *
 * This modifies the environment child process will be run in; it has
 * no effect on the current process.  See the documentation of
 * g_environ_unsetenv() for more information about environment
 * variables.
 *
 * It is invalid to call this function after g_subprocess_start() has
 * been called.
 */
void
g_subprocess_unsetenv (GSubprocess       *self,
		       const gchar       *variable)
{
  g_return_if_fail (G_IS_SUBPROCESS (self));
  g_return_if_fail (self->state == G_SUBPROCESS_STATE_BUILDING);
  g_return_if_fail (variable != NULL);

  if (self->child_envp == NULL)
    self->child_envp = g_get_environ ();

  self->child_envp = g_environ_unsetenv (self->child_envp, variable);
}

/**
 * g_subprocess_set_environment:
 * @self: a #GSubprocess
 * @envp: (array zero-terminated=1): An environment list
 *
 * This completely replaces the environment child process will be run
 * in; it has no effect on the current process.  See the documentation
 * of g_environ_setenv() for more information about environment
 * variables.
 *
 * It is invalid to call this function after g_subprocess_start() has
 * been called.
 *
 * Since: 2.34
 */
void
g_subprocess_set_environment (GSubprocess       *self,
			      gchar            **envp)
{
  g_return_if_fail (G_IS_SUBPROCESS (self));
  g_return_if_fail (self->state == G_SUBPROCESS_STATE_BUILDING);
  g_return_if_fail (envp != NULL);

  g_strfreev (self->child_envp);
  self->child_envp = g_strdupv (envp);
}

/**
 * g_subprocess_set_working_directory:
 * @self: a #GSubprocess
 * @working_directory: (allow-none): Path to working directory, in the GLib file name encoding
 *
 * By default, the child process will inherit the working
 * directory of the current process.  This function allows
 * overriding that default.
 *
 * It is invalid to call this function after g_subprocess_start() has
 * been called.
 */
void
g_subprocess_set_working_directory (GSubprocess       *self,
				    const gchar       *working_directory)
{
  g_return_if_fail (G_IS_SUBPROCESS (self));
  g_return_if_fail (self->state == G_SUBPROCESS_STATE_BUILDING);

  g_free (self->working_directory);
  self->working_directory = g_strdup (working_directory);
}

/**
 * g_subprocess_set_child_setup:
 * @self: a #GSubprocess
 * @child_setup: (allow-none): Function to run in the context of just-forked child
 * @user_data: (allow-none): User data for @child_setup
 *
 * This functionality is only available on Unix.  See the
 * documentation of g_spawn_async_with_pipes() for more information
 * about @child_setup.
 *
 * It is invalid to call this function after g_subprocess_start() has
 * been called.
 *
 * Since: 2.34
 */
void
g_subprocess_set_child_setup (GSubprocess           *self,
			      GSpawnChildSetupFunc   child_setup,
			      gpointer               user_data)
{
  g_return_if_fail (G_IS_SUBPROCESS (self));
  g_return_if_fail (self->state == G_SUBPROCESS_STATE_BUILDING);
#ifdef G_OS_UNIX
  self->child_setup = child_setup;
  self->child_setup_user_data = user_data;
#else
  g_warning ("g_subprocess_set_child_setup is only available on Unix");
#endif
}

/**** Input and Output ****/

/**
 * g_subprocess_set_io_priority:
 * @self: a #GSubprocess
 * @io_priority: I/O priority
 *
 * For the cases where #GSubprocess internally uses asynchronous I/O,
 * this function allows controlling the priority.  The default is
 * %G_PRIORITY_DEFAULT.
 *
 * It is invalid to call this function after g_subprocess_start() has
 * been called.
 *
 * Since: 2.34
 */
void
g_subprocess_set_io_priority (GSubprocess       *self,
			      gint               io_priority)
{
  g_return_if_fail (G_IS_SUBPROCESS (self));
  g_return_if_fail (self->state == G_SUBPROCESS_STATE_BUILDING);

  self->io_priority = io_priority;
}

/**
 * g_subprocess_set_standard_input_file_path:
 * @self: a #GSubprocess
 * @file_path: String containing path to file to use as standard input
 *
 * This function allows providing a file as input to the given
 * subprocess.  The file will not be opened until g_subprocess_start()
 * has been called.
 *
 * Calling this function overrides any previous calls, as well as
 * other related functions such as
 * g_subprocess_set_standard_input_unix_fd(),
 * g_subprocess_set_standard_input_stream(), and
 * g_subprocess_set_standard_input_to_devnull().
 *
 * It is invalid to call this function after g_subprocess_start() has
 * been called.
 *
 * Since: 2.34
 */
void
g_subprocess_set_standard_input_file_path (GSubprocess       *self,
					   const gchar       *file_path)
{
  g_return_if_fail (G_IS_SUBPROCESS (self));
  g_return_if_fail (self->state == G_SUBPROCESS_STATE_BUILDING);
  g_return_if_fail (file_path != NULL);

  g_clear_object (&self->stdin_stream);
  g_clear_pointer (&self->stdin_path, g_free);
  self->stdin_to_devnull = FALSE;
  self->stdin_fd = -1;

  self->stdin_path = g_strdup (file_path);
}

/**
 * g_subprocess_set_standard_input_unix_fd:
 * @self: a #GSubprocess
 * @fd: File descriptor to use as standard input
 *
 * This function allows providing a file descriptor as input to the
 * given subprocess.  Only available on Unix.
 *
 * Calling this function overrides any previous calls, as well as
 * other related functions such as
 * g_subprocess_set_standard_input_file_path().
 *
 * It is invalid to call this function after g_subprocess_start() has
 * been called.
 *
 * Since: 2.34
 */
#ifdef G_OS_UNIX
void
g_subprocess_set_standard_input_unix_fd (GSubprocess       *self,
					 gint               fd)
{
  g_return_if_fail (G_IS_SUBPROCESS (self));
  g_return_if_fail (self->state == G_SUBPROCESS_STATE_BUILDING);

  g_clear_object (&self->stdin_stream);
  g_clear_pointer (&self->stdin_path, g_free);
  self->stdin_to_devnull = FALSE;
  self->stdin_fd = -1;

  self->stdin_fd = fd;
}
#endif

/**
 * g_subprocess_set_standard_input_to_devnull:
 * @self: a #GSubprocess
 * @to_devnull: If %TRUE, redirect input from null stream, if %FALSE, inherit
 *
 * The default is for child processes to have their input stream
 * pointed at a null stream (e.g. on Unix, /dev/null), because having
 * multiple processes read from an input stream creates race
 * conditions and is generally nonsensical.  See the documentation of
 * g_spawn_async_with_pipes() and %G_SPAWN_CHILD_INHERITS_STDIN.
 *
 * If @to_devnull is %FALSE, then this function will cause the
 * standard input of the child process to be inherited.
 *
 * Calling this function overrides any previous calls, as well as
 * other related functions such as
 * g_subprocess_set_standard_input_file_path().
 *
 * It is invalid to call this function after g_subprocess_start() has
 * been called.
 *
 * Since: 2.34
 */
void
g_subprocess_set_standard_input_to_devnull (GSubprocess       *self,
					    gboolean           to_devnull)
{
  g_return_if_fail (G_IS_SUBPROCESS (self));
  g_return_if_fail (self->state == G_SUBPROCESS_STATE_BUILDING);

  g_clear_object (&self->stdin_stream);
  g_clear_pointer (&self->stdin_path, g_free);
  self->stdin_to_devnull = FALSE;
  self->stdin_fd = -1;

  self->stdin_to_devnull = to_devnull;
}

/**
 * g_subprocess_set_standard_input_stream:
 * @self: a #GSubprocess
 * @stream: an input stream
 *
 * Use the provided stream as input to the child process.  When the
 * process is started, such as via g_subprocess_start(), the stream
 * will be asynchronously provided to the child, via a function
 * equivalent to g_output_stream_splice_async().  This operation may
 * or may not occur in a separate thread; if your program modifies the
 * given @stream after the process has started, the given @stream must
 * be threadsafe.
 * 
 * Because of the fact that input is asynchronous, it is safe to use
 * g_subprocess_set_standard_input_stream(), then synchronously wait
 * for the child to complete via e.g. g_subprocess_run_sync().  The
 * high level utility function g_subprocess_run_sync_get_stdout_utf8()
 * is built this way.
 *
 * <note>If your input stream is from a file, such as that returned
 * by g_file_read(), it is significantly more efficient (on Unix) to
 * cause the child process to read the file directly via
 * g_subprocess_set_standard_input_file_path().</note>
 *
 * Calling this function overrides any previous calls, as well as
 * other related functions such as
 * g_subprocess_set_standard_input_file_path().
 *
 * It is invalid to call this function after g_subprocess_start() has
 * been called.
 *
 * Since: 2.34
 */
void
g_subprocess_set_standard_input_stream (GSubprocess       *self,
					GInputStream      *stream)
{
  g_return_if_fail (G_IS_SUBPROCESS (self));
  g_return_if_fail (self->state == G_SUBPROCESS_STATE_BUILDING);
  g_return_if_fail (stream != NULL);

  g_clear_object (&self->stdin_stream);
  g_clear_pointer (&self->stdin_path, g_free);
  self->stdin_to_devnull = FALSE;
  self->stdin_fd = -1;

  self->stdin_stream = g_object_ref (stream);
}

/**
 * g_subprocess_set_standard_input_bytes:
 * @self: a #GSubprocess
 * @buf: Buffer to use as input
 *
 * Use the provided data as input to the child process.  This
 * function simply wraps g_subprocess_set_standard_input_stream()
 * using g_memory_input_stream_new_from_bytes().
 *
 * Calling this function overrides any previous calls, as well as
 * other related functions such as
 * g_subprocess_set_standard_input_file_path().
 *
 * It is invalid to call this function after g_subprocess_start() has
 * been called.
 *
 * Since: 2.34
 */
void
g_subprocess_set_standard_input_bytes (GSubprocess       *self,
				       GBytes            *buf)
{
  GInputStream *stream;

  g_return_if_fail (G_IS_SUBPROCESS (self));
  g_return_if_fail (self->state == G_SUBPROCESS_STATE_BUILDING);
  g_return_if_fail (buf != NULL);

  stream = g_memory_input_stream_new_from_bytes (buf);
  g_subprocess_set_standard_input_stream (self, stream);
  g_object_unref (stream);
}

/**
 * g_subprocess_set_standard_input_str:
 * @self: a #GSubprocess
 * @str: (array zero-terminated=1) (element-type guint8): Buffer to use as input
 *
 * Use the provided data as input to the child process.  This function
 * simply wraps g_subprocess_set_standard_input_bytes().
 *
 * Calling this function overrides any previous calls, as well as
 * other related functions such as
 * g_subprocess_set_standard_input_file_path().
 *
 * It is invalid to call this function after g_subprocess_start() has
 * been called.
 *
 * Since: 2.34
 */
void
g_subprocess_set_standard_input_str (GSubprocess       *self,
				     const gchar       *str)
{
  GBytes *bytes;

  g_return_if_fail (G_IS_SUBPROCESS (self));
  g_return_if_fail (self->state == G_SUBPROCESS_STATE_BUILDING);
  g_return_if_fail (str != NULL);

  bytes = g_bytes_new (str, strlen (str));
  g_subprocess_set_standard_input_bytes (self, bytes);
  g_bytes_unref (bytes);
}

/**
 * g_subprocess_set_standard_output_to_devnull:
 * @self: a #GSubprocess
 * @to_devnull: If %TRUE, redirect process output to null stream
 *
 * The default is for the child process to inherit the standard output
 * of the current process.  Specify %TRUE for @to_devnull to redirect
 * it to the operating system's null stream.
 *
 * Calling this function overrides any previous calls, as well as
 * other related functions such as
 * g_subprocess_set_standard_output_unix_fd().
 *
 * It is invalid to call this function after g_subprocess_start() has
 * been called.
 *
 * Since: 2.34
 */
void
g_subprocess_set_standard_output_to_devnull (GSubprocess       *self,
					     gboolean           to_devnull)
{
  g_return_if_fail (G_IS_SUBPROCESS (self));
  g_return_if_fail (self->state == G_SUBPROCESS_STATE_BUILDING);

  self->stdout_to_devnull = FALSE;
  self->stdout_fd = -1;

  self->stdout_to_devnull = to_devnull;
}

/**
 * g_subprocess_set_standard_output_unix_fd:
 * @self: a #GSubprocess
 * @fd: File descriptor
 *
 * This function allows providing a file descriptor as standard output
 * for the given subprocess.  Only available on Unix.
 *
 * Calling this function overrides any previous calls, as well as
 * other related functions such as
 * g_subprocess_set_standard_output_to_devnull().
 *
 * It is invalid to call this function after g_subprocess_start() has
 * been called.
 *
 * Since: 2.34
 */
#ifdef G_OS_UNIX
void
g_subprocess_set_standard_output_unix_fd (GSubprocess       *self,
					  gint               fd)
{
  g_return_if_fail (G_IS_SUBPROCESS (self));
  g_return_if_fail (self->state == G_SUBPROCESS_STATE_BUILDING);

  self->stdout_to_devnull = FALSE;
  self->stdout_fd = -1;

  self->stdout_fd = fd;
}
#endif

/**
 * g_subprocess_set_standard_error_to_devnull:
 * @self: a #GSubprocess
 * @to_devnull: If %TRUE, redirect process error output to null stream
 *
 * The default is for the child process to inherit the standard error
 * of the current process.  Specify %TRUE for @to_devnull to redirect
 * it to the operating system's null stream.
 *
 * Calling this function overrides any previous calls, as well as
 * other related functions such as
 * g_subprocess_set_standard_error_to_stdout().
 *
 * It is invalid to call this function after g_subprocess_start() has
 * been called.
 *
 * Since: 2.34
 */
void
g_subprocess_set_standard_error_to_devnull (GSubprocess       *self,
					    gboolean           to_devnull)
{
  g_return_if_fail (G_IS_SUBPROCESS (self));
  g_return_if_fail (self->state == G_SUBPROCESS_STATE_BUILDING);

  self->stderr_to_devnull = FALSE;
  self->stderr_to_stdout = FALSE;
  self->stderr_fd = -1;

  self->stderr_to_devnull = to_devnull;
}

/**
 * g_subprocess_set_standard_error_to_stdout:
 * @self: a #GSubprocess
 * @to_stdout: If %TRUE, redirect process error output to standard output
 *
 * The default is for the child process to inherit the standard error
 * of the current process.  Specify %TRUE for it to be merged with
 * standard output.  In this case, the disposition of standard output
 * controls the merged stream; for example, if you combine
 * g_subprocess_set_standard_error_to_stdout() and request a stream
 * for standard output from g_subprocess_start_with_pipes(), both
 * streams will be merged into the returned stream.
 *
 * Calling this function overrides any previous calls, as well as
 * other related functions such as
 * g_subprocess_set_standard_error_to_devnull().
 *
 * It is invalid to call this function after g_subprocess_start() has
 * been called.
 *
 * Since: 2.34
 */
void
g_subprocess_set_standard_error_to_stdout (GSubprocess       *self,
					   gboolean           to_stdout)
{
  g_return_if_fail (G_IS_SUBPROCESS (self));
  g_return_if_fail (self->state == G_SUBPROCESS_STATE_BUILDING);

  self->stderr_to_devnull = FALSE;
  self->stderr_to_stdout = FALSE;
  self->stderr_fd = -1;

  self->stderr_to_stdout = to_stdout;

}

/**
 * g_subprocess_set_standard_error_unix_fd:
 * @self: a #GSubprocess
 * @fd: File descriptor
 *
 * This function allows providing a file descriptor as standard error
 * for the given subprocess.  Only available on Unix.
 *
 * Calling this function overrides any previous calls, as well as
 * other related functions such as
 * g_subprocess_set_standard_error_to_devnull().
 *
 * It is invalid to call this function after g_subprocess_start() has
 * been called.
 *
 * Since: 2.34
 */
void
g_subprocess_set_standard_error_unix_fd (GSubprocess       *self,
					  gint               fd)
{
  g_return_if_fail (G_IS_SUBPROCESS (self));
  g_return_if_fail (self->state == G_SUBPROCESS_STATE_BUILDING);

  self->stderr_to_devnull = FALSE;
  self->stderr_to_stdout = FALSE;
  self->stderr_fd = -1;

  self->stderr_fd = fd;
}

/**
 * g_subprocess_start:
 * @self: a #GSubprocess
 * @cancellable: a #GCancellable
 * @error: a #GError
 *
 * When called, this runs the child process asynchronously using the
 * current configuration.  This function is equivalent to calling
 * g_subprocess_start_with_pipes() with %NULL streams.  See the
 * documentation of that function for more information.
 *
 * It is invalid to call this function after g_subprocess_start() has
 * already been called.
 *
 * Returns: %TRUE if subprocess was started, %FALSE on error (and @error will be set)
 * Since: 2.34
 */
gboolean
g_subprocess_start (GSubprocess       *self,
		    GCancellable      *cancellable,
		    GError           **error)
{
  return g_subprocess_start_with_pipes (self, NULL, NULL, NULL,
					cancellable, error);
}

/**
 * g_subprocess_run_sync:
 * @self: a #GSubprocess
 * @cancellable: a #GCancellable
 * @error: a #GError
 *
 * When called, this runs the child process synchronously using the
 * current configuration.
 *
 * This function simply wraps g_subprocess_start() and
 * g_subprocess_wait_sync().  See the documentation for both of those,
 * as well as g_subprocess_query_success().
 *
 * It is invalid to call this function after g_subprocess_start() has
 * already been called.
 *
 * Since: 2.34
 * Returns: %TRUE if subprocess was executed successfully, %FALSE on error (and @error will be set)
 */
gboolean
g_subprocess_run_sync (GSubprocess   *self,
		       GCancellable  *cancellable,
		       GError       **error)
{
  gboolean ret = FALSE;

  g_return_val_if_fail (G_IS_SUBPROCESS (self), FALSE);
  g_return_val_if_fail (self->state == G_SUBPROCESS_STATE_BUILDING, FALSE);

  if (!g_subprocess_start (self, cancellable, error))
    goto out;
  if (!g_subprocess_wait_sync (self, cancellable, error))
    goto out;
  
  ret = TRUE;
 out:
  return ret;
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

static inline void
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

  if (self->stdin_fd >= 0)
    safe_dup2 (self->stdin_fd, 0);
  else if (self->internal_stdin_fd >= 0)
    safe_dup2 (self->internal_stdin_fd, 0);

  if (self->stdout_fd >= 0)
    safe_dup2 (self->stdout_fd, 1);
  else if (self->internal_stdout_fd >= 0)
    safe_dup2 (self->internal_stdout_fd, 1);

  if (self->stderr_fd >= 0)
    safe_dup2 (self->stderr_fd, 2);
  else if (self->internal_stderr_fd >= 0)
    safe_dup2 (self->internal_stderr_fd, 2);

  if (self->stderr_to_stdout)
    safe_dup2 (1, 2);

  if (self->child_setup)
    self->child_setup (self->child_setup_user_data);
}

#endif

static void
internal_error_occurred (GSubprocess   *self,
			 GError       **error)
{
  if (self->internal_error == NULL)
    {
      g_prefix_error (error, _("While writing input to child process: "));
      g_propagate_error (&self->internal_error, *error);
    }
  else
    g_clear_error (error);
}

static void
g_subprocess_on_input_splice_finished (GObject      *src,
				       GAsyncResult *res,
				       gpointer      user_data)
{
  GSubprocess *self = user_data;
  GError *local_error = NULL;
  GError **error = &local_error;
  gssize bytes_written;
  
  bytes_written = g_output_stream_splice_finish (G_OUTPUT_STREAM (src),
						 res, error);
  if (bytes_written < 0)
    {
      internal_error_occurred (self, error);
    }

  g_object_unref (self);
}

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

/**
 * g_subprocess_start_with_pipes:
 * @self: a #GSubprocess
 * @out_stdin_stream: (out) (allow-none): Return location for standard input pipe
 * @out_stdout_stream: (out) (allow-none): Return location for standard output pipe
 * @out_stderr_stream: (out) (allow-none): Return location for standard error pipe
 * @cancellable: a #GCancellable
 * @error: a #GError
 *
 * When called, this runs the child process asynchronously using the
 * current configuration, with pipes connected to any of standard
 * input, standard output and standard error.
 *
 * See the documentation for g_spawn_async_with_pipes() for more
 * information about how the child process will be run.
 *
 * If both input and output streams are given, using synchronous I/O
 * from the same thread on both sides risks deadlock.  Use
 * asynchronous I/O such as g_input_stream_read_async() or higher
 * level wrappers.
 *
 * The provided @cancellable controls internal input splicing.
 * Specifically, if an input stream has been provided to the process
 * via g_subprocess_set_standard_input_stream() or a wrapper such as
 * g_subprocess_set_standard_input_bytes(), then cancelling
 * @cancellable will stop I/O.  Because input I/O errors will be
 * returned from g_subprocess_query_success(), the returned error in
 * that case will be %G_IO_ERROR_CANCELLED.
 *
 * It is invalid to call this function after
 * g_subprocess_start() has already been called.
 *
 * Since: 2.34
 * Returns: %TRUE if subprocess was started, %FALSE on error (and @error will be set)
 */
gboolean
g_subprocess_start_with_pipes (GSubprocess       *self,
			       GOutputStream    **out_stdin_stream,
			       GInputStream     **out_stdout_stream,
			       GInputStream     **out_stderr_stream,
			       GCancellable      *cancellable,
			       GError           **error)
{
  gboolean ret = FALSE;
  GPtrArray *tmp_argv = NULL;
  gchar **real_argv;
  GSpawnFlags spawn_flags = 0;
  gint stdin_pipe_fd = -1;
  gint *stdin_arg = NULL;
  gint stdout_pipe_fd = -1;
  gint *stdout_arg = NULL;
  gint stderr_pipe_fd = -1;
  gint *stderr_arg = NULL;
  GSpawnChildSetupFunc child_setup;
  gpointer child_setup_user_data;

  g_return_val_if_fail (G_IS_SUBPROCESS (self), FALSE);
  g_return_val_if_fail (self->state == G_SUBPROCESS_STATE_BUILDING, FALSE);
  g_return_val_if_fail (self->child_argv->len > 0, FALSE);

  if (g_cancellable_set_error_if_cancelled (cancellable, error))
    return FALSE;

  if (out_stdin_stream)
    g_return_val_if_fail (self->stdin_fd == -1
			  && self->stdin_path == NULL
			  && self->stdin_stream == NULL,
			  FALSE);
  if (out_stdout_stream)
    g_return_val_if_fail (self->stdout_fd == -1
			  && self->stdout_to_devnull == FALSE,
			  FALSE);
  if (out_stderr_stream)
    g_return_val_if_fail (self->stderr_fd == -1
			  && self->stderr_to_devnull == FALSE
			  && self->stderr_to_stdout == FALSE,
			  FALSE);

#ifdef G_OS_UNIX
  if (self->stdin_path)
    {
      self->internal_stdin_fd = g_open (self->stdin_path, O_RDONLY);
      if (self->internal_stdin_fd < 0)
	{
	  g_set_error (error, G_IO_ERROR, g_io_error_from_errno (errno),
		       _("Failed to open file '%s'"), self->stdin_path);
	  goto out;
	}
      g_free (self->stdin_path);
      self->stdin_path = NULL;
    }
#else
  if (self->stdin_path)
    {
      GFile *stdin_file;

      stdin_file = g_file_new_for_path (self->stdin_path);
      self->stdin_stream = (GInputStream*)g_file_read (stdin_file, cancellable, error);
      g_object_unref (stdin_file);
      if (!self->stdin_stream)
	goto out;
      g_free (self->stdin_path);
      self->stdin_path = NULL;
    }
#endif

  g_assert (self->stdin_path == NULL);

  if (self->child_argv0)
    {
      guint i;

      tmp_argv = g_ptr_array_new ();
      g_assert (self->child_argv->len > 0);
      g_ptr_array_add (tmp_argv, self->child_argv->pdata[0]);
      g_ptr_array_add (tmp_argv, self->child_argv0);
      for (i = 1; i < self->child_argv->len; i++)
	g_ptr_array_add (tmp_argv, self->child_argv->pdata[i]);
      g_ptr_array_add (tmp_argv, NULL);

      real_argv = (char**)tmp_argv->pdata;

      spawn_flags |= G_SPAWN_FILE_AND_ARGV_ZERO;
    }
  else
    {
      /* Now add the trailing NULL */
      g_ptr_array_add (self->child_argv, NULL);
      real_argv = (char**)self->child_argv->pdata;
    }

  if (self->leave_descriptors_open)
    spawn_flags |= G_SPAWN_LEAVE_DESCRIPTORS_OPEN;
  if (self->search_path)
    spawn_flags |= G_SPAWN_SEARCH_PATH;
  if (self->search_path_from_envp)
    spawn_flags |= G_SPAWN_SEARCH_PATH_FROM_ENVP;
  if (!self->detached)
    spawn_flags |= G_SPAWN_DO_NOT_REAP_CHILD;

#ifdef G_OS_UNIX
  child_setup = g_subprocess_internal_child_setup;
  child_setup_user_data = self;
#else
  child_setup = NULL;
  child_setup_user_data = NULL;
#endif

  if (out_stdin_stream != NULL
      || self->stdin_stream != NULL)
    stdin_arg = &stdin_pipe_fd;
  else
    {
      g_assert (self->stdin_fd != -1 || self->stdin_to_devnull);
      stdin_arg = NULL;
      if (!self->stdin_to_devnull)
	spawn_flags |= G_SPAWN_CHILD_INHERITS_STDIN;
    }

  if (out_stdout_stream != NULL)
    stdout_arg = &stdout_pipe_fd;
  else
    {
      g_assert (self->stdout_fd == -1 || self->stdout_to_devnull);
      stdout_arg = NULL;
      if (self->stdout_to_devnull)
	spawn_flags |= G_SPAWN_STDOUT_TO_DEV_NULL;
    }

  if (out_stderr_stream != NULL)
    stderr_arg = &stderr_pipe_fd;
  else
    {
      g_assert (self->stderr_fd == -1
		|| self->stderr_to_devnull
		|| self->stderr_to_stdout);
      stderr_arg = NULL;
      if (self->stderr_to_devnull)
	spawn_flags |= G_SPAWN_STDERR_TO_DEV_NULL;
    }

  if (!g_spawn_async_with_pipes (self->working_directory,
				 real_argv,
				 self->child_envp,
				 spawn_flags,
				 child_setup, child_setup_user_data,
				 self->detached ? NULL : &self->pid,
				 stdin_arg, stdout_arg, stderr_arg,
				 error))
    goto out;

  ret = TRUE;
  self->state = G_SUBPROCESS_STATE_RUNNING;

  if (stdin_pipe_fd != -1)
    {
      GOutputStream *child_stdout = platform_output_stream_from_spawn_fd (stdin_pipe_fd);
      if (self->stdin_stream)
	{
	  self->child_input_pipe_stream = child_stdout;
	  g_output_stream_splice_async (self->child_input_pipe_stream,
					self->stdin_stream,
					G_OUTPUT_STREAM_SPLICE_CLOSE_SOURCE |
					G_OUTPUT_STREAM_SPLICE_CLOSE_TARGET,
					self->io_priority,
					cancellable,
					g_subprocess_on_input_splice_finished,
					g_object_ref (self));
	}
      else
	{
	  g_assert (out_stdin_stream);
	  *out_stdin_stream = child_stdout;
	}
    }
  if (stdout_pipe_fd != -1)
    {
      g_assert (out_stdout_stream);
      *out_stdout_stream = platform_input_stream_from_spawn_fd (stdout_pipe_fd);
    }
  if (stderr_pipe_fd != -1)
    {
      g_assert (out_stderr_stream);
      *out_stderr_stream = platform_input_stream_from_spawn_fd (stderr_pipe_fd);
    }
 out:
  if (tmp_argv)
    g_ptr_array_unref (tmp_argv);
#ifdef G_OS_UNIX
  if (self->internal_stdin_fd >= 0)
    (void) close (self->internal_stdin_fd);
  if (self->internal_stdout_fd >= 0)
    (void) close (self->internal_stdout_fd);
  if (self->internal_stderr_fd >= 0)
    (void) close (self->internal_stderr_fd);
#endif
  return ret;
}

/**
 * g_subprocess_get_pid:
 * @self: a #GSubprocess
 *
 * Returns the identifier for this child process. 

 * This function may not be used if g_subprocess_set_detached() has
 * been called.
 *
 * Since: 2.34
 */
GPid
g_subprocess_get_pid (GSubprocess     *self)
{
  g_return_val_if_fail (G_IS_SUBPROCESS (self), 0);
  g_return_val_if_fail (!self->detached, 0);
  g_return_val_if_fail (self->state > G_SUBPROCESS_STATE_BUILDING, 0);
  
  return self->pid;
}

/**
 * g_subprocess_add_watch:
 * @self: a #GSubprocess
 * @function: Callback invoked when child has exited
 * @user_data: Data for @function
 *
 * This function creates a source via g_subprocess_create_source() and
 * attaches it the to the <link
 * linkend="g-main-context-push-thread-default">thread-default main
 * loop</link>.
 *
 * For more information, see the documentation of
 * g_subprocess_add_watch_full().
 *
 * Returns: (transfer full): A newly-created #GSource for this process
 * Since: 2.34
 */
GSource *
g_subprocess_add_watch (GSubprocess             *self,
			GSubprocessWatchFunc     function,
			gpointer                 user_data)
{
  return g_subprocess_add_watch_full (self, G_PRIORITY_DEFAULT, function, user_data, NULL);
}

typedef struct {
  GSubprocess *self;
  GSubprocessWatchFunc *callback;
  gpointer user_data;
  GDestroyNotify notify;
  gboolean have_wnowait;
} GSubprocessWatchTrampolineData;

static void
g_subprocess_child_watch_func (GPid       pid,
			       gint       status_code,
			       gpointer   user_data)
{
  GSubprocessWatchTrampolineData *data = user_data;

  data->self->status_code = status_code;
  data->self->state = G_SUBPROCESS_STATE_TERMINATED;

  if (!data->have_wnowait)
    data->self->reaped_child = TRUE;
  
  if (data->callback)
    data->callback (data->self, data->user_data);
}

static void
g_subprocess_trampoline_data_destroy (gpointer user_data)
{
  GSubprocessWatchTrampolineData *data = user_data;

  if (data->notify)
    data->notify (data->user_data);
  
  g_object_unref (data->self);
  g_free (data);
}

/**
 * g_subprocess_add_watch_full:
 * @self: a #GSubprocess
 * @priority: I/O priority
 * @function: Callback invoked when child has exited
 * @user_data: Data for @function
 * @notify: Destroy notify
 *
 * This function creates a source via g_subprocess_create_source() and
 * attaches it the to the <link
 * linkend="g-main-context-push-thread-default">thread-default main
 * loop</link>.
 *
 * Inside the callback, you should call either
 * g_subprocess_query_success() or g_subprocess_get_exit_code() to
 * determine the status of the child.
 *
 * This function may not be used if g_subprocess_set_detached() has
 * been called.
 *
 * Returns: (transfer full): A newly-created #GSource for this process
 * Since: 2.34
 */
GSource *
g_subprocess_add_watch_full (GSubprocess             *self,
			     gint                     priority,
			     GSubprocessWatchFunc     function,
			     gpointer                 user_data,
			     GDestroyNotify           notify)
{
  GSource *source;

  source = g_subprocess_create_source (self, priority, function, user_data, notify);
  g_source_attach (source, g_main_context_get_thread_default ());

  return source;
}

/**
 * g_subprocess_create_source:
 * @self: a #GSubprocess
 * @priority: I/O priority
 * @function: Callback invoked when child has exited
 * @user_data: Data for @function
 * @notify: Destroy notify
 *
 * This function is similar to g_child_watch_source_new(), except the
 * callback signature includes the subprocess @self, and status is
 * accessed via g_subprocess_query_success().
 *
 * This function may not be used if g_subprocess_set_detached() has
 * been called.
 *
 * Returns: (transfer full): A newly-created #GSource for this process
 * Since: 2.34
 */
GSource *
g_subprocess_create_source (GSubprocess             *self,
			    gint                     priority,
			    GSubprocessWatchFunc     function,
			    gpointer                 user_data,
			    GDestroyNotify           notify)
{
  GSource *source;
  GSubprocessWatchTrampolineData *trampoline_data;

  g_return_val_if_fail (G_IS_SUBPROCESS (self), 0);
  g_return_val_if_fail (self->state == G_SUBPROCESS_STATE_RUNNING, 0);
  g_return_val_if_fail (!self->detached, 0);

  source = GLIB_PRIVATE_CALL (g_child_watch_source_new_with_flags) (self->pid, _G_CHILD_WATCH_FLAGS_WNOWAIT);
  if (source == NULL)
    {
      source = g_child_watch_source_new (self->pid);
      trampoline_data->have_wnowait = FALSE;
    }
  else
    {
      trampoline_data->have_wnowait = TRUE;
    }
  g_source_set_priority (source, priority);
  trampoline_data = g_new (GSubprocessWatchTrampolineData, 1);
  trampoline_data->self = g_object_ref (self);
  trampoline_data->callback = function;
  trampoline_data->user_data = user_data;
  trampoline_data->notify = notify;
  g_source_set_callback (source, (GSourceFunc)g_subprocess_child_watch_func, trampoline_data,
			 g_subprocess_trampoline_data_destroy);

  return source;
}

/**
 * g_subprocess_query_success:
 * @self: a #GSubprocess
 * @error: a #GError
 *
 * This function sets @error based on the exit code if the child exits
 * abnormally (e.g. with a nonzero exit code, or via a fatal signal).
 * This contrasts with the lower-level GLib API of g_child_watch_add()
 * where callers must use platform-specific macros such as the Unix
 * WIFEXITED() macro on the exit code.
 *
 * In the case where the child exits abnormally, the resulting @error
 * will have domain %G_IO_ERROR, code %G_IO_ERROR_SUBPROCESS_EXIT_ABNORMAL.
 *
 * You can query the actual exit status via
 * g_subprocess_get_exit_code().
 *
 * It is invalid to call this function unless the child has actually
 * terminated.  You can wait for the child to exit via
 * g_subprocess_add_watch(), or synchronously via
 * g_subprocess_wait_sync().
 *
 * Returns: %TRUE if child exited successfully, %FALSE otherwise
 * Since: 2.34
 */
gboolean
g_subprocess_query_success (GSubprocess   *self,
			    GError       **error)
{
  gboolean ret = FALSE;

  g_return_val_if_fail (G_IS_SUBPROCESS (self), FALSE);
  g_return_val_if_fail (!self->detached, FALSE);
  g_return_val_if_fail (self->state == G_SUBPROCESS_STATE_TERMINATED, FALSE);

  if (self->internal_error)
    {
      if (error)
	*error = g_error_copy (self->internal_error);
      return FALSE;
    }

#ifdef G_OS_UNIX
  if (WIFEXITED (self->status_code))
    {
      if (WEXITSTATUS (self->status_code) != 0)
	{
	  g_set_error (error, G_IO_ERROR, G_IO_ERROR_SUBPROCESS_EXIT_ABNORMAL,
		       _("Child process %ld exited with code %ld"),
		       (long) self->pid, (long) self->status_code);
	  goto out;
	}
    }
  else if (WIFSIGNALED (self->status_code))
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_SUBPROCESS_EXIT_ABNORMAL,
		   _("Child process %ld killed by signal %ld"),
		   (long) self->pid, (long) WTERMSIG (self->status_code));
      goto out;
    }
  else if (WIFSTOPPED (self->status_code))
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_SUBPROCESS_EXIT_ABNORMAL,
		   _("Child process %ld stopped by signal %ld"),
		   (long) self->pid, (long) WSTOPSIG (self->status_code));
      goto out;
    }
  else
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_SUBPROCESS_EXIT_ABNORMAL,
		   _("Child process %ld exited abnormally"),
		   (long) self->pid);
      goto out;
    }
#else
  if (self->status_code != 0)
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_SUBPROCESS_EXIT_ABNORMAL,
		   _("Child process %ld exited abnormally"),
		   (long) self->pid);
      goto out;
    }
#endif
      
  ret = TRUE;
 out:
  return ret;
}

/**
 * g_subprocess_get_status_code:
 * @self: a #GSubprocess
 *
 * Returns an integer with platform-specific semantics representing
 * the process status, in the same form as provided by
 * g_spawn_async_with_pipes().  In the typical case where you simply
 * want an error set if the child exited abnormally, use
 * g_subprocess_query_success().
 *
 * It is invalid to call this function unless the child has actually
 * terminated.  You can wait for the child to exit via
 * g_subprocess_add_watch(), or synchronously via
 * g_subprocess_wait_sync().
 *
 * Returns: Exit code of child
 * Since: 2.34
 */
gint
g_subprocess_get_status_code (GSubprocess   *self)
{
  g_return_val_if_fail (G_IS_SUBPROCESS (self), FALSE);
  g_return_val_if_fail (!self->detached, FALSE);
  g_return_val_if_fail (self->state == G_SUBPROCESS_STATE_TERMINATED, FALSE);

  return self->status_code;
}

static void
g_subprocess_on_sync_watch (GSubprocess   *self,
			    gpointer       user_data)
{
  GMainLoop *loop = user_data;

  g_main_loop_quit (loop);
}

/**
 * g_subprocess_wait_sync:
 * @self: a #GSubprocess
 * @cancellable: a #GCancellable
 * @error: a #GError
 *
 * Synchronously wait for the subprocess to terminate.  This function
 * will also invoke g_subprocess_query_success(), meaning that by
 * default @error will be set if the subprocess exits abnormally.
 * 
 * Returns: %TRUE if child exited successfully, %FALSE on
 *   non-successful status or @cancellable was cancelled
 * Since: 2.34
 */
gboolean
g_subprocess_wait_sync (GSubprocess        *self,
			GCancellable       *cancellable,
			GError            **error)
{
  gboolean ret = FALSE;
  gboolean pushed_thread_default = FALSE;
  GMainContext *context = NULL;
  GMainLoop *loop = NULL;
  GSource *source = NULL;
  GSource *cancellable_source = NULL;

  g_return_val_if_fail (G_IS_SUBPROCESS (self), FALSE);
  g_return_val_if_fail (!self->detached, FALSE);
  g_return_val_if_fail (self->state == G_SUBPROCESS_STATE_RUNNING, FALSE);

  if (g_cancellable_set_error_if_cancelled (cancellable, error))
    return FALSE;

  context = g_main_context_new ();
  g_main_context_push_thread_default (context);
  pushed_thread_default = TRUE;
  loop = g_main_loop_new (context, TRUE);
  
  source = g_subprocess_add_watch (self, g_subprocess_on_sync_watch, loop);
  cancellable_source = g_cancellable_source_new (cancellable);
  g_source_add_child_source (source, cancellable_source);
  g_source_unref (cancellable_source);

  g_main_loop_run (loop);

  if (!g_subprocess_query_success (self, error))
    goto out;

  ret = TRUE;
 out:
  if (pushed_thread_default)
    g_main_context_pop_thread_default (context);
  if (source)
    g_source_unref (source);
  if (context)
    g_main_context_unref (context);
  if (loop)
    g_main_loop_unref (loop);

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
 * This function may not be used if g_subprocess_set_detached() has
 * been called.
 *
 * Returns: %TRUE if the operation is supported, %FALSE otherwise.
 */
gboolean
g_subprocess_request_exit (GSubprocess       *self)
{
  g_return_val_if_fail (G_IS_SUBPROCESS (self), FALSE);
  g_return_val_if_fail (!self->detached, FALSE);
  g_return_val_if_fail (self->state > G_SUBPROCESS_STATE_BUILDING, FALSE);

  if (self->state == G_SUBPROCESS_STATE_TERMINATED)
    return TRUE;

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
 * forceful termination of the process.
 *
 * On Unix, this function sends %SIGKILL.
 *
 * You can use g_subprocess_add_watch() to monitor the status of the
 * process after calling this function.
 *
 * This function may not be used if g_subprocess_set_detached() has
 * been called.
 */
void             
g_subprocess_force_exit (GSubprocess       *self)
{
  g_return_if_fail (G_IS_SUBPROCESS (self));
  g_return_if_fail (!self->detached);
  g_return_if_fail (self->state > G_SUBPROCESS_STATE_BUILDING);

  if (self->state == G_SUBPROCESS_STATE_TERMINATED)
    return;

#ifdef G_OS_UNIX
  (void) kill (self->pid, SIGKILL);
#else
  TerminateProcess (self->pid, 1);
#endif
}

/**** High level wrapers ****/

typedef struct {
  GSubprocess *self;
  gboolean caught_error;
  GError *error;
  GMainLoop *loop;
  guint events_needed;
} GSubprocessRunSyncGetOutputData;

static void
g_subprocess_on_get_output_splice_done (GObject      *obj,
					GAsyncResult *res,
					gpointer      user_data)
{
  GSubprocessRunSyncGetOutputData *data = user_data;

  if (!data->caught_error)
    {
      if (g_output_stream_splice_finish ((GOutputStream*)obj, res, &data->error) < 0)
	data->caught_error = TRUE;
    }
  
  data->events_needed--;
  if (data->events_needed == 0)
    g_main_loop_quit (data->loop);
}

static gboolean
run_sync_get_output_membufs (GSubprocess               *self,
			     GOutputStreamSpliceFlags   flags,
			     GMemoryOutputStream      **out_stdout_buf,
			     GMemoryOutputStream      **out_stderr_buf,
			     GCancellable              *cancellable,
			     GError                   **error)
{
  gboolean ret = FALSE;
  GSubprocessRunSyncGetOutputData data;
  gboolean pushed_thread_default;
  GMainContext *context = NULL;
  GInputStream *subproc_stdout = NULL;
  GInputStream *subproc_stderr = NULL;
  GOutputStream *stdout_membuf = NULL;
  GOutputStream *stderr_membuf = NULL;

  if (out_stdout_buf)
    stdout_membuf = g_memory_output_stream_new (NULL, 0, g_realloc, g_free);
  if (out_stderr_buf)
    stderr_membuf = g_memory_output_stream_new (NULL, 0, g_realloc, g_free);

  memset (&data, 0, sizeof (data));
  data.error = NULL;
  data.self = self;
  
  context = g_main_context_new ();
  g_main_context_push_thread_default (context);
  pushed_thread_default = TRUE;
  data.loop = g_main_loop_new (context, TRUE);

  if (!g_subprocess_start_with_pipes (self, NULL, out_stdout_buf ? &subproc_stdout : NULL,
				      out_stderr_buf ? &subproc_stderr : NULL,
				      cancellable, error))
    goto out;

  if (subproc_stdout)
    {
      g_output_stream_splice_async (stdout_membuf, subproc_stdout, flags,
				    self->io_priority, cancellable,
				    g_subprocess_on_get_output_splice_done, &data);
      g_object_unref (subproc_stdout);
      data.events_needed++;
    }

  if (subproc_stderr)
    {
      g_output_stream_splice_async (stderr_membuf, subproc_stderr, flags,
				    self->io_priority, cancellable,
				    g_subprocess_on_get_output_splice_done, &data);
      g_object_unref (subproc_stderr);
      data.events_needed++;
    }

  g_main_loop_run (data.loop);

  if (data.caught_error)
    {
      g_propagate_error (error, data.error);
      goto out;
    }

  /* Note minor optimization opportunity: we create two main loops
   * presently */
  if (!g_subprocess_wait_sync (self, cancellable, error))
    goto out;

  ret = TRUE;
  if (out_stdout_buf)
    {
      *out_stdout_buf = (GMemoryOutputStream*)stdout_membuf;
      stdout_membuf = NULL;
    }
  if (out_stderr_buf)
    {
      *out_stderr_buf = (GMemoryOutputStream*)stderr_membuf;
      stderr_membuf = NULL;
    }
 out:
  g_clear_object (&stdout_membuf);
  g_clear_object (&stderr_membuf);
  if (pushed_thread_default)
    g_main_context_pop_thread_default (context);
  if (context)
    g_main_context_unref (context);
  if (data.loop)
    g_main_loop_unref (data.loop);

  return ret;
}

/**
 * g_subprocess_run_sync_get_output_bytes:
 * @self: a #GSubprocess
 * @out_stdout_bytes: (out) (transfer full) (allow-none): Standard output from child process
 * @out_stderr_bytes: (out) (transfer full) (allow-none): Standard error from child process
 * @cancellable: a #GCancellable
 * @error: a #GError
 *
 * Synchronously run the child process, gathering output from standard
 * output and/or standard error into the returned buffers.  To get them
 * both into one stream, use g_subprocess_set_standard_error_to_stdout().
 *
 * If @error is set for any reason (including the child exiting
 * unsuccessfully), then @out_stdout_bytes and @out_stderr_bytes will
 * be left uninitialized.  If you want to run the child process while
 * gathering output regardless of exit status, you should use the
 * lower-level function g_subprocess_start_with_pipes().
 *
 * Returns: %TRUE if child process exited, successfully %FALSE otherwise
 * Since: 2.34
 */
gboolean
g_subprocess_run_sync_get_output_bytes (GSubprocess          *self,
					GBytes              **out_stdout_bytes,
					GBytes              **out_stderr_bytes,
					GCancellable         *cancellable,
					GError              **error)
{
  gboolean ret = FALSE;
  GMemoryOutputStream *stdout_membuf = NULL;
  GMemoryOutputStream *stderr_membuf = NULL;
  const gint flags = G_OUTPUT_STREAM_SPLICE_CLOSE_TARGET | G_OUTPUT_STREAM_SPLICE_CLOSE_SOURCE;

  if (!run_sync_get_output_membufs (self, flags,
				    out_stdout_bytes ? &stdout_membuf : NULL,
				    out_stderr_bytes ? &stderr_membuf : NULL,
				    cancellable, error))
    goto out;

  if (stdout_membuf && !g_output_stream_close ((GOutputStream*) stdout_membuf,
					       cancellable, error))
    goto out;
  if (stderr_membuf && !g_output_stream_close ((GOutputStream*) stderr_membuf,
					       cancellable, error))
    goto out;

  ret = TRUE;
  if (out_stdout_bytes)
    *out_stdout_bytes = g_memory_output_stream_steal_as_bytes (stdout_membuf);
  if (out_stderr_bytes)
    *out_stderr_bytes = g_memory_output_stream_steal_as_bytes (stderr_membuf);
 out:
  g_clear_object (&stdout_membuf);
  g_clear_object (&stderr_membuf);
  return ret;
}

/**
 * g_subprocess_run_sync_get_stdout_utf8:
 * @self: A #GSubprocess
 * @output_utf8: (out) (transfer full) (allow-none): Output string
 * @cancellable: a #GCancellable
 * @error: a #GError
 *
 * Synchronously run the child process, gathering complete output into
 * the returned @output_utf8 string.  By default, stderr will go to
 * the current process' stderr, but you may redirect it normally via
 * g_subprocess_set_standard_error_to_devnull().
 *
 * Returns: %TRUE on success, %FALSE otherwise
 * Since: 2.34
 */
gboolean
g_subprocess_run_sync_get_stdout_utf8 (GSubprocess   *self,
				       gchar        **output_utf8,
				       GCancellable  *cancellable,
				       GError       **error)
{
  gboolean ret = FALSE;
  gsize bytes_written;
  GMemoryOutputStream *buf = NULL;

  if (!run_sync_get_output_membufs (self, G_OUTPUT_STREAM_SPLICE_CLOSE_SOURCE,
				    &buf, NULL, cancellable, error))
    goto out;

  /* Ensure it's NUL terminated */
  if (!g_output_stream_write_all (G_OUTPUT_STREAM (buf), "", 1, &bytes_written,
				  cancellable, error))
    goto out;

  if (!g_output_stream_close (G_OUTPUT_STREAM (buf), cancellable, error))
    goto out;

  if (!g_utf8_validate (g_memory_output_stream_get_data (buf),
			-1, NULL))
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_INVALID_DATA,
		   _("Subprocess output was invalid UTF-8"));
      goto out;
    }

  ret = TRUE;
  if (output_utf8)
    *output_utf8 = g_memory_output_stream_steal_data (buf);
 out:
  g_clear_object (&buf);
  return ret;
}
