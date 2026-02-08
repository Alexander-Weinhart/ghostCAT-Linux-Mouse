/***
  This file is part of ratbagd.

  Copyright 2015 David Herrmann <dh.herrmann@gmail.com>

  Permission is hereby granted, free of charge, to any person obtaining a
  copy of this software and associated documentation files (the "Software"),
  to deal in the Software without restriction, including without limitation
  the rights to use, copy, modify, merge, publish, distribute, sublicense,
  and/or sell copies of the Software, and to permit persons to whom the
  Software is furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice (including the next
  paragraph) shall be included in all copies or substantial portions of the
  Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
  THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
  FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
  DEALINGS IN THE SOFTWARE.
***/

#include "config.h"

#include <linux/input-event-codes.h>

#include <assert.h>
#include <errno.h>
#include <libgen.h>
#include <libghostcat.h>
#include <libudev.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/resource.h>
#include <systemd/sd-bus.h>
#include <systemd/sd-event.h>
#include "ghostcatd.h"
#include "shared-macro.h"
#include "libghostcat-util.h"

#include "ghostcatd-test.h"

enum log_level {
	LL_QUIET = 1,
	LL_INFO,
	LL_VERBOSE,
	LL_RAW,
} log_level = LL_INFO;

void log_info(const char *fmt, ...)
{
	va_list args;

	va_start(args, fmt);
	if (log_level >= LL_INFO)
		vprintf(fmt, args);
	va_end(args);
}

void log_verbose(const char *fmt, ...)
{
	va_list args;

	va_start(args, fmt);

	if (log_level >= LL_VERBOSE)
		vprintf(fmt, args);
	va_end(args);
}

void log_error(const char *fmt, ...)
{
	va_list args;

	va_start(args, fmt);
	fprintf(stderr, "%s error: ", program_invocation_short_name);
	vfprintf(stderr, fmt, args);
	va_end(args);
}

static int ghostcatd_find_device(sd_bus *bus,
			       const char *path,
			       const char *interface,
			       void *userdata,
			       void **found,
			       sd_bus_error *error)
{
	_cleanup_(freep) char *name = NULL;
	struct ghostcatd *ctx = userdata;
	struct ghostcatd_device *device;
	int r;

	r = sd_bus_path_decode_many(path,
				    GHOSTCATD_OBJ_ROOT "/device/%",
				    &name);
	if (r <= 0)
		return r;

	device = ghostcatd_device_lookup(ctx, name);
	if (!device)
		return 0;

	*found = device;
	return 1;
}

static int ghostcatd_list_devices(sd_bus *bus,
				const char *path,
				void *userdata,
				char ***paths,
				sd_bus_error *error)
{
	struct ghostcatd *ctx = userdata;
	struct ghostcatd_device *device;
	char **devices, **pos;

	devices = calloc(ctx->n_devices + 1, sizeof(char *));
	if (!devices)
		return -ENOMEM;

	pos = devices;

	GHOSTCATD_DEVICE_FOREACH(device, ctx) {
		*pos = strdup(ghostcatd_device_get_path(device));
		if (!*pos)
			goto error;
		++pos;
	}

	*pos = NULL;
	*paths = devices;
	return 1;

error:
	for (pos = devices; *pos; ++pos)
		free(*pos);
	free(devices);
	return -ENOMEM;
}

static int ghostcatd_get_devices(sd_bus *bus,
			       const char *path,
			       const char *interface,
			       const char *property,
			       sd_bus_message *reply,
			       void *userdata,
			       sd_bus_error *error)
{
	struct ghostcatd *ctx = userdata;
	struct ghostcatd_device *device;

	CHECK_CALL(sd_bus_message_open_container(reply, 'a', "o"));

	GHOSTCATD_DEVICE_FOREACH(device, ctx) {
		CHECK_CALL(sd_bus_message_append(reply,
						 "o",
						 ghostcatd_device_get_path(device)));
	}

	CHECK_CALL(sd_bus_message_close_container(reply));

	return 0;
}

static const sd_bus_vtable ghostcatd_vtable[] = {
	SD_BUS_VTABLE_START(0),
	SD_BUS_PROPERTY("APIVersion", "i", 0, offsetof(struct ghostcatd, api_version), SD_BUS_VTABLE_PROPERTY_CONST),
	SD_BUS_PROPERTY("Devices", "ao", ghostcatd_get_devices, 0, SD_BUS_VTABLE_PROPERTY_EMITS_CHANGE),
#ifdef GHOSTCAT_DEVELOPER_EDITION
	SD_BUS_METHOD("LoadTestDevice", "s", "i", ghostcatd_load_test_device, SD_BUS_VTABLE_UNPRIVILEGED),
#endif /* GHOSTCAT_DEVELOPER_EDITION */
	SD_BUS_VTABLE_END,
};

static void ghostcatd_process_device(struct ghostcatd *ctx,
				   struct udev_device *udevice)
{
	struct ghostcat_device *lib_device;
	struct ghostcatd_device *device;
	const char *sysname;
	int r;

	/*
	 * TODO: libratbag should provide some mechanism to allow
	 *       device-grouping, just like libinput does. If multiple input
	 *       devices belong to the same virtual device, we should not add
	 *       it multiple times. Instead, libratbag should group them and
	 *       provide *us* a unique sysname that identifies the group, rather
	 *       than taking a random input-device as tag.
	 */

	sysname = udev_device_get_sysname(udevice);
	if (!sysname || !startswith(sysname, "hidraw"))
		return;

	device = ghostcatd_device_lookup(ctx, sysname);

	if (streq_ptr("remove", udev_device_get_action(udevice))) {
		/* device was removed, unlink it and destroy our context */
		if (device) {
			ghostcatd_device_unlink(device);
			ghostcatd_device_unref(device);

			(void) sd_bus_emit_properties_changed(ctx->bus,
							      GHOSTCATD_OBJ_ROOT,
							      GHOSTCATD_NAME_ROOT ".Manager",
							      "Devices",
							      NULL);
		}
	} else if (device) {
		/* device already known, refresh our view of the device */
	} else {
		enum ghostcat_error_code error;

		/* device unknown, create new one and link it */
		error = ghostcat_device_new_from_udev_device(ctx->lib_ctx,
							   udevice,
							   &lib_device);
		if (error != GHOSTCAT_SUCCESS)
			return; /* unsupported device */

		r = ghostcatd_device_new(&device, ctx, sysname, lib_device);

		/* the ghostcatd_device takes its own reference, drop ours */
		ghostcat_device_unref(lib_device);

		if (r < 0) {
			log_error("%s: cannot track device\n", sysname);
			return;
		}

		ghostcatd_device_link(device);
		(void) sd_bus_emit_properties_changed(ctx->bus,
						      GHOSTCATD_OBJ_ROOT,
						      GHOSTCATD_NAME_ROOT ".Manager",
						      "Devices",
						      NULL);
	}
}

static int ghostcatd_monitor_event(sd_event_source *source,
				 int fd,
				 uint32_t mask,
				 void *userdata)
{
	struct ghostcatd *ctx = userdata;
	struct udev_device *udevice;

	udevice = udev_monitor_receive_device(ctx->monitor);
	if (!udevice)
		return 0;

	ghostcatd_process_device(ctx, udevice);
	udev_device_unref(udevice);

	return 0;
}

static int ghostcatd_lib_open_restricted(const char *path,
				       int flags,
				       void *userdata)
{
	return open(path, flags, 0);
}

static void ghostcatd_lib_close_restricted(int fd, void *userdata)
{
	safe_close(fd);
}

static const struct ghostcat_interface ghostcatd_lib_interface = {
	.open_restricted	= ghostcatd_lib_open_restricted,
	.close_restricted	= ghostcatd_lib_close_restricted,
};

static struct ghostcatd *ghostcatd_free(struct ghostcatd *ctx)
{
	struct ghostcatd_device *device, *tmp;

	if (!ctx)
		return NULL;

	GHOSTCATD_DEVICE_FOREACH_SAFE(device, tmp, ctx) {
		ghostcatd_device_unlink(device);
		ghostcatd_device_unref(device);
	}

	ctx->bus = sd_bus_flush_close_unref(ctx->bus);
	ctx->monitor_source = sd_event_source_unref(ctx->monitor_source);
	ctx->monitor = udev_monitor_unref(ctx->monitor);
	ctx->lib_ctx = ghostcat_unref(ctx->lib_ctx);
	ctx->event = sd_event_unref(ctx->event);

	assert(!ctx->device_map.root);
	assert(!ctx->lib_ctx); /* ratbag returns non-NULL if still pinned */

	return mfree(ctx);
}

DEFINE_TRIVIAL_CLEANUP_FUNC(struct ghostcatd *, ghostcatd_free);

static int ghostcatd_init_monitor(struct ghostcatd *ctx)
{
	struct udev *udev;
	int r;

	udev = udev_new();
	if (!udev)
		return -ENOMEM;

	ctx->monitor = udev_monitor_new_from_netlink(udev, "udev");

	/* we don't need the context to stay around; drop it */
	udev_unref(udev);

	if (!ctx->monitor)
		return -ENOMEM;

	r = udev_monitor_filter_add_match_subsystem_devtype(ctx->monitor,
							    "hidraw",
							    NULL);
	if (r < 0)
		return r;

	r = udev_monitor_enable_receiving(ctx->monitor);
	if (r < 0)
		return r;

	r = sd_event_add_io(ctx->event,
			    &ctx->monitor_source,
			    udev_monitor_get_fd(ctx->monitor),
			    EPOLLIN,
			    ghostcatd_monitor_event,
			    ctx);
	if (r < 0)
		return r;

	return 0;
}

static int ghostcatd_new(struct ghostcatd **out)
{
	_cleanup_(ghostcatd_freep) struct ghostcatd *ctx = NULL;
	int r;

	log_verbose("Starting ghostcatd version %s (API version %d)\n",
		 GHOSTCAT_VERSION, GHOSTCATD_API_VERSION);

	ctx = zalloc(sizeof(*ctx));
	ctx->api_version = GHOSTCATD_API_VERSION;

	r = sd_event_default(&ctx->event);
	if (r < 0)
		return r;

	r = sd_event_set_watchdog(ctx->event, true);
	if (r < 0)
		return r;

	log_verbose("Initializing libghostcat\n");
	ctx->lib_ctx = ghostcat_create_context(&ghostcatd_lib_interface, ctx);
	if (!ctx->lib_ctx)
		return -ENOMEM;

	if (log_level >= LL_RAW)
		ghostcat_log_set_priority(ctx->lib_ctx,
					GHOSTCAT_LOG_PRIORITY_RAW);
	else if (log_level >= LL_VERBOSE)
		ghostcat_log_set_priority(ctx->lib_ctx,
					GHOSTCAT_LOG_PRIORITY_DEBUG);

	r = ghostcatd_init_monitor(ctx);
	if (r < 0)
		return r;

	r = sd_bus_open_system(&ctx->bus);
	if (r < 0)
		return r;

	r = sd_bus_add_object_vtable(ctx->bus,
				     NULL,
				     GHOSTCATD_OBJ_ROOT,
				     GHOSTCATD_NAME_ROOT ".Manager",
				     ghostcatd_vtable,
				     ctx);
	if (r < 0)
		return r;

	r = sd_bus_add_fallback_vtable(ctx->bus,
				       NULL,
				       GHOSTCATD_OBJ_ROOT "/device",
				       GHOSTCATD_NAME_ROOT ".Device",
				       ghostcatd_device_vtable,
				       ghostcatd_find_device,
				       ctx);
	if (r < 0)
		return r;

	r = sd_bus_add_node_enumerator(ctx->bus,
				       NULL,
				       GHOSTCATD_OBJ_ROOT "/device",
				       ghostcatd_list_devices,
				       ctx);
	if (r < 0)
		return r;

	r = sd_bus_request_name(ctx->bus, GHOSTCATD_NAME_ROOT, 0);
	if (r < 0)
		return r;

	r = sd_bus_attach_event(ctx->bus, ctx->event, 0);
	if (r < 0)
		return r;

	*out = ctx;
	ctx = NULL;
	return 0;
}

static int ghostcatd_run_enumerate(struct ghostcatd *ctx)
{
	struct udev_list_entry *list, *iter;
	struct udev_enumerate *e;
	struct udev *udev;
	int r;

	udev = udev_monitor_get_udev(ctx->monitor);

	e = udev_enumerate_new(udev);
	if (!e)
		return -ENOMEM;

	r = udev_enumerate_add_match_subsystem(e, "hidraw");
	if (r < 0)
		goto exit;

	r = udev_enumerate_add_match_is_initialized(e);
	if (r < 0)
		goto exit;

	r = udev_enumerate_scan_devices(e);
	if (r < 0)
		goto exit;

	list = udev_enumerate_get_list_entry(e);
	udev_list_entry_foreach(iter, list) {
		struct udev_device *udevice;
		const char *p;

		p = udev_list_entry_get_name(iter);
		udevice = udev_device_new_from_syspath(udev, p);
		if (udevice)
			ghostcatd_process_device(ctx, udevice);
		udev_device_unref(udevice);
	}

	r = 0;

exit:
	udev_enumerate_unref(e);
	return r;
}

static int on_timeout_cb(sd_event_source *s, uint64_t usec, void *userdata)
{
	/* Disabled idle exit - keep running */
	return 0;
}

#define POLL_INTERVAL_USEC (2 * 1000000ULL) /* 2 seconds */

static int on_poll_active_resolution(sd_event_source *s, uint64_t usec, void *userdata)
{
	struct ghostcatd *ctx = userdata;
	struct ghostcatd_device *device;
	uint64_t now;

	GHOSTCATD_DEVICE_FOREACH(device, ctx) {
		ghostcatd_device_poll_active_resolution(device, ctx->bus);
	}

	/* Re-arm the timer */
	sd_event_now(ctx->event, CLOCK_MONOTONIC, &now);
	sd_event_source_set_time(s, now + POLL_INTERVAL_USEC);
	sd_event_source_set_enabled(s, SD_EVENT_ONESHOT);

	return 0;
}

static int before_idle_cb(sd_event_source *s, void *userdata)
{
	struct ghostcatd *ctx = userdata;
	uint64_t usec;

	sd_event_now(sd_event_source_get_event(s), CLOCK_MONOTONIC, &usec);
#define min2us(us_) (us_ * 1000000 * 60)
	usec += min2us(20);
	sd_event_source_set_time(ctx->timeout_source, usec);

	return 0;
}

static int sighandler(sd_event_source *source,
		      const struct signalfd_siginfo *si,
		      void *userdata)
{
	sd_event *event = sd_event_source_get_event(source);
	sd_event_exit(event, 0);
	return 0;
}

static int ghostcatd_run(struct ghostcatd *ctx)
{
	int r;

	r = ghostcatd_run_enumerate(ctx);
	if (r < 0)
		return r;

	sigset_t sigset;
	sigemptyset(&sigset);
	sigaddset(&sigset, SIGINT);
	sigprocmask(SIG_BLOCK, &sigset, NULL);
	sd_event_add_signal(ctx->event, NULL, SIGINT, sighandler, NULL);

	/* exit-on-idle: we set up a timer to simply exit. Since we don't
	 * store anything, it doesn't matter and we can just restart next
	 * time someone wants us.
	 *
	 * since we don't want to monitor every single dbus call, we just
	 * set up a post source that gets called before we go idle. That
	 * resets the timer, so as long as something is happening, ratbagd
	 * won't exit.
	 */
	sd_event_add_time(ctx->event,
			  &ctx->timeout_source,
			  CLOCK_MONOTONIC,
			  -1, /* infinite, see before_idle_cb */
			  min2us(1), /* accuracy doesn't matter */
			  on_timeout_cb,
			  ctx);
	sd_event_add_post(ctx->event, NULL, before_idle_cb, ctx);

	/* Poll active resolution every 2 seconds to detect physical
	 * DPI button presses on the device */
	{
		sd_event_source *poll_source = NULL;
		uint64_t now;
		sd_event_now(ctx->event, CLOCK_MONOTONIC, &now);
		sd_event_add_time(ctx->event,
				  &poll_source,
				  CLOCK_MONOTONIC,
				  now + POLL_INTERVAL_USEC,
				  1000000, /* 1s accuracy */
				  on_poll_active_resolution,
				  ctx);
	}

	log_verbose("DBus server ready\n");

	return sd_event_loop(ctx->event);
}

static bool install_ghostcatd_devel_dbus_policy(void)
{
	bool rc = true;
#ifdef GHOSTCAT_DEVELOPER_EDITION
	_cleanup_close_ int in = -1, out = -1;
	int nread, nwrite;
	char buf[40960];
	int r;
	sd_bus_error error = SD_BUS_ERROR_NULL;
	sd_bus_message *m = NULL;
	sd_bus *bus = NULL;

	rc = false;

	log_verbose("Installing DBus policy file to %s\n", DBUS_POLICY_DST);

	in = open(DBUS_POLICY_SRC, O_RDONLY);
	if (in == -1) {
		log_error("Failed to source policy file: %m\n");
		goto out;
	}

	if (mkdir_p(dirname(strdupa(DBUS_POLICY_DST)), 0755)) {
		log_error("Failed to create destination path: %m\n");
		goto out;
	}


	out = open(DBUS_POLICY_DST, O_CREAT|O_WRONLY, 0644);
	if (out == -1) {
		log_error("Failed to open destination: %m\n");
		goto out;
	}


	nread = read(in, buf, sizeof(buf));
	if (nread < 0) {
		log_error("Failed to read policy file: %m\n");
		goto out;
	}

	nwrite = write(out, buf, nread);
	if (nread != nwrite) {
		log_error("Failed to write policy file: %m\n");
		goto out;
	}

	/* Now poke DBus to reload itself */
	r = sd_bus_open_system(&bus);
	if (r < 0) {
		log_error("Unable to open system bus: %s\n", strerror(-r));
		goto out;
	}
	r = sd_bus_call_method(bus, "org.freedesktop.DBus",
			       "/org/freedesktop/DBus",
			       "org.freedesktop.DBus",
			       "ReloadConfig",
			       &error,
			       &m,
			       "");
	if (r < 0) {
		log_error("Failed to call DBus ReloadConfig: %s\n",
			  error.message);
		goto out;
	}

	rc = true;

out:
	sd_bus_error_free(&error);
	sd_bus_message_unref(m);
	sd_bus_unref(bus);

#endif
	return rc;
}

static void remove_ghostcatd_devel_dbus_policy(void)
{
#ifdef GHOSTCAT_DEVELOPER_EDITION
	unlink(DBUS_POLICY_DST);
#endif
}

int main(int argc, char *argv[])
{
	struct ghostcatd *ctx = NULL;
	int r = 0;

#if DISABLE_COREDUMP
	const struct rlimit corelimit = { 0, 0 };

	setrlimit(RLIMIT_CORE, &corelimit);
#endif

	if (argc > 1) {
		if (streq(argv[1], "--version")) {
			printf("%s\n", GHOSTCAT_VERSION);
			return 0;
		} else if (streq(argv[1], "--quiet")) {
			log_level = LL_QUIET;
		} else if (streq(argv[1], "--verbose") || streq(argv[1], "--verbose=raw")) {
			log_level = LL_RAW;
		} else if (streq(argv[1], "--verbose=debug")) {
			log_level = LL_VERBOSE;
		} else {
			fprintf(stderr, "Usage: %s [--version | --quiet | --verbose[=debug]]\n",
				program_invocation_short_name);
			r = -EINVAL;
			goto exit;
		}
	}

	if (!install_ghostcatd_devel_dbus_policy())
		goto exit;

	r = ghostcatd_new(&ctx);
	if (r < 0)
		goto exit;

	ghostcatd_init_test_device(ctx);

	r = ghostcatd_run(ctx);

	remove_ghostcatd_devel_dbus_policy();
exit:
	ghostcatd_free(ctx);

	if (r < 0) {
		if (r == -EEXIST) {
			log_error("Bus name is taken, another instance of ghostcatd is already running.\n");
		} else {
			errno = -r;
			log_error("Failed to start ghostcatd: %m\n");
		}
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}

struct ghostcatd_callback
{
	ghostcatd_callback_t callback;
	void *userdata;
};

static int ghostcatd_callback_handler(sd_event_source *s, void *userdata)
{
	struct ghostcatd_callback *cb = userdata;

	cb->callback(cb->userdata);

	sd_event_source_set_enabled(s, SD_EVENT_OFF);
	sd_event_source_unref(s);
	free(cb);

	return 0;
}

void ghostcatd_schedule_task(struct ghostcatd *ctx,
			   ghostcatd_callback_t callback,
			   void *userdata)
{
	struct ghostcatd_callback *cb = zalloc(sizeof *cb);
	sd_event_source *source;

	cb->callback = callback;
	cb->userdata = userdata;

	sd_event_add_defer(ctx->event, &source, ghostcatd_callback_handler, cb);
}
