// SPDX-License-Identifier: GPL-2.0-or-later

#include "config.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <getopt.h>
#include <systemd/sd-bus.h>

#include "basics.h"

static bool debug = false;

static int
wall_broadcast_cb(sd_bus_message *m, void *userdata,
		  sd_bus_error *ret_error _unused_)
{
  sd_bus *user_bus = userdata;
  _cleanup_(sd_bus_message_unrefp) sd_bus_message *notify_msg = NULL;
  _cleanup_(sd_bus_error_free) sd_bus_error error = SD_BUS_ERROR_NULL;
  const char *appname = NULL, *summary = NULL, *body = NULL, *sender = NULL;
  uint8_t urgency = 0;
  int r;

  r = sd_bus_message_read(m, "sssys", &appname, &summary, &body,
			  &urgency, &sender);
  if (r < 0)
    {
      fprintf(stderr, "Failed to parse WallBroadcast message: %s\n",
	      strerror(-r));
      return 0;
    }

  if (debug)
    {
      printf("Forwarding Broadcast:\n");
      printf("AppName: '%s'\n", strna(appname));
      printf("Summary: '%s'\n", strna(summary));
      printf("Urgency: %u\n", urgency);
      printf("Sender: '%s'\n", strna(sender));
      printf("Body: '%s'\n", strna(body));
    }

  r = sd_bus_message_new_method_call(user_bus, &notify_msg,
				     "org.freedesktop.Notifications",
				     "/org/freedesktop/Notifications",
				     "org.freedesktop.Notifications",
				     "Notify");
  if (r >= 0)
    {
      // App Name (s), Replaces ID (u), Icon (s), Summary (s), Body (s)
      r = sd_bus_message_append(notify_msg, "susss", appname, (uint32_t)0, "dialog-information", summary, body);
    }
  if (r >= 0)
    r = sd_bus_message_open_container(notify_msg, 'a', "s");
  if (r >= 0)
    r = sd_bus_message_close_container(notify_msg);
  if (r >= 0)
    r = sd_bus_message_open_container(notify_msg, 'a', "{sv}");
  if (r >= 0)
    r = sd_bus_message_open_container(notify_msg, 'e', "sv");
  if (r >= 0)
    r = sd_bus_message_append(notify_msg, "s", "urgency");
  if (r >= 0)
    r = sd_bus_message_open_container(notify_msg, 'v', "y");
  if (r >= 0)
    r = sd_bus_message_append(notify_msg, "y", urgency);
  if (r >= 0)
    r = sd_bus_message_close_container(notify_msg);
  if (r >= 0)
    r = sd_bus_message_close_container(notify_msg);
  if (r >= 0)
    r = sd_bus_message_close_container(notify_msg);
  if (r >= 0)
    {
      if (urgency == 2)
	r = sd_bus_message_append(notify_msg, "i", (int32_t)0);
      else
	r = sd_bus_message_append(notify_msg, "i", (int32_t)-1);
    }

  if (r < 0)
    fprintf(stderr, "Error constructing message: %s\n", strerror(-r));
  else
    {
      r = sd_bus_call(user_bus, notify_msg, 0, &error, NULL);
      if (r < 0)
	fprintf(stderr, "Failed to send desktop notification: %s\n",
		error.message);
    }

  return 0; // Return 0 to tell sd_bus we handled the message
}

static int
run_loop(void)
{
  _cleanup_(sd_bus_unrefp) sd_bus *system_bus = NULL;
  _cleanup_(sd_bus_unrefp) sd_bus *user_bus = NULL;
  int r;

  r = sd_bus_default_user(&user_bus);
  if (r < 0)
    {
      fprintf(stderr, "Failed to connect to the user bus: %s\n",
	      strerror(-r));
      return r;
    }

  r = sd_bus_default_system(&system_bus);
  if (r < 0)
    {
      fprintf(stderr, "Failed to connect to the system bus: %s\n",
	      strerror(-r));
      return r;
    }

  r = sd_bus_add_match(system_bus,
		       NULL,
		       "type='signal',interface='org.opensuse.WallBroadcast'",
		       wall_broadcast_cb,
		       user_bus);
  if (r < 0)
    {
      fprintf(stderr, "Failed to add match rule: %s\n", strerror(-r));
      return r;
    }

  if (debug)
    printf("Listening for WallBroadcast messages on the system bus...\n");

  for (;;)
    {
      r = sd_bus_process(system_bus, NULL);
      if (r < 0)
	{
	  fprintf(stderr, "Failed to process system bus: %s\n", strerror(-r));
	  return r;
        }

      // If > 0, a message was processed, loop back immediately
      if (r > 0)
	continue;

      // If 0, wait until a message arrives
      r = sd_bus_wait(system_bus, (uint64_t) -1);
      if (r < 0)
	{
	  fprintf(stderr, "Failed to wait on system bus: %s\n", strerror(-r));
	  return r;
        }
    }

  return -EIO;
}

static void
print_usage(FILE *stream)
{
  fprintf(stream, "Usage: wall-bcst-gateway [--help] [--version]\n");
}

static void
print_help(void)
{
  fprintf(stdout, "wall-bcst-gateway - forward wall broadcast messages as notification\n\n");
  print_usage(stdout);

  fputs("  -d, --debug         Print debug messages\n", stdout);
  fputs("  -h, --help          Give this help list\n", stdout);
  fputs("  -v, --version       Print program version\n", stdout);
}

static void
print_error(void)
{
  fprintf(stderr, "Try `wall-bcst-gateway --help' for more information.\n");
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
	  {"debug",   no_argument, NULL, 'd' },
          {"help",    no_argument, NULL, 'h' },
          {"version", no_argument, NULL, 'v' },
          {NULL,      0,           NULL, '\0'}
        };

      c = getopt_long (argc, argv, "dhv",
                       long_options, &option_index);
      if (c == (-1))
        break;
      switch (c)
        {
	case 'd':
	  debug = true;
	  break;
        case 'h':
          print_help();
          return 0;
        case 'v':
          printf("wall-bcst-gateway (%s) %s\n", PACKAGE, VERSION);
          return 0;
        default:
          print_error();
          return 1;
        }
    }

  argc -= optind;
  argv += optind;

  if (argc > 1)
    {
      fprintf(stderr, "wall-bcst-gateway: too many arguments.");
      print_error();
      return EINVAL;
    }

  return -run_loop();
}
