// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

extern bool debug;

typedef struct {
  sd_event *loop;
  sd_bus *bus;
} ctx_t;

extern void log_msg(int priority, const char *fmt, ...);
extern int send_dbus_msg(sd_bus *bus, const char *appname,
			 const char *summary, const char *body,
			 int urgency, const char *sender);
extern int setup_varlink(ctx_t *ctx);
