#include <gio/gio.h>
#include <string.h>

#ifdef G_OS_UNIX
#include <sys/wait.h>
#include <gio/gfiledescriptorbased.h>
#endif

static GPtrArray *
get_test_subprocess_args (const char *mode,
			  ...) G_GNUC_NULL_TERMINATED;

static GPtrArray *
get_test_subprocess_args (const char *mode,
			  ...)
{
  GPtrArray *ret;
  char *cwd;
  char *cwd_path;
  const char *binname;
  va_list args;
  gpointer arg;

  ret = g_ptr_array_new_with_free_func (g_free);

  cwd = g_get_current_dir ();

#ifdef G_OS_WIN32
  binname = "gsubprocess-testprog.exe";
#else
  binname = "gsubprocess-testprog";
#endif

  cwd_path = g_build_filename (cwd, binname, NULL);
  g_free (cwd);
  g_ptr_array_addv (ret, cwd_path, g_strdup (mode), NULL);

  va_start (args, mode);
  while ((arg = va_arg (args, gpointer)) != NULL)
    g_ptr_array_add (ret, g_strdup (arg));
  va_end (args);

  g_ptr_array_add (ret, NULL);
  return ret;
}

static void
test_noop (void)
{
  GError *local_error = NULL;
  GError **error = &local_error;
  GPtrArray *args;
  GSubprocess *proc;

  args = get_test_subprocess_args ("noop", NULL);
  proc = g_subprocess_new ((char**) args->pdata, NULL, NULL, 0, NULL, NULL,
			   g_subprocess_stream_devnull (),
			   g_subprocess_stream_inherit (),
			   g_subprocess_stream_inherit (),
			   error);
  g_ptr_array_free (args, TRUE);
  g_assert_no_error (local_error);

  (void)g_subprocess_wait_sync_check (proc, NULL, error);
  g_assert_no_error (local_error);
  
  g_object_unref (proc);
}

static void
test_noop_all_to_null (void)
{
  GError *local_error = NULL;
  GError **error = &local_error;
  GPtrArray *args;
  GSubprocess *proc;

  args = get_test_subprocess_args ("noop", NULL);
  proc = g_subprocess_new ((char**) args->pdata, NULL, NULL, 0, NULL, NULL,
			   g_subprocess_stream_devnull (),
			   g_subprocess_stream_devnull (),
			   g_subprocess_stream_devnull (),
			   error);
  g_ptr_array_free (args, TRUE);
  g_assert_no_error (local_error);

  (void)g_subprocess_wait_sync_check (proc, NULL, error);
  g_assert_no_error (local_error);
  
  g_object_unref (proc);
}

static void
test_noop_no_wait (void)
{
  GError *local_error = NULL;
  GError **error = &local_error;
  GPtrArray *args;
  GSubprocess *proc;

  args = get_test_subprocess_args ("noop", NULL);
  proc = g_subprocess_new ((char**) args->pdata, NULL, NULL, 0, NULL, NULL,
			   g_subprocess_stream_devnull (),
			   g_subprocess_stream_inherit (),
			   g_subprocess_stream_inherit (),
			   error);
  g_ptr_array_free (args, TRUE);
  g_assert_no_error (local_error);
  
  g_object_unref (proc);
}

static void
test_noop_stdin_inherit (void)
{
  GError *local_error = NULL;
  GError **error = &local_error;
  GPtrArray *args;
  GSubprocess *proc;

  args = get_test_subprocess_args ("noop", NULL);
  proc = g_subprocess_new ((char**) args->pdata, NULL, NULL, 0, NULL, NULL,
			   g_subprocess_stream_inherit (),
			   g_subprocess_stream_inherit (),
			   g_subprocess_stream_inherit (),
			   error);
  g_ptr_array_free (args, TRUE);
  g_assert_no_error (local_error);

  (void)g_subprocess_wait_sync_check (proc, NULL, error);
  g_assert_no_error (local_error);
  
  g_object_unref (proc);
}


#ifdef G_OS_UNIX
static void
test_search_path (void)
{
  GError *local_error = NULL;
  GError **error = &local_error;
  GPtrArray *args;
  GSubprocess *proc;

  args = g_ptr_array_new ();
  g_ptr_array_add (args, "true");
  g_ptr_array_add (args, NULL);
  proc = g_subprocess_new ((char**) args->pdata, NULL, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL,
			   g_subprocess_stream_inherit (),
			   g_subprocess_stream_inherit (),
			   g_subprocess_stream_inherit (),
			   error);
  g_ptr_array_free (args, TRUE);
  g_assert_no_error (local_error);

  (void)g_subprocess_wait_sync_check (proc, NULL, error);
  g_assert_no_error (local_error);
  
  g_object_unref (proc);
}
#endif

static void
test_exit1 (void)
{
  GError *local_error = NULL;
  GError **error = &local_error;
  GPtrArray *args;
  GSubprocess *proc;

  args = get_test_subprocess_args ("exit1", NULL);
  proc = g_subprocess_new ((char**) args->pdata, NULL, NULL, 0, NULL, NULL,
			   g_subprocess_stream_devnull (),
			   g_subprocess_stream_inherit (),
			   g_subprocess_stream_inherit (),
			   error);
  g_ptr_array_free (args, TRUE);
  g_assert_no_error (local_error);

  (void)g_subprocess_wait_sync_check (proc, NULL, error);
  g_assert_error (local_error, G_SPAWN_EXIT_ERROR, 1);
  g_clear_error (error);

  g_object_unref (proc);
}

static gchar *
splice_to_string (GInputStream   *stream,
		  GError        **error)
{
  GMemoryOutputStream *buffer = NULL;
  char *ret = NULL;

  buffer = (GMemoryOutputStream*)g_memory_output_stream_new (NULL, 0, g_realloc, g_free);
  if (g_output_stream_splice ((GOutputStream*)buffer, stream, 0, NULL, error) < 0)
    goto out;

  if (!g_output_stream_write ((GOutputStream*)buffer, "\0", 1, NULL, error))
    goto out;

  if (!g_output_stream_close ((GOutputStream*)buffer, NULL, error))
    goto out;

  ret = g_memory_output_stream_steal_data (buffer);
 out:
  g_clear_object (&buffer);
  return ret;
}

static void
test_echo1 (void)
{
  GError *local_error = NULL;
  GError **error = &local_error;
  GSubprocess *proc;
  GPtrArray *args;
  GInputStream *stdout;
  gchar *result;

  args = get_test_subprocess_args ("echo", "hello", "world!", NULL);
  proc = g_subprocess_new ((char**) args->pdata, NULL, NULL, 0, NULL, NULL,
			   g_subprocess_stream_devnull (),
			   g_subprocess_stream_pipe (),
			   g_subprocess_stream_inherit (),
			   error);
  g_ptr_array_free (args, TRUE);
  g_assert_no_error (local_error);

  stdout = g_subprocess_get_stdout_pipe (proc);

  result = splice_to_string (stdout, error);
  g_assert_no_error (local_error);

  g_assert_cmpstr (result, ==, "hello\nworld!\n");

  g_free (result);
  g_object_unref (proc);
}

#ifdef G_OS_UNIX
static void
test_echo_merged (void)
{
  GError *local_error = NULL;
  GError **error = &local_error;
  GSubprocess *proc;
  GPtrArray *args;
  GInputStream *stdout;
  gchar *result;

  args = get_test_subprocess_args ("echo-stdout-and-stderr", "merge", "this", NULL);
  proc = g_subprocess_new ((char**) args->pdata, NULL, NULL, 0, NULL, NULL,
			   g_subprocess_stream_devnull (),
			   g_subprocess_stream_pipe (),
			   g_subprocess_stream_merge_stdout (),
			   error);
  g_ptr_array_free (args, TRUE);
  g_assert_no_error (local_error);

  stdout = g_subprocess_get_stdout_pipe (proc);
  result = splice_to_string (stdout, error);
  g_assert_no_error (local_error);

  g_assert_cmpstr (result, ==, "merge\nmerge\nthis\nthis\n");

  g_free (result);
  g_object_unref (proc);
}
#endif

typedef struct {
  guint events_pending;
  GMainLoop *loop;
} TestCatData;

static void
test_cat_on_input_splice_complete (GObject      *object,
				   GAsyncResult *result,
				   gpointer      user_data)
{
  TestCatData *data = user_data;
  GError *error = NULL;

  (void)g_output_stream_splice_finish ((GOutputStream*)object, result, &error);
  g_assert_no_error (error);

  data->events_pending--;
  if (data->events_pending == 0)
    g_main_loop_quit (data->loop);
}

static void
test_cat_utf8 (void)
{
  GError *local_error = NULL;
  GError **error = &local_error;
  GSubprocess *proc;
  GPtrArray *args;
  GBytes *input_buf;
  GBytes *output_buf;
  GInputStream *input_buf_stream = NULL;
  GOutputStream *output_buf_stream = NULL;
  GOutputStream *stdin_stream = NULL;
  GInputStream *stdout_stream = NULL;
  TestCatData data;

  memset (&data, 0, sizeof (data));
  data.loop = g_main_loop_new (NULL, TRUE);

  args = get_test_subprocess_args ("cat", NULL);
  proc = g_subprocess_new ((char**) args->pdata, NULL, NULL, 0, NULL, NULL,
			   g_subprocess_stream_pipe (),
			   g_subprocess_stream_pipe (),
			   g_subprocess_stream_inherit (),
			   error);
  g_ptr_array_free (args, TRUE);
  g_assert_no_error (local_error);

  stdin_stream = g_subprocess_get_stdin_pipe (proc);
  stdout_stream = g_subprocess_get_stdout_pipe (proc);

  input_buf = g_bytes_new_static ("hello, world!", strlen ("hello, world!"));
  input_buf_stream = g_memory_input_stream_new_from_bytes (input_buf);
  g_bytes_unref (input_buf);

  output_buf_stream = g_memory_output_stream_new (NULL, 0, g_realloc, g_free);

  g_output_stream_splice_async (stdin_stream, input_buf_stream, G_OUTPUT_STREAM_SPLICE_CLOSE_SOURCE | G_OUTPUT_STREAM_SPLICE_CLOSE_TARGET,
				G_PRIORITY_DEFAULT, NULL, test_cat_on_input_splice_complete,
				&data);
  data.events_pending++;
  g_output_stream_splice_async (output_buf_stream, stdout_stream, G_OUTPUT_STREAM_SPLICE_CLOSE_SOURCE | G_OUTPUT_STREAM_SPLICE_CLOSE_TARGET,
				G_PRIORITY_DEFAULT, NULL, test_cat_on_input_splice_complete,
				&data);
  data.events_pending++;
  
  g_main_loop_run (data.loop);

  g_subprocess_wait_sync_check (proc, NULL, error);
  g_assert_no_error (local_error);

  output_buf = g_memory_output_stream_steal_as_bytes ((GMemoryOutputStream*)output_buf_stream);
  
  g_assert_cmpint (g_bytes_get_size (output_buf), ==, 13);
  g_assert_cmpint (memcmp (g_bytes_get_data (output_buf, NULL), "hello, world!", 13), ==, 0);

  g_bytes_unref (output_buf);
  g_main_loop_unref (data.loop);
  g_object_unref (input_buf_stream);
  g_object_unref (output_buf_stream);
  g_object_unref (proc);
}

#ifdef G_OS_UNIX
static void
test_file_redirection (void)
{
  GError *local_error = NULL;
  GError **error = &local_error;
  char *expected_buf = NULL;
  GSubprocess *proc;
  GPtrArray *args;
  GBytes *input_buf;
  GBytes *output_buf;
  GInputStream *input_buf_stream = NULL;
  GOutputStream *output_buf_stream = NULL;
  GOutputStream *stdin_stream = NULL;
  GInputStream *stdout_stream = NULL;
  GFile *temp_file = NULL;
  GFileIOStream *temp_file_io = NULL;
  TestCatData data;

  memset (&data, 0, sizeof (data));
  data.loop = g_main_loop_new (NULL, TRUE);

  expected_buf = g_strdup_printf ("this is a test file, written by pid:%lu at monotonic time:%" G_GUINT64_FORMAT,
				  (gulong) getpid (), g_get_monotonic_time ());
  
  temp_file = g_file_new_tmp ("gsubprocess-tmpXXXXXX",
			      &temp_file_io, error);
  g_assert_no_error (local_error);

  g_io_stream_close ((GIOStream*)temp_file_io, NULL, error);
  g_assert_no_error (local_error);

  args = get_test_subprocess_args ("cat", NULL);
  proc = g_subprocess_new ((char**) args->pdata, NULL, NULL, 0, NULL, NULL,
			   g_subprocess_stream_pipe (),
			   (GObject*) temp_file,
			   g_subprocess_stream_inherit (),
			   error);
  g_ptr_array_free (args, TRUE);
  g_assert_no_error (local_error);

  stdin_stream = g_subprocess_get_stdin_pipe (proc);

  input_buf = g_bytes_new_take (expected_buf, strlen (expected_buf));
  expected_buf = NULL;
  input_buf_stream = g_memory_input_stream_new_from_bytes (input_buf);

  g_output_stream_splice_async (stdin_stream, input_buf_stream, G_OUTPUT_STREAM_SPLICE_CLOSE_SOURCE | G_OUTPUT_STREAM_SPLICE_CLOSE_TARGET,
				G_PRIORITY_DEFAULT, NULL, test_cat_on_input_splice_complete,
				&data);
  data.events_pending++;
  
  g_main_loop_run (data.loop);

  g_subprocess_wait_sync_check (proc, NULL, error);
  g_assert_no_error (local_error);

  g_object_unref (proc);
  g_object_unref (input_buf_stream);

  args = get_test_subprocess_args ("cat", NULL);
  proc = g_subprocess_new ((char**) args->pdata, NULL, NULL, 0, NULL, NULL,
			   (GObject*)temp_file,
			   g_subprocess_stream_pipe (),
			   g_subprocess_stream_inherit (),
			   error);
  g_ptr_array_free (args, TRUE);
  g_assert_no_error (local_error);

  stdout_stream = g_subprocess_get_stdout_pipe (proc);
  output_buf_stream = g_memory_output_stream_new (NULL, 0, g_realloc, g_free);

  g_output_stream_splice_async (output_buf_stream, stdout_stream, G_OUTPUT_STREAM_SPLICE_CLOSE_SOURCE | G_OUTPUT_STREAM_SPLICE_CLOSE_TARGET,
				G_PRIORITY_DEFAULT, NULL, test_cat_on_input_splice_complete,
				&data);
  data.events_pending++;
  
  g_main_loop_run (data.loop);

  output_buf = g_memory_output_stream_steal_as_bytes ((GMemoryOutputStream*)output_buf_stream);
  
  g_assert_cmpint (g_bytes_get_size (output_buf), ==, g_bytes_get_size (input_buf));
  g_assert_cmpint (memcmp (g_bytes_get_data (output_buf, NULL),
			   g_bytes_get_data (input_buf, NULL), g_bytes_get_size (input_buf)), ==, 0);

  g_bytes_unref (input_buf);
  g_bytes_unref (output_buf);
  g_main_loop_unref (data.loop);
  g_object_unref (output_buf_stream);
  g_object_unref (proc);
  
  g_file_delete (temp_file, NULL, error);
  g_assert_no_error (local_error);
  g_object_unref (temp_file);
  g_object_unref (temp_file_io);
}

static void
test_unix_fd_passing (void)
{
  GError *local_error = NULL;
  GError **error = &local_error;
  GSubprocess *proc;
  GPtrArray *args;
  GOutputStream *stdin;
  GFile *temp_file = NULL;
  GFileIOStream *temp_file_io = NULL;
  GOutputStream *temp_file_out = NULL;
  gsize bytes_written;
  gchar *write_buf;
  gchar *read_buf;

  temp_file = g_file_new_tmp ("gsubprocess-tmpXXXXXX",
			      &temp_file_io, error);

  temp_file_out = g_io_stream_get_output_stream ((GIOStream*)temp_file_io);
  g_assert (G_IS_FILE_DESCRIPTOR_BASED (temp_file_out));

  args = get_test_subprocess_args ("cat", NULL);
  proc = g_subprocess_new ((char**) args->pdata, NULL, NULL, 0, NULL, NULL,
			   g_subprocess_stream_pipe (),
			   (GObject*)temp_file_out,
			   g_subprocess_stream_inherit (),
			   error);
  g_ptr_array_free (args, TRUE);
  g_assert_no_error (local_error);

  g_io_stream_close ((GIOStream*)temp_file_io, NULL, error);
  g_assert_no_error (local_error);

  stdin = g_subprocess_get_stdin_pipe (proc);
  
  write_buf = g_strdup_printf ("fd-passing timestamp:%" G_GUINT64_FORMAT,
			       g_get_monotonic_time ());
  g_output_stream_write_all (stdin, write_buf, strlen (write_buf), &bytes_written,
			     NULL, error);
  g_assert_no_error (local_error);

  g_output_stream_close (stdin, NULL, error);
  g_assert_no_error (local_error);

  g_subprocess_wait_sync_check (proc, NULL, error);
  g_assert_no_error (local_error);

  g_file_load_contents (temp_file, NULL, &read_buf, &bytes_written, NULL, error);
  g_assert_no_error (local_error);

  g_assert_cmpstr (read_buf, ==, write_buf);

  g_free (read_buf);
  g_free (write_buf);
  g_object_unref (proc);

  g_file_delete (temp_file, NULL, error);
  g_assert_no_error (local_error);
  g_object_unref (temp_file);
  g_object_unref (temp_file_io);
}
#endif

typedef struct {
  guint events_pending;
  gboolean caught_error;
  GError *error;
  GMainLoop *loop;

  gint counter;
  GOutputStream *first_stdin;
} TestMultiSpliceData;

static void
on_one_multi_splice_done (GObject       *obj,
			  GAsyncResult  *res,
			  gpointer       user_data)
{
  TestMultiSpliceData *data = user_data;

  if (!data->caught_error)
    {
      if (g_output_stream_splice_finish ((GOutputStream*)obj, res, &data->error) < 0)
	data->caught_error = TRUE;
    }

  data->events_pending--;
  if (data->events_pending == 0)
    g_main_loop_quit (data->loop);
}

static gboolean
on_idle_multisplice (gpointer     user_data)
{
  TestMultiSpliceData *data = user_data;

  /* We write 2^1 + 2^2 ... + 2^10 or 2047 copies of "Hello World!\n"
   * ultimately
   */
  if (data->counter >= 2047 || data->caught_error)
    {
      if (!g_output_stream_close (data->first_stdin, NULL, &data->error))
	data->caught_error = TRUE;
      data->events_pending--;
      if (data->events_pending == 0)
	{
	  g_main_loop_quit (data->loop);
	}
      return FALSE;
    }
  else
    {
      int i;
      for (i = 0; i < data->counter; i++)
	{
	  gsize bytes_written;
	  if (!g_output_stream_write_all (data->first_stdin, "hello world!\n",
					  strlen ("hello world!\n"), &bytes_written,
					  NULL, &data->error))
	    {
	      data->caught_error = TRUE;
	      return FALSE;
	    }
	}
      data->counter *= 2;
      return TRUE;
    }
}

static void
on_subprocess_exited (GObject         *object,
		      GAsyncResult    *result,
		      gpointer         user_data)
{
  TestMultiSpliceData *data = user_data;
  GError *error = NULL;
  int exit_status;

  if (!g_subprocess_wait_finish ((GSubprocess*)object, result, &exit_status, &error))
    {
      if (!data->caught_error)
	{
	  data->caught_error = TRUE;
	  g_propagate_error (&data->error, error);
	}
    }
  g_spawn_check_exit_status (exit_status, &error);
  g_assert_no_error (error);
  data->events_pending--;
  if (data->events_pending == 0)
    g_main_loop_quit (data->loop);
}

static void
test_multi_1 (void)
{
  GError *local_error = NULL;
  GError **error = &local_error;
  GPtrArray *args;
  GSubprocess *first;
  GSubprocess *second;
  GSubprocess *third;
  GOutputStream *first_stdin;
  GInputStream *first_stdout;
  GOutputStream *second_stdin;
  GInputStream *second_stdout;
  GOutputStream *third_stdin;
  GInputStream *third_stdout;
  GOutputStream *membuf;
  TestMultiSpliceData data;
  int splice_flags = G_OUTPUT_STREAM_SPLICE_CLOSE_SOURCE | G_OUTPUT_STREAM_SPLICE_CLOSE_TARGET;

  args = get_test_subprocess_args ("cat", NULL);
  first = g_subprocess_new ((char**) args->pdata, NULL, NULL, 0, NULL, NULL,
			    g_subprocess_stream_pipe (),
			    g_subprocess_stream_pipe (),
			    g_subprocess_stream_inherit (),
			    error);
  g_assert_no_error (local_error);
  second = g_subprocess_new ((char**) args->pdata, NULL, NULL, 0, NULL, NULL,
			     g_subprocess_stream_pipe (),
			     g_subprocess_stream_pipe (),
			     g_subprocess_stream_inherit (),
			     error);
  g_assert_no_error (local_error);
  third = g_subprocess_new ((char**) args->pdata, NULL, NULL, 0, NULL, NULL,
			    g_subprocess_stream_pipe (),
			    g_subprocess_stream_pipe (),
			    g_subprocess_stream_inherit (),
			    error);
  g_assert_no_error (local_error);

  g_ptr_array_free (args, TRUE);

  membuf = g_memory_output_stream_new (NULL, 0, g_realloc, g_free);

  first_stdin = g_subprocess_get_stdin_pipe (first);
  first_stdout = g_subprocess_get_stdout_pipe (first);
  second_stdin = g_subprocess_get_stdin_pipe (second);
  second_stdout = g_subprocess_get_stdout_pipe (second);
  third_stdin = g_subprocess_get_stdin_pipe (third);
  third_stdout = g_subprocess_get_stdout_pipe (third);

  memset (&data, 0, sizeof (data));
  data.loop = g_main_loop_new (NULL, TRUE);
  data.counter = 1;
  data.first_stdin = first_stdin;

  data.events_pending++;
  g_output_stream_splice_async (second_stdin, first_stdout, splice_flags, G_PRIORITY_DEFAULT,
				NULL, on_one_multi_splice_done, &data);
  data.events_pending++;
  g_output_stream_splice_async (third_stdin, second_stdout, splice_flags, G_PRIORITY_DEFAULT,
				NULL, on_one_multi_splice_done, &data);
  data.events_pending++;
  g_output_stream_splice_async (membuf, third_stdout, splice_flags, G_PRIORITY_DEFAULT,
				NULL, on_one_multi_splice_done, &data);

  data.events_pending++;
  g_timeout_add (250, on_idle_multisplice, &data);

  data.events_pending++;
  g_subprocess_wait (first, NULL, on_subprocess_exited, &data);
  data.events_pending++;
  g_subprocess_wait (second, NULL, on_subprocess_exited, &data);
  data.events_pending++;
  g_subprocess_wait (third, NULL, on_subprocess_exited, &data);

  g_main_loop_run (data.loop);

  g_assert (!data.caught_error);
  g_assert_no_error (data.error);

  g_assert_cmpint (g_memory_output_stream_get_data_size ((GMemoryOutputStream*)membuf), ==, 26611);

  g_main_loop_unref (data.loop);
  g_object_unref (membuf);
  g_object_unref (first);
  g_object_unref (second);
  g_object_unref (third);
}

/*
  TODO -
  This doesn't work because libtool eats our argv0 trick =/
*/
#if 0
static void
test_argv0 (void)
{
  GError *local_error = NULL;
  GError **error = &local_error;
  GSubprocess *proc;

  proc = get_test_subprocess ("assert-argv0");

  g_subprocess_set_argv0 (proc, "moocow");

  (void)g_subprocess_run_sync_check (proc, NULL, error);
  g_assert_no_error (local_error);
  
  g_object_unref (proc);
}
#endif

static gboolean
send_terminate (gpointer   user_data)
{
  GSubprocess *proc = user_data;

  g_subprocess_force_exit (proc);

  return FALSE;
}

static void
on_request_quit_exited (GObject        *object,
			GAsyncResult   *result,
			gpointer        user_data)
{
  GError *error = NULL;
  int exit_status;
  
  (void)g_subprocess_wait_finish ((GSubprocess*)object, result, &exit_status, &error);
  g_assert_no_error (error);
#ifdef G_OS_UNIX
  g_assert (WIFSIGNALED (exit_status) && WTERMSIG (exit_status) == 9);
#endif
  g_spawn_check_exit_status (exit_status, &error);
  g_assert (error != NULL);
  g_clear_error (&error);
  
  g_main_loop_quit ((GMainLoop*)user_data);
}

static void
test_terminate (void)
{
  GError *local_error = NULL;
  GError **error = &local_error;
  GSubprocess *proc;
  GPtrArray *args;
  GMainLoop *loop;

  args = get_test_subprocess_args ("sleep-forever", NULL);
  proc = g_subprocess_new ((char**) args->pdata, NULL, NULL, 0, NULL, NULL,
			   g_subprocess_stream_devnull (),
			   g_subprocess_stream_inherit (),
			   g_subprocess_stream_inherit (),
			   error);
  g_ptr_array_free (args, TRUE);
  g_assert_no_error (local_error);

  loop = g_main_loop_new (NULL, TRUE);

  g_subprocess_wait (proc, NULL, on_request_quit_exited, loop);

  g_timeout_add_seconds (3, send_terminate, proc);

  g_main_loop_run (loop);

  g_main_loop_unref (loop);
  g_object_unref (proc);
}

int
main (int argc, char **argv)
{
  g_type_init ();

  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/gsubprocess/noop", test_noop);
  g_test_add_func ("/gsubprocess/noop-all-to-null", test_noop_all_to_null);
  g_test_add_func ("/gsubprocess/noop-no-wait", test_noop_no_wait);
  g_test_add_func ("/gsubprocess/noop-stdin-inherit", test_noop_stdin_inherit);
#ifdef G_OS_UNIX
  g_test_add_func ("/gsubprocess/search-path", test_search_path);
#endif
  g_test_add_func ("/gsubprocess/exit1", test_exit1);
  g_test_add_func ("/gsubprocess/echo1", test_echo1);
#ifdef G_OS_UNIX
  g_test_add_func ("/gsubprocess/echo-merged", test_echo_merged);
#endif
  g_test_add_func ("/gsubprocess/cat-utf8", test_cat_utf8);
#ifdef G_OS_UNIX
  g_test_add_func ("/gsubprocess/file-redirection", test_file_redirection);
  g_test_add_func ("/gsubprocess/unix-fd-passing", test_unix_fd_passing);
#endif
  g_test_add_func ("/gsubprocess/multi1", test_multi_1);
  g_test_add_func ("/gsubprocess/terminate", test_terminate);
  /* g_test_add_func ("/gsubprocess/argv0", test_argv0); */

  return g_test_run ();
}
