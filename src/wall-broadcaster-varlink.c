// SPDX-License-Identifier: GPL-2.0-or-later

#include "config.h"

#include <pwd.h>
#include <syslog.h>
#include <systemd/sd-bus.h>
#include <systemd/sd-varlink.h>

#include "basics.h"
#include "wall-broadcaster.h"
#include "varlink-org.openSUSE.wall-broadcaster.h"

#define VARLINK_SOCKET "/run/wall-broadcaster.socket"

static bool
check_caller_perms(uid_t peer_uid, uid_t *allowed)
{
  if (peer_uid == 0)
    return true;

  if (!allowed)
    return false;

  for (size_t i = 0; allowed[i] != 0; i++)
    if (peer_uid == allowed[i])
      return true;

  return false;
}

static int
vl_method_quit(sd_varlink *link, sd_json_variant *parameters,
	       sd_varlink_method_flags_t _unused_(flags),
	       void *userdata)
{
  struct p {
    int code;
  } p = {
    .code = 0
  };
  static const sd_json_dispatch_field dispatch_table[] = {
    { "ExitCode", SD_JSON_VARIANT_INTEGER, sd_json_dispatch_int, offsetof(struct p, code), 0 },
    {}
  };
  ctx_t *ctx = userdata;
  int r;

  if (debug)
    log_msg(LOG_DEBUG, "Varlink method \"Quit\" called...");

  r = sd_varlink_dispatch(link, parameters, dispatch_table, /* userdata= */ NULL);
  if (r != 0)
    {
      log_msg(LOG_ERR, "Quit request: varlink dispatch failed: %s", strerror(-r));
      return r;
    }

  uid_t peer_uid;
  r = sd_varlink_get_peer_uid(link, &peer_uid);
  if (r < 0)
    {
      log_msg(LOG_ERR, "Failed to get peer UID: %s", strerror(-r));
      return r;
    }
  if (peer_uid != 0)
    {
      log_msg(LOG_WARNING, "Quit: peer UID %i denied", peer_uid);
      return sd_varlink_error(link, SD_VARLINK_ERROR_PERMISSION_DENIED, parameters);
    }

  r = sd_event_exit(ctx->loop, p.code);
  if (r != 0)
    {
      log_msg(LOG_ERR, "Quit request: disabling event loop failed: %s",
	      strerror(-r));
      return sd_varlink_errorbo(link, "org.openSUSE.wallBraodcaster.InternalError",
                                SD_JSON_BUILD_PAIR_BOOLEAN("Success", false));
    }

  return sd_varlink_replybo(link, SD_JSON_BUILD_PAIR_BOOLEAN("Success", true));
}

struct message {
  char *appname;
  char *summary;
  char *body;
  int urgency;
  char *sender;
};

static void
message_free(struct message *var)
{
  var->appname = mfree(var->appname);
  var->summary = mfree(var->summary);
  var->body = mfree(var->body);
  var->sender = mfree(var->sender);
}

static int
vl_method_broadcast(sd_varlink *link, sd_json_variant *parameters,
		    sd_varlink_method_flags_t _unused_(flags),
		    void *userdata)
{
  _cleanup_(message_free) struct message p = {
    .appname = NULL,
    .summary = NULL,
    .body = NULL,
    .urgency = 1,
    .sender = NULL
  };
  static const sd_json_dispatch_field dispatch_table[] = {
    { "AppName", SD_JSON_VARIANT_STRING,  sd_json_dispatch_string, offsetof(struct message, appname), SD_JSON_MANDATORY },
    { "Summary", SD_JSON_VARIANT_STRING,  sd_json_dispatch_string, offsetof(struct message, summary), SD_JSON_MANDATORY },
    { "Body",    SD_JSON_VARIANT_STRING,  sd_json_dispatch_string, offsetof(struct message, body),    SD_JSON_MANDATORY },
    { "Urgency", SD_JSON_VARIANT_INTEGER, sd_json_dispatch_int,    offsetof(struct message, urgency), SD_JSON_MANDATORY },
    { "Sender",  SD_JSON_VARIANT_STRING,  sd_json_dispatch_string, offsetof(struct message, sender),  0 },
    {}
  };
  ctx_t *ctx = userdata;
  int r;

  if (debug)
    log_msg(LOG_DEBUG, "Varlink method \"Broadcast\" called...");

  r = sd_varlink_dispatch(link, parameters, dispatch_table, &p);
  if (r != 0)
    {
      log_msg(LOG_ERR, "Broadcast request: varlik dispatch failed: %s", strerror(-r));
      return r;
    }

  uid_t peer_uid;
  r = sd_varlink_get_peer_uid(link, &peer_uid);
  if (r < 0)
    {
      log_msg(LOG_ERR, "Failed to get peer UID: %s", strerror(-r));
      return r;
    }

  if (!check_caller_perms(peer_uid, ctx->allow_send))
    {
      log_msg(LOG_WARNING, "Broadcast: peer UID %i denied", peer_uid);
      return sd_varlink_error(link, SD_VARLINK_ERROR_PERMISSION_DENIED, parameters);
    }

  // add caller as sender if not set
  if (p.sender == NULL)
    {
      // ignore errors, sender is optional
      struct passwd *pw = getpwuid(peer_uid);
      if (pw && pw->pw_name)
	p.sender = strdup(pw->pw_name);
    }

  send_dbus_msg(ctx->bus, p.appname, p.summary, p.body, p.urgency,
		strna(p.sender));

  return sd_varlink_replybo(link, SD_JSON_BUILD_PAIR_BOOLEAN("Success", true));
}

int
setup_varlink(ctx_t *ctx)
{
  int r;
  /* XXX _cleanup_(sd_varlink_server_unrefp) */ sd_varlink_server *server = NULL;

  r = sd_varlink_server_new(&server, SD_VARLINK_SERVER_ACCOUNT_UID|SD_VARLINK_SERVER_INHERIT_USERDATA);
  if (r < 0)
    {
      log_msg(LOG_ERR, "Failed to allocate varlink server: %s",
	      strerror(-r));
      return r;
    }

  r = sd_varlink_server_set_info(server, "openSUSE",
                                 PACKAGE" (wall-broadcaster)",
                                 VERSION,
                                 "https://github.com/thkukuk/wall-broadcaster");
  if (r < 0)
    return r;

  r = sd_varlink_server_set_description(server, "WallBroadcaster");
  if (r < 0)
    {
      log_msg(LOG_ERR, "Failed to set varlink server description: %s",
	      strerror(-r));
      return r;
    }

  sd_varlink_server_set_userdata(server, ctx);

  r = sd_varlink_server_add_interface(server, &vl_interface_org_openSUSE_wallBroadcaster);
  if (r < 0)
    {
      log_msg(LOG_ERR, "Failed to add Varlink interface: %s",
	      strerror(-r));
      return r;
    }

  r = sd_varlink_server_bind_method_many(server,
                                         "org.openSUSE.wallBroadcaster.Broadcast", vl_method_broadcast,
                                         "org.openSUSE.wallBroadcaster.Quit",      vl_method_quit);
  if (r < 0)
    {
      log_msg(LOG_ERR, "Failed to bind Varlink methods: %s",
              strerror(-r));
      return r;
    }

  r = sd_varlink_server_listen_address(server, VARLINK_SOCKET, 0666);
  if (r < 0)
    {
      log_msg(LOG_ERR, "Failed to bind to Varlink socket: %s", strerror(-r));
      return r;
    }

  r = sd_varlink_server_set_exit_on_idle(server, false);
  if (r < 0)
    {
      log_msg(LOG_ERR, "Failed to disable 'exit on idle': %s", strerror(-r));
      return r;
    }

  r = sd_varlink_server_attach_event(server, ctx->loop, SD_EVENT_PRIORITY_NORMAL);
  if (r < 0)
    {
      log_msg(LOG_ERR, "Failed to attach event loop to varlink server: %s", strerror(-r));
      return r;
    }

  r = sd_varlink_server_listen_auto(server);
  if (r < 0)
    {
      log_msg(LOG_ERR, "Varlink server listen auto failed: %s", strerror(-r));
      return r;
    }

  return 0;
}
