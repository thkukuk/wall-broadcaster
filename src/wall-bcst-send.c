// SPDX-License-Identifier: GPL-2.0-or-later

#include "config.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <systemd/sd-varlink.h>

#include "basics.h"

#define VARLINK_SOCKET "/run/wall-broadcaster.socket"

static int
connect_to_service(sd_varlink **ret)
{
  _cleanup_(sd_varlink_unrefp) sd_varlink *link = NULL;
  int r;

  r = sd_varlink_connect_address(&link, VARLINK_SOCKET);
  if (r < 0)
    {
      fprintf(stderr, "Failed to connect to " VARLINK_SOCKET ": %s\n",
               strerror(-r));
      return r;
    }

  *ret = TAKE_PTR(link);
  return 0;
}

struct status {
  bool success;
  char *error;
};

static void
status_free(struct status *var)
{
  var->error = mfree(var->error);
}

static int
varlink_send_msg(const char *appname, const char *summary, const char *body,
		 int urgency, const char *sender, char **error)
{
  _cleanup_(status_free) struct status p = {
    .success = false,
    .error = NULL,
  };
  static const sd_json_dispatch_field dispatch_table[] = {
    { "Success", SD_JSON_VARIANT_BOOLEAN, sd_json_dispatch_stdbool, offsetof(struct status, success), 0 },
    { "ErrorMsg", SD_JSON_VARIANT_STRING, sd_json_dispatch_string,  offsetof(struct status, error), 0 },
    {}
  };
  _cleanup_(sd_varlink_unrefp) sd_varlink *link = NULL;
  _cleanup_(sd_json_variant_unrefp) sd_json_variant *params = NULL;
  sd_json_variant *result;
  int r;

  r = connect_to_service(&link);
  if (r < 0)
    return r;

  r = sd_json_buildo(&params,
                     SD_JSON_BUILD_PAIR("AppName", SD_JSON_BUILD_STRING(appname)),
		     SD_JSON_BUILD_PAIR("Summary", SD_JSON_BUILD_STRING(summary)),
		     SD_JSON_BUILD_PAIR("Body",    SD_JSON_BUILD_STRING(body)),
                     SD_JSON_BUILD_PAIR("Urgency", SD_JSON_BUILD_INTEGER(urgency)));
  if (r >= 0 && sender)
    r = sd_json_variant_merge_objectbo(&params,
				       SD_JSON_BUILD_PAIR("Sender", SD_JSON_BUILD_STRING(sender)));
  if (r < 0)
    {
      if (error)
        if (asprintf (error, "Failed to build JSON data: %s",
                      strerror(-r)) < 0)
	  {
	    *error = strdup ("Out of memory");
	    return -ENOMEM;
	  }
      return r;
    }

  const char *error_id;
  r = sd_varlink_call(link, "org.openSUSE.wallBroadcaster.Broadcast", params, &result, &error_id);
  if (r < 0)
    {
      if (error)
        if (asprintf (error, "Failed to call Broadcast method: %s",
                      strerror(-r)) < 0)
          *error = strdup ("Out of memory");
      return r;
    }

  /* dispatch before checking error_id, we may need the result for the error
     message */
  r = sd_json_dispatch(result, dispatch_table, SD_JSON_ALLOW_EXTENSIONS, &p);
  if (r < 0)
    {
      if (error)
        if (asprintf (error, "Failed to parse JSON answer: %s",
                      strerror(-r)) < 0)
	  {
	    *error = strdup("Out of memory");
	    return -ENOMEM;
	  }
      return r;
    }

  if (error_id && strlen(error_id) > 0)
    {
      if (error)
        {
          if (p.error)
            *error = strdup(p.error);
          else
            *error = strdup(error_id);
	  if (*error == NULL)
	    return -ENOMEM;
        }
      return -EIO;
    }

  return 0;
}


static void
print_usage(FILE *stream)
{
  fprintf(stream, "Usage: wall-bcst-send [--help] [--version]\n");
}

static void
print_help(void)
{
  fprintf(stdout, "wall-bcst-send - send wall broadcast messages via dbus\n\n");
  print_usage(stdout);

  fputs("  -a, --appname <string>  Application Name\n", stdout);
  fputs("  -b, --body <string>     Message body\n", stdout);
  fputs("  -s, --summary <string>  Summary of message\n", stdout);
  fputs("  -S, --sender <string>   Message sender\n", stdout);
  fputs("  -u, --urgency [0|1|2]   Message urgency\n", stdout);
  fputs("  -h, --help              Give this help list\n", stdout);
  fputs("  -v, --version           Print program version\n", stdout);
}

static void
print_error(void)
{
  fprintf(stderr, "Try `wall-bcst-send --help' for more information.\n");
}

int
main(int argc, char *argv[])
{
  char *appname = "wall-bcst-send";
  char *summary = NULL;
  char *body = NULL;
  int urgency = 1;
  char *sender = NULL;
  int r;

  while (1)
    {
      int c;
      int option_index = 0;
      static struct option long_options[] =
        {
	  {"appname", required_argument, NULL, 'a' },
	  {"summary", required_argument, NULL, 's' },
	  {"body",    required_argument, NULL, 'b' },
	  {"urgency", required_argument, NULL, 'u' },
	  {"sender",  required_argument, NULL, 'S' },
          {"help",    no_argument,       NULL, 'h' },
          {"version", no_argument,       NULL, 'v' },
          {NULL,      0,                 NULL, '\0'}
        };

      c = getopt_long (argc, argv, "a:s:b:u:S:hv",
                       long_options, &option_index);
      if (c == (-1))
        break;
      switch (c)
        {
	case 'a':
	  appname = optarg;
	  break;
	case 's':
	  summary = optarg;
	  break;
	case 'b':
	  body = optarg;
	  break;
	case 'u':
	  urgency = atoi(optarg);
	  break;
	case 'S':
	  sender = optarg;
	  break;
	case 'h':
          print_help();
          return 0;
        case 'v':
          printf("wall-bcst-send (%s) %s\n", PACKAGE, VERSION);
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
      fprintf(stderr, "wall-bcst-send: too many arguments.");
      print_error();
      return EINVAL;
    }

  _cleanup_free_ char *error = NULL;
  r = varlink_send_msg(appname, summary, body, urgency, sender, &error);
  if (r < 0)
    {
      if (error)
	fprintf(stderr, "%s\n", error);
      else
	fprintf(stderr, "Sending the message failed: %s\n", strerror(-r));
      return -r;
    }

  return 0;
}
