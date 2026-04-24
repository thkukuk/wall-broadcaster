
// SPDX-License-Identifier: GPL-2.0-or-later

#include "config.h"

#include <getopt.h>
#include <gtk/gtk.h>

#include "basics.h"

static inline void g_freep(void *p)
{
  g_free(*(void**) p);
  *(void**)p = NULL;
}

static inline void g_error_freep(GError **error)
{
  if (!*error)
    return;

  g_error_free(*error);
  *error = NULL;
}

static gboolean
timeout_cb(gpointer user_data)
{
  GtkWindow *window = GTK_WINDOW(user_data);

  g_object_set_data(G_OBJECT(window), "timeout_id", GUINT_TO_POINTER(0));
  gtk_window_destroy(window);
  return G_SOURCE_REMOVE;
}

static void
close_window_cb(GtkWidget *widget, gpointer user_data _unused_)
{
  guint timeout_id = GPOINTER_TO_UINT(g_object_get_data(G_OBJECT(widget), "timeout_id"));

  // Cancel timeout if still pending
  if (timeout_id > 0)
    g_source_remove(timeout_id);
}

static void
signal_received_cb(GDBusConnection *connection _unused_,
		   const gchar *sender_name _unused_,
		   const gchar *object_path _unused_,
		   const gchar *interface_name _unused_,
		   const gchar *signal_name _unused_,
		   GVariant *parameters, gpointer user_data)
{
  GtkApplication *app = GTK_APPLICATION(user_data);
  const gchar *appname, *summary, *body, *sender;
  guchar urgency;
  const gchar *color;
  guint timeout_seconds = 0;

  if (!g_variant_check_format_string(parameters, "(sssys)", FALSE))
    {
      g_printerr("Received signal with incorrect signature. Expected (sssys).\n");
      return;
    }

  g_variant_get(parameters, "(&s&s&sy&s)", &appname, &summary, &body, &urgency, &sender);

  switch (urgency)
    {
    case 0:
      color = "green";
      timeout_seconds = 30;
      break;
    case 2:
      color = "red";
      timeout_seconds = 0;   // No timeout
      break;
    case 1:
    default:
      color = "black";
      timeout_seconds = 60;
      break;
    }

  GtkWidget *window = gtk_application_window_new(app);
  gtk_window_set_title(GTK_WINDOW(window), "System Broadcast");
  gtk_window_set_default_size(GTK_WINDOW(window), 400, -1);
  gtk_window_set_resizable(GTK_WINDOW(window), FALSE);
  GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 15);

  gtk_widget_set_margin_start(box, 20);
  gtk_widget_set_margin_end(box, 20);
  gtk_widget_set_margin_top(box, 20);
  gtk_widget_set_margin_bottom(box, 20);

  gtk_window_set_child(GTK_WINDOW(window), box);

  /* Escape text to safely use Pango markup */
  _cleanup_(g_freep) gchar *escaped_sender = g_markup_escape_text(sender, -1);
  _cleanup_(g_freep) gchar *escaped_appname = g_markup_escape_text(appname, -1);
  _cleanup_(g_freep) gchar *escaped_summary = g_markup_escape_text(summary, -1);
  _cleanup_(g_freep) gchar *escaped_body = g_markup_escape_text(body, -1);

  _cleanup_(g_freep) gchar *markup = g_strdup_printf(
        "<span foreground='%s'>"
        "<b>Summary:</b> %s\n"
        "<b>Application:</b> %s\n"
        "<b>Sender:</b> %s\n"
        "\n%s"
        "</span>",
        color, escaped_summary, escaped_appname, escaped_sender, escaped_body);

  GtkWidget *label = gtk_label_new(NULL);
  gtk_label_set_markup(GTK_LABEL(label), markup);
  gtk_label_set_wrap(GTK_LABEL(label), TRUE);
  gtk_label_set_xalign(GTK_LABEL(label), 0.0); // Left align text

  gtk_widget_set_vexpand(label, TRUE);
  gtk_widget_set_hexpand(label, TRUE);
  gtk_box_append(GTK_BOX(box), label);

  GtkWidget *button = gtk_button_new_with_label("OK");
  g_signal_connect_swapped(button, "clicked", G_CALLBACK(gtk_window_destroy), window);

  GtkWidget *button_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_widget_set_halign(button_box, GTK_ALIGN_END);
  gtk_box_append(GTK_BOX(button_box), button);
  gtk_box_append(GTK_BOX(box), button_box);

  if (timeout_seconds > 0)
    {
      guint timeout_id = g_timeout_add_seconds(timeout_seconds, timeout_cb, window);
      g_object_set_data(G_OBJECT(window), "timeout_id", GUINT_TO_POINTER(timeout_id));
      g_signal_connect(window, "destroy", G_CALLBACK(close_window_cb), NULL);
    }

  gtk_window_present(GTK_WINDOW(window));
}

/* Application Activation: Do nothing, wait for D-Bus signals */
static void
activate_cb(GtkApplication *app _unused_, gpointer user_data _unused_)
{
  // Left intentionally blank. No initial window is spawned.
}

/* Connect to D-Bus and run in background */
static void
startup_cb(GApplication *app, gpointer user_data _unused_)
{
  _cleanup_(g_error_freep) GError *error = NULL;
  GDBusConnection *connection;

  /* Keep the application alive even when there are no windows open */
  g_application_hold(app);

  connection = g_bus_get_sync(G_BUS_TYPE_SYSTEM, NULL, &error);
  if (error != NULL)
    {
      g_printerr("Failed to connect to D-Bus: %s\n", error->message);
      return;
    }

  g_dbus_connection_signal_subscribe(
      connection,
      NULL,
      "org.opensuse.WallBroadcast",
      NULL,
      NULL,
      NULL,
      G_DBUS_SIGNAL_FLAGS_NONE,
      signal_received_cb,
      app,
      NULL);
}

static int
run_app(int argc, char *argv[])
{
  GtkApplication *app;
  int status;

  /* G_APPLICATION_DEFAULT_FLAGS is fine here; we enforce background running via g_application_hold() */
  app = gtk_application_new("org.opensuse.wall-bcst-watcher-gtk4", G_APPLICATION_DEFAULT_FLAGS);

  g_signal_connect(app, "startup", G_CALLBACK(startup_cb), NULL);
  g_signal_connect(app, "activate", G_CALLBACK(activate_cb), NULL);

  status = g_application_run(G_APPLICATION(app), argc, argv);
  g_object_unref(app);

  return status;
}

static void
print_usage(FILE *stream)
{
  fprintf(stream, "Usage: wall-bcst-watcher-gtk4 [--help] [--version]\n");
}

static void
print_help(void)
{
  fprintf(stdout, "wall-bcst-watcher-gtk4 - listen on wall broadcast messages\n\n");
  print_usage(stdout);

  fputs("  -h, --help          Give this help list\n", stdout);
  fputs("  -v, --version       Print program version\n", stdout);
}

static void
print_error(void)
{
  fprintf(stderr, "Try `wall-bcst-watcher-gtk4 --help' for more information.\n");
}

int
main(int argc, char *argv[])
{
  while (1)
    {
      int c;
      int option_index = 0;
      static struct option long_options[] =
        {
          {"help",    no_argument, NULL, 'h' },
          {"version", no_argument, NULL, 'v' },
          {NULL,      0,           NULL, '\0'}
        };

      c = getopt_long (argc, argv, "hv",
                       long_options, &option_index);
      if (c == (-1))
        break;
      switch (c)
        {
        case 'h':
          print_help();
          return 0;
        case 'v':
          printf("wall-bcst-watcher-gtk4 (%s) %s\n", PACKAGE, VERSION);
          return 0;
        default:
          print_error();
          return EINVAL;
        }
    }

  argc -= optind;
  argv += optind;

  if (argc > 1)
    {
      fprintf(stderr, "wall-bcst-watcher: too many arguments.");
      print_error();
      return EINVAL;
    }

  return run_app(argc, argv);
}
