#include <gio/gio.h>
#include <string.h>

#ifdef G_OS_UNIX
#include <sys/wait.h>
#endif

static GSubprocess *
get_test_subprocess (const char *mode)
{
  char *cwd;
  char *cwd_path;
  GSubprocess *ret;
  const char *binname;

  cwd = g_get_current_dir ();

#ifdef G_OS_WIN32
  binname = "gsubprocess-testprog.exe";
#else
  binname = "gsubprocess-testprog";
#endif

  cwd_path = g_build_filename (cwd, binname, NULL);
  ret = g_subprocess_new_with_args (cwd_path, mode, NULL);
  g_free (cwd);
  g_free (cwd_path);

  return ret;
}

static void
test_noop (void)
{
  GError *local_error = NULL;
  GError **error = &local_error;
  GSubprocess *proc;

  proc = get_test_subprocess ("noop");

  (void)g_subprocess_run_sync (proc, NULL, error);
  g_assert_no_error (local_error);
  
  g_object_unref (proc);
}

static void
test_noop_all_to_null (void)
{
  GError *local_error = NULL;
  GError **error = &local_error;
  GSubprocess *proc;

  proc = get_test_subprocess ("noop");

  g_subprocess_set_standard_input_to_devnull (proc, TRUE);
  g_subprocess_set_standard_output_to_devnull (proc, TRUE);
  g_subprocess_set_standard_error_to_devnull (proc, TRUE);

  (void)g_subprocess_run_sync (proc, NULL, error);
  g_assert_no_error (local_error);
  
  g_object_unref (proc);
}

static void
test_noop_detached (void)
{
  GError *local_error = NULL;
  GError **error = &local_error;
  GSubprocess *proc;

  proc = get_test_subprocess ("noop");

  g_subprocess_set_detached (proc, TRUE);

  (void)g_subprocess_start (proc, NULL, error);
  g_assert_no_error (local_error);
  
  g_object_unref (proc);
}

static void
test_noop_non_detached (void)
{
  GError *local_error = NULL;
  GError **error = &local_error;
  GSubprocess *proc;

  proc = get_test_subprocess ("noop");

  (void)g_subprocess_start (proc, NULL, error);
  g_assert_no_error (local_error);
  
  g_object_unref (proc);
}


#ifdef G_OS_UNIX
static void
test_search_path (void)
{
  GError *local_error = NULL;
  GError **error = &local_error;
  GSubprocess *proc;

  proc = g_subprocess_new ("true");

  g_subprocess_set_use_search_path (proc, TRUE);

  (void)g_subprocess_run_sync (proc, NULL, error);
  g_assert_no_error (local_error);
  
  g_object_unref (proc);
}
#endif

static void
test_exit1 (void)
{
  GSubprocess *proc;
  GError *local_error = NULL;
  GError **error = &local_error;

  proc = get_test_subprocess ("exit1");

  (void)g_subprocess_run_sync (proc, NULL, error);
  g_assert_error (local_error, G_IO_ERROR, G_IO_ERROR_SUBPROCESS_EXIT_ABNORMAL);
  g_clear_error (error);

#ifdef G_OS_UNIX
  {
    int scode = g_subprocess_get_status_code (proc);
    g_assert (WIFEXITED (scode) && WEXITSTATUS (scode) == 1);
  }
#endif

  g_object_unref (proc);
}

static void
test_echo1 (void)
{
  GSubprocess *proc;
  GError *local_error = NULL;
  GError **error = &local_error;
  gchar *result;

  proc = get_test_subprocess ("echo");
  
  g_subprocess_append_args (proc, "hello", "world!", NULL);

  g_subprocess_run_sync_get_stdout_utf8 (proc, &result, NULL, error);
  g_assert_no_error (local_error);

  g_assert_cmpstr (result, ==, "hello\nworld!\n");

  g_object_unref (proc);
}

#ifdef G_OS_UNIX
static void
test_echo_merged (void)
{
  GSubprocess *proc;
  GError *local_error = NULL;
  GError **error = &local_error;
  gchar *result;

  proc = get_test_subprocess ("echo-stdout-and-stderr");
  
  g_subprocess_append_args (proc, "merge", "this", NULL);
  g_subprocess_set_standard_error_to_stdout (proc, TRUE);

  g_subprocess_run_sync_get_stdout_utf8 (proc, &result, NULL, error);
  g_assert_no_error (local_error);

  g_assert_cmpstr (result, ==, "merge\nmerge\nthis\nthis\n");

  g_free (result);
  g_object_unref (proc);
}
#endif

static void
test_cat_utf8 (void)
{
  GSubprocess *proc;
  GError *local_error = NULL;
  GError **error = &local_error;
  gchar *result = NULL;

  proc = get_test_subprocess ("cat");

  g_subprocess_set_standard_input_str (proc, "hello, world!");
  g_subprocess_run_sync_get_stdout_utf8 (proc, &result, NULL, error);
  g_assert_no_error (local_error);

  g_assert_cmpstr (result, ==, "hello, world!");

  g_free (result);
  g_object_unref (proc);
}

static void
test_cat_non_utf8 (void)
{
  GSubprocess *proc;
  GError *local_error = NULL;
  GError **error = &local_error;
  gchar *result;

  proc = get_test_subprocess ("cat");

  g_subprocess_set_standard_input_str (proc, "\xFE\xFE\xFF\xFF");
  g_subprocess_run_sync_get_stdout_utf8 (proc, &result, NULL, error);
  g_assert_error (local_error, G_IO_ERROR, G_IO_ERROR_INVALID_DATA);
  g_clear_error (&local_error);

  g_object_unref (proc);
}

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
on_subprocess_exited (GSubprocess     *proc,
		      gpointer         user_data)
{
  TestMultiSpliceData *data = user_data;

  if (!data->caught_error)
    {
      if (!g_subprocess_query_success (proc, &data->error))
	data->caught_error = TRUE;
    }
  data->events_pending--;
  if (data->events_pending == 0)
    g_main_loop_quit (data->loop);
}

static void
test_multi_1 (void)
{
  GError *local_error = NULL;
  GError **error = &local_error;
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

  first = get_test_subprocess ("cat");
  second = get_test_subprocess ("cat");
  third = get_test_subprocess ("cat");

  membuf = g_memory_output_stream_new (NULL, 0, g_realloc, g_free);

  g_subprocess_start_with_pipes (first, &first_stdin, &first_stdout, NULL,
				 NULL, error);
  g_assert_no_error (local_error);
  g_subprocess_start_with_pipes (second, &second_stdin, &second_stdout, NULL,
				 NULL, error);
  g_assert_no_error (local_error);
  g_subprocess_start_with_pipes (third, &third_stdin, &third_stdout, NULL,
				 NULL, error);
  g_assert_no_error (local_error);

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
  g_subprocess_add_watch (first, G_PRIORITY_DEFAULT, on_subprocess_exited,
			  &data, NULL);
  data.events_pending++;
  g_subprocess_add_watch (second, G_PRIORITY_DEFAULT, on_subprocess_exited,
			  &data, NULL);
  data.events_pending++;
  g_subprocess_add_watch (third, G_PRIORITY_DEFAULT, on_subprocess_exited,
			  &data, NULL);

  g_main_loop_run (data.loop);

  g_assert (!data.caught_error);
  g_assert_no_error (data.error);

  g_assert_cmpint (g_memory_output_stream_get_data_size ((GMemoryOutputStream*)membuf), ==, 26611);

  g_main_loop_unref (data.loop);
  g_object_unref (first_stdin);
  g_object_unref (first_stdout);
  g_object_unref (second_stdin);
  g_object_unref (second_stdout);
  g_object_unref (third_stdin);
  g_object_unref (third_stdout);
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

  (void)g_subprocess_run_sync (proc, NULL, error);
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
on_request_quit_exited (GSubprocess   *self,
			gpointer       user_data)
{
  g_main_loop_quit ((GMainLoop*)user_data);
}

static void
test_terminate (void)
{
  GSubprocess *proc;
  GError *local_error = NULL;
  GError **error = &local_error;
  GMainLoop *loop;

  proc = get_test_subprocess ("sleep-forever");

  g_subprocess_start (proc, NULL, error);
  g_assert_no_error (local_error);

  loop = g_main_loop_new (NULL, TRUE);

  (void) g_subprocess_add_watch (proc, G_PRIORITY_DEFAULT, on_request_quit_exited,
				 loop, NULL);

  g_timeout_add_seconds (3, send_terminate, proc);

  g_main_loop_run (loop);

  (void)g_subprocess_query_success (proc, error);
  g_assert_error (local_error, G_IO_ERROR, G_IO_ERROR_SUBPROCESS_EXIT_ABNORMAL);
  g_clear_error (error);

#ifdef G_OS_UNIX
  {
    int scode = g_subprocess_get_status_code (proc);
    g_assert (WIFSIGNALED (scode) && WTERMSIG (scode) == 9);
  }
#endif

  g_object_unref (proc);
}

int
main (int argc, char **argv)
{
  g_type_init ();

  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/gsubprocess/noop", test_noop);
  g_test_add_func ("/gsubprocess/noop-all-to-null", test_noop_all_to_null);
  g_test_add_func ("/gsubprocess/noop-detached", test_noop_detached);
  g_test_add_func ("/gsubprocess/noop-nondetached", test_noop_non_detached);
#ifdef G_OS_UNIX
  g_test_add_func ("/gsubprocess/search-path", test_search_path);
#endif
  g_test_add_func ("/gsubprocess/exit1", test_exit1);
  g_test_add_func ("/gsubprocess/echo1", test_echo1);
#ifdef G_OS_UNIX
  g_test_add_func ("/gsubprocess/echo-merged", test_echo_merged);
#endif
  g_test_add_func ("/gsubprocess/cat-utf8", test_cat_utf8);
  g_test_add_func ("/gsubprocess/cat-non-utf8", test_cat_non_utf8);
  g_test_add_func ("/gsubprocess/multi1", test_multi_1);
  g_test_add_func ("/gsubprocess/terminate", test_terminate);
  /* g_test_add_func ("/gsubprocess/argv0", test_argv0); */

  return g_test_run ();
}
