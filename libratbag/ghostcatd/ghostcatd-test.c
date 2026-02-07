/*
 * Copyright © 2017 Red Hat, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#include "ghostcatd-test.h"

#ifdef GHOSTCAT_DEVELOPER_EDITION

#include <linux/input.h>
#include <systemd/sd-bus.h>
#include <systemd/sd-event.h>

#include "libghostcat-test.h"
#include "ghostcatd-json.h"

static int load_test_device(sd_bus_message *m,
			    struct ghostcatd *ctx,
			    const struct ghostcat_test_device *source)
{
	static int count;
	static struct ghostcatd_device *ghostcatd_test_device = NULL;
	struct ghostcat_device *device;
	int r;
	char devicename[64];

	if (ghostcatd_test_device) {
		ghostcatd_device_unlink(ghostcatd_test_device);
		ghostcatd_device_unref(ghostcatd_test_device);

		(void) sd_bus_emit_properties_changed(ctx->bus,
						      GHOSTCATD_OBJ_ROOT,
						      GHOSTCATD_NAME_ROOT ".Manager",
						      "Devices",
						      NULL);
	}

	device = ghostcat_device_new_test_device(ctx->lib_ctx, source);

	snprintf(devicename, sizeof(devicename), "testdevice%d", count++);
	r = ghostcatd_device_new(&ghostcatd_test_device, ctx, devicename, device);

	/* the ghostcatd_device takes its own reference, drop ours */
	ghostcat_device_unref(device);

	if (r < 0) {
		log_error("Cannot track test device\n");
		return r;
	}

	ghostcatd_device_link(ghostcatd_test_device);
	if (m) {
		sd_bus_reply_method_return(m, "u", r);
		(void) sd_bus_emit_properties_changed(ctx->bus,
						      GHOSTCATD_OBJ_ROOT,
						      GHOSTCATD_NAME_ROOT ".Manager",
						      "Devices",
						      NULL);
	}

	return 0;
}

static const struct ghostcat_test_device default_device_descr = {
	.num_profiles = 1,
	.num_resolutions = 1,
	.num_buttons = 1,
	.num_leds = 0,
	.profiles = {
		{
			.name = NULL,
			.buttons = {
				{ .action_type = GHOSTCAT_BUTTON_ACTION_TYPE_BUTTON,
				  .button = 0 },
			},
			.resolutions = {
				{ .xres = 1000, .yres = 1000,
				  .dpi_min = 1000, .dpi_max = 1000},
			},
			.disabled = false,
			.active = true,
			.dflt = true,
			.hz = 1000,
			.report_rates = {1000},
		},
	},
	.destroyed = NULL,
	.destroyed_data = NULL,
};


int ghostcatd_load_test_device(sd_bus_message *m,
			     void *userdata,
			     sd_bus_error *error)
{
	struct ghostcatd *ctx = userdata;
	struct ghostcat_test_device td = default_device_descr;
	char *data;
	int r = 0;

	CHECK_CALL(sd_bus_message_read(m, "s", &data));

	r = ghostcatd_parse_json(data, &td);
	if (r != 0) {
		log_error("Failed to parse JSON data\n");
	} else {
		r = load_test_device(m, ctx, &td);
	}
	return sd_bus_reply_method_return(m, "i", r);
}

#endif

void ghostcatd_init_test_device(struct ghostcatd *ctx)
{
#ifdef GHOSTCAT_DEVELOPER_EDITION
	setenv("GHOSTCAT_TEST", "1", 0);

	load_test_device(NULL, ctx, &default_device_descr);
#endif
}

