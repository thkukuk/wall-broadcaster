// SPDX-License-Identifier: GPL-2.0-or-later

#include "config.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <systemd/sd-bus.h>

#include "basics.h"

// Callback function to handle the incoming signal
static int
message_handler(sd_bus_message *m, void *userdata _unused_,
		sd_bus_error *ret_error _unused_)
{
  const char *appname;
  const char *summary;
  const char *body;
  int8_t urgency;
  const char *sender;
  int r;

  r = sd_bus_message_read(m, "sssys", &appname, &summary, &body, &urgency, &sender);
  if (r < 0)
    {
      fprintf(stderr, "Failed to parse signal message: %s\n", strerror(-r));
      return 0;
    }

  printf("New Wall Broadcast Message:\n");
  printf("Application: [%s]\n", appname);
  printf("Summary    : [%s]\n", summary);
  printf("Body       : [%s]\n", body);
  printf("Urgency    : %i\n",   urgency);
  printf("Sender     : [%s]\n", sender);

  return 0;
}

static int
run_loop(void)
{
  _cleanup_(sd_bus_slot_unrefp) sd_bus_slot *slot = NULL;
  _cleanup_(sd_bus_unrefp) sd_bus *bus = NULL;
  int r;

  r = sd_bus_default_system(&bus);
  if (r < 0)
    {
      fprintf(stderr, "Failed to connect to the system bus: %s\n",
	      strerror(-r));
      return r;
    }

  /* Define the match rule to filter only the exact signal we want */
  const char *match = "type='signal',"
                        "path='/org/opensuse/WallBroadcast',"
                        "interface='org.opensuse.WallBroadcast',"
                        "member='MessageReceived'";

    /* Add the match rule and assign our callback function */
  r = sd_bus_add_match(bus, &slot, match, message_handler, NULL);
  if (r < 0)
    {
      fprintf(stderr, "Failed to add match rule: %s\n", strerror(-r));
      return r;
    }

  printf("Listening for Wall Broadcast messages on D-Bus...\n");

  /* Enter the main event loop to process D-Bus traffic */
  for (;;)
    {
      /* Process requests */
      r = sd_bus_process(bus, NULL);
      if (r < 0)
	{
	  fprintf(stderr, "Failed to process bus: %s\n", strerror(-r));
	  return r;
        }

      /* If we processed a request, try to process another one right away */
      if (r > 0)
	continue;

      /* Wait for the next request to process to avoid spinning the CPU */
      r = sd_bus_wait(bus, (uint64_t) -1);
      if (r < 0)
	{
	  fprintf(stderr, "Failed to wait on bus: %s\n", strerror(-r));
	  return r;
        }
    }

  return 0;
}

static void
print_usage(FILE *stream)
{
  fprintf(stream, "Usage: wall-bcst-watcher [--help] [--version]\n");
}

static void
print_help(void)
{
  fprintf(stdout, "wall-bcst-watcher - listen on wall broadcast messages\n\n");
  print_usage(stdout);

  fputs("  -h, --help          Give this help list\n", stdout);
  fputs("  -v, --version       Print program version\n", stdout);
}

static void
print_error(void)
{
  fprintf(stderr, "Try `wall-bcst-watcher --help' for more information.\n");
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

      c = getopt_long (argc, argv, "dhv",
                       long_options, &option_index);
      if (c == (-1))
        break;
      switch (c)
        {
	case 'h':
          print_help();
          return 0;
        case 'v':
          printf("wall-bcst-watcher (%s) %s\n", PACKAGE, VERSION);
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
      fprintf(stderr, "wall-bcst-watcher: too many arguments.");
      print_error();
      return EINVAL;
    }

  return -run_loop();
}
