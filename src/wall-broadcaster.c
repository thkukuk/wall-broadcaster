// SPDX-License-Identifier: GPL-2.0-or-later

#include "config.h"

#include <errno.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <syslog.h>
#include <systemd/sd-bus.h>
#include <systemd/sd-event.h>
#include <systemd/sd-journal.h>

#include "basics.h"

static bool debug = false;

static void
log_msg(int priority, const char *fmt, ...)
{
  static int is_tty = -1;

  if (is_tty == -1)
    is_tty = isatty(STDOUT_FILENO);

  va_list ap;
  va_start(ap, fmt);

  if (is_tty)
    {
      if (priority <= LOG_ERR)
        {
          vfprintf(stderr, fmt, ap);
          fputc('\n', stderr);
        }
      else
        {
          vprintf(fmt, ap);
          putchar('\n');
        }
    }
  else
    sd_journal_printv(priority, fmt, ap);

  va_end(ap);
}

static int
return_errno_error(const char *function, int r)
{
  log_msg(LOG_ERR, "%s failed: %s", function, strerror(-r));
  return r;
}

static int
parse_broadcast_message(const char *prefix, const char *msg,
			char **sender_ret)
{
  _cleanup_free_ char *sender = NULL;
  size_t prefix_len = strlen(prefix);

  if (strncmp(msg, prefix, prefix_len) != 0)
    return -EINVAL;

  const char *sender_start = msg + prefix_len;
  const char *sender_end = strchr(sender_start, ' ');
  if (!sender_end)
    return -EINVAL; // Malformed string: no space found after sender

  size_t sender_len = sender_end - sender_start;
  sender = malloc(sender_len + 1);
  if (sender == NULL)
    return -ENOMEM;

  strncpy(sender, sender_start, sender_len);
  sender[sender_len] = '\0';

  *sender_ret = TAKE_PTR(sender);

  return 0;
}

// Handler for incoming data on the PTY
static int
pty_handler(sd_event_source *s _unused_, int fd,
	    uint32_t revents _unused_, void *userdata)
{
  sd_bus *bus = userdata;
  const char *appname = "WallBroadcaster";
  _cleanup_free_ char *summary = NULL;
  _cleanup_free_ char *sender = NULL; // Optional
  int8_t urgency = 1; // 0 = low, 1 = normal, 2 = critical
  char buffer[4096]; // XXX try to find a better solution
  const char *body = buffer;
  int r;

  if (debug)
    log_msg(LOG_DEBUG, "pty_handler called");

  ssize_t n = read(fd, buffer, sizeof(buffer) - 1);
  if (n <= 0)
    return 0;

  buffer[n] = '\0';

  if (debug)
    log_msg(LOG_DEBUG, "buffer[%s]", buffer);

  const char *prefix_wall = "Broadcast message from ";
  const char *prefix_write = "Message from ";

  if (strncmp(buffer, prefix_wall, strlen(prefix_wall)) == 0)
    {
      char *cp = strchr(buffer, '\n');
      if (cp == NULL)
	log_msg(LOG_NOTICE, "\\n not found in buffer [%s]", buffer);
      else
	{
	  *cp++='\0';
	  body = cp;
	  summary = buffer;
	}
      r = parse_broadcast_message(prefix_wall, buffer, &sender);
      if (r < 0)
	return 0;
    }
  else if (strncmp(buffer, prefix_write, strlen(prefix_write)) == 0)
    {
      char *cp = strchr(buffer, '\n');
      if (cp == NULL)
	log_msg(LOG_NOTICE, "\\n not found in buffer [%s]", buffer);
      else
	{
	  *cp++='\0';
	  body = cp;
	  summary = buffer;
	}
      r = parse_broadcast_message(prefix_write, buffer, &sender);
      if (r < 0)
	return 0;
    }

  /*
    Signature 'sssys' maps to:
    Application Name(s), Summary(s), Body(s), Urgency(y), Sender(s)
    Application Name, Summary, Body and Urgency follow the
    Freedesktop Notification Specification.
  */
  sd_bus_emit_signal(bus,
		     "/org/opensuse/WallBroadcast",
		     "org.opensuse.WallBroadcast",
		     "MessageReceived",
		     "sssys",
		     strempty(appname), strempty(summary), strna(body),
		     urgency, strna(sender));

  return 0;
}

// Register session with systemd-logind via D-Bus
static int
create_logind_session(sd_bus *bus, const char *tty)
{
  _cleanup_(sd_bus_message_unrefp) sd_bus_message *m = NULL;
  _cleanup_(sd_bus_error_free) sd_bus_error error = SD_BUS_ERROR_NULL;
  int r;

  r = sd_bus_message_new_method_call(bus, &m,
				     "org.freedesktop.login1",
				     "/org/freedesktop/login1",
				     "org.freedesktop.login1.Manager",
				     "CreateSession");
  if (r < 0)
    return return_errno_error("sd_bus_message_new_method_cal", r);

  // Signature for CreateSession is: uusssssussbssa(sv)
  r = sd_bus_message_append(m, "uusssssussbss",
			    getuid(), getpid(), "wall-listener", "tty", "user",
			    "", "", 0, tty, "", 0, "", "");
  if (r < 0)
    return return_errno_error("sd_bus_message_apend", r);

  // Append empty array of properties a(sv)
  r = sd_bus_message_open_container(m, 'a', "(sv)");
  if (r < 0)
    return return_errno_error("sd_bus_message_open_container", r);
  r = sd_bus_message_close_container(m);
  if (r < 0)
    return return_errno_error("sd_bus_message_close_container", r);

  r = sd_bus_call(bus, m, 0, &error, NULL);
  if (r < 0)
    {
      log_msg(LOG_ERR, "Failed to register with logind: %s", error.message);
      return r;
    }

  return 0;
}

static int
run_service_loop(void)
{
  _cleanup_(sd_bus_unrefp) sd_bus *bus = NULL;
  _cleanup_(sd_event_unrefp) sd_event *event = NULL;
  _cleanup_close_ int ptm = -EBADF;
  _cleanup_close_ int pts = -EBADF;
  char *pts_name;
  int r;

  if (debug)
    log_msg(LOG_DEBUG, "run_service_loop called");

  // Allocate a Pseudo-Terminal (PTY)
  ptm = posix_openpt(O_RDWR | O_NOCTTY | O_NONBLOCK);
  if (ptm < 0)
    return return_errno_error("posix_openpt", -errno);

  r = grantpt(ptm);
  if (r < 0)
    return return_errno_error("grantpt", r);

  r = unlockpt(ptm);
  if (r < 0)
    return return_errno_error("unlockpt", r);

  pts_name = ptsname(ptm);
  if (pts_name == NULL)
    return return_errno_error("ptsname", -errno);

  if (debug)
    log_msg(LOG_DEBUG, "accquired pty: %s", pts_name);

  // Open the slave side so the master doesn't trigger EOF
  pts = open(pts_name, O_RDWR);
  if (pts == -1)
    return return_errno_error("open", r);

  // Connect to System D-Bus
  r = sd_bus_default_system(&bus);
  if (r < 0)
    return return_errno_error("sd_bus_default_system", r);
  r = sd_bus_request_name(bus, "org.opensuse.WallBroadcast", 0);
  if (r < 0)
    return return_errno_error("sd_bus_request_name", r);

  // pts_name: drop "/dev/"
  r = create_logind_session(bus, &pts_name[5]);
  if (r < 0)
    return r;

  r = sd_event_default(&event);
  if (r < 0)
    return return_errno_error("sd_event_default", r);
  r = sd_bus_attach_event(bus, event, SD_EVENT_PRIORITY_NORMAL);

  // Add the PTY master file descriptor to the event loop
  r = sd_event_add_io(event, NULL, ptm, EPOLLIN, pty_handler, bus);
  if (r < 0)
    return return_errno_error("sd_event_add_io", r);

  log_msg(LOG_INFO, "Wall Broadcaster started on %s", pts_name);
  r = sd_event_loop(event);
  log_msg(LOG_INFO, "Wall Broadcaster stopped with code %i", r);

  return 0;
}

static void
print_usage(FILE *stream)
{
  fprintf(stream, "Usage: wall-broadcaster [--help] [--version]\n");
}

static void
print_help(void)
{
  fprintf(stdout, "wall-broadcaster - read wall/write message and broadcast via D-BUS\n\n");
  print_usage(stdout);

  fputs("  -d, --debug         Print debug messages\n", stdout);
  fputs("  -h, --help          Give this help list\n", stdout);
  fputs("  -v, --version       Print program version\n", stdout);
}

static void
print_error(void)
{
  log_msg(LOG_ERR, "Try `wall-broadcaster --help' for more information.\n");
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
          printf("wall-broadcaster (%s) %s\n", PACKAGE, VERSION);
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
      log_msg(LOG_ERR, "wall-broadcaster: too many arguments.");
      print_error();
      return EINVAL;
    }

  return -run_service_loop();
}
