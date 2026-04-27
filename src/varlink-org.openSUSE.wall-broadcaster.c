//SPDX-License-Identifier: GPL-2.0-or-later

#include "varlink-org.openSUSE.wall-broadcaster.h"

static SD_VARLINK_DEFINE_METHOD(
		Broadcast,
		SD_VARLINK_FIELD_COMMENT("Send a wall broadcast message"),
		SD_VARLINK_DEFINE_INPUT(AppName, SD_VARLINK_STRING,  0),
		SD_VARLINK_DEFINE_INPUT(Summary, SD_VARLINK_STRING,  0),
		SD_VARLINK_DEFINE_INPUT(Body, SD_VARLINK_STRING,  0),
		SD_VARLINK_DEFINE_INPUT(Urgency, SD_VARLINK_INT, 0),
		SD_VARLINK_DEFINE_INPUT(Sender, SD_VARLINK_STRING,  SD_VARLINK_NULLABLE),
		SD_VARLINK_DEFINE_OUTPUT(Success, SD_VARLINK_BOOL, 0),
                SD_VARLINK_DEFINE_OUTPUT(ErrorMsg, SD_VARLINK_STRING, SD_VARLINK_NULLABLE));

static SD_VARLINK_DEFINE_METHOD(
                Quit,
                SD_VARLINK_FIELD_COMMENT("Stop the daemon"),
                SD_VARLINK_DEFINE_INPUT(ExitCode, SD_VARLINK_INT, SD_VARLINK_NULLABLE),
                SD_VARLINK_DEFINE_OUTPUT(Success, SD_VARLINK_BOOL, 0));

static SD_VARLINK_DEFINE_ERROR(InvalidParameter);
static SD_VARLINK_DEFINE_ERROR(InternalError);

SD_VARLINK_DEFINE_INTERFACE(
                org_openSUSE_wallBroadcaster,
                "org.openSUSE.wallBroadcaster",
		SD_VARLINK_INTERFACE_COMMENT("Wall-Broadcaster control APIs"),
		SD_VARLINK_SYMBOL_COMMENT("Send wall message"),
                &vl_method_Broadcast,
		SD_VARLINK_SYMBOL_COMMENT("Stop the daemon"),
                &vl_method_Quit,
		SD_VARLINK_SYMBOL_COMMENT("Invalid Parameter"),
                &vl_error_InvalidParameter,
		SD_VARLINK_SYMBOL_COMMENT("Internal Error which should never happen"),
		&vl_error_InternalError);
