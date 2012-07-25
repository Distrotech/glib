#include <gio/gio.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#ifdef G_OS_UNIX
#include <gio/gunixinputstream.h>
#include <gio/gunixoutputstream.h>
#endif

static GOptionEntry options[] = {
  {NULL}
};

static void
write_all (int           fd,
	   const guint8* buf,
	   gsize         len)
{
  while (len > 0)
    {
      ssize_t bytes_written = write (fd, buf, len);
      if (bytes_written < 0)
	g_error ("Failed to write to fd %d: %s",
		 fd, strerror (errno));
      buf += bytes_written;
      len -= bytes_written;
    }
}

static int
echo_mode (int argc,
	   char **argv)
{
  int i;

  for (i = 2; i < argc; i++)
    {
      write_all (1, (guint8*)argv[i], strlen (argv[i]));
      write_all (1, (guint8*)"\n", 1);
    }

  return 0;
}

static int
echo_stdout_and_stderr_mode (int argc,
			     char **argv)
{
  int i;

  for (i = 2; i < argc; i++)
    {
      write_all (1, (guint8*)argv[i], strlen (argv[i]));
      write_all (1, (guint8*)"\n", 1);
      write_all (2, (guint8*)argv[i], strlen (argv[i]));
      write_all (2, (guint8*)"\n", 1);
    }

  return 0;
}

static int
cat_mode (int argc,
	  char **argv)
{
  GIOChannel *chan_stdin;
  GIOChannel *chan_stdout;
  GIOStatus status;
  char buf[1024];
  gsize bytes_read, bytes_written;
  GError *local_error = NULL;
  GError **error = &local_error;

  chan_stdin = g_io_channel_unix_new (0);
  g_io_channel_set_encoding (chan_stdin, NULL, error);
  g_assert_no_error (local_error);
  chan_stdout = g_io_channel_unix_new (1);
  g_io_channel_set_encoding (chan_stdout, NULL, error);
  g_assert_no_error (local_error);

  while (TRUE)
    {
      do
	status = g_io_channel_read_chars (chan_stdin, buf, sizeof (buf),
					  &bytes_read, error);
      while (status == G_IO_STATUS_AGAIN);

      if (status == G_IO_STATUS_EOF || status == G_IO_STATUS_ERROR)
	break;

      do
	status = g_io_channel_write_chars (chan_stdout, buf, bytes_read,
					   &bytes_written, error);
      while (status == G_IO_STATUS_AGAIN);

      if (status == G_IO_STATUS_EOF || status == G_IO_STATUS_ERROR)
	break;
    }

  g_io_channel_unref (chan_stdin);
  g_io_channel_unref (chan_stdout);

  if (local_error)
    {
      g_printerr ("I/O error: %s\n", local_error->message);
      g_clear_error (&local_error);
      return 1;
    }
  return 0;
}

static gint
sleep_forever_mode (int argc,
		    char **argv)
{
  GMainLoop *loop;
  
  loop = g_main_loop_new (NULL, TRUE);
  g_main_loop_run (loop);

  return 0;
}

int
main (int argc, char **argv)
{
  GOptionContext *context;
  GError *error = NULL;
  const char *mode;

  g_type_init ();

  context = g_option_context_new ("MODE - Test GSubprocess stuff");
  g_option_context_add_main_entries (context, options, NULL);
  if (!g_option_context_parse (context, &argc, &argv, &error))
    {
      g_printerr ("%s: %s\n", argv[0], error->message);
      return 1;
    }

  if (argc < 2)
    {
      g_printerr ("MODE argument required\n");
      return 1;
    }

  mode = argv[1];
  if (strcmp (mode, "noop") == 0)
    return 0;
  else if (strcmp (mode, "exit1") == 0)
    return 1;
  else if (strcmp (mode, "assert-argv0") == 0)
    {
      if (strcmp (argv[0], "moocow") == 0)
	return 0;
      g_printerr ("argv0=%s != moocow\n", argv[0]);
      return 1;
    }
  else if (strcmp (mode, "echo") == 0)
    return echo_mode (argc, argv);
  else if (strcmp (mode, "echo-stdout-and-stderr") == 0)
    return echo_stdout_and_stderr_mode (argc, argv);
  else if (strcmp (mode, "cat") == 0)
    return cat_mode (argc, argv);
  else if (strcmp (mode, "sleep-forever") == 0)
    return sleep_forever_mode (argc, argv);
  else
    {
      g_printerr ("Unknown MODE %s\n", argv[1]);
      return 1;
    }

  return TRUE;
}
