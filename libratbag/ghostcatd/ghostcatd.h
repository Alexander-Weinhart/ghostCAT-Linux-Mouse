#pragma once

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

#include <assert.h>
#include <errno.h>
#include <libratbag.h>
#include <libudev.h>
#include <stdbool.h>
#include <stdlib.h>
#include <systemd/sd-bus.h>
#include <systemd/sd-event.h>
#include "shared-macro.h"
#include <rbtree/shared-rbtree.h>

#ifndef GHOSTCAT_DBUS_INTERFACE
#define GHOSTCAT_DBUS_INTERFACE	"ratbag1"
#else
#define GHOSTCAT_DEVELOPER_EDITION
#endif

#define GHOSTCATD_OBJ_ROOT "/org/freedesktop/" GHOSTCAT_DBUS_INTERFACE
#define GHOSTCATD_NAME_ROOT "org.freedesktop." GHOSTCAT_DBUS_INTERFACE

struct ghostcatd;
struct ghostcatd_device;
struct ghostcatd_profile;
struct ghostcatd_resolution;
struct ghostcatd_button;
struct ghostcatd_led;

void log_info(const char *fmt, ...) _printf_(1, 2);
void log_verbose(const char *fmt, ...) _printf_(1, 2);
void log_error(const char *fmt, ...) _printf_(1, 2);

#define CHECK_CALL(_call) \
	do { \
		int _r = _call; \
		if (_r < 0) { \
			log_error("%s: '%s' failed with: %s\n", __func__, #_call, strerror(-_r)); \
			return _r; \
		} \
	} while (0)


/*
 * Profiles
 */

extern const sd_bus_vtable ghostcatd_profile_vtable[];

int ghostcatd_profile_new(struct ghostcatd_profile **out,
			struct ghostcatd_device *device,
			struct ghostcat_profile *lib_profile,
			unsigned int index);
struct ghostcatd_profile *ghostcatd_profile_free(struct ghostcatd_profile *profile);
const char *ghostcatd_profile_get_path(struct ghostcatd_profile *profile);
bool ghostcatd_profile_is_default(struct ghostcatd_profile *profile);
unsigned int ghostcatd_profile_get_index(struct ghostcatd_profile *profile);
int ghostcatd_profile_register_resolutions(struct sd_bus *bus,
					 struct ghostcatd_device *device,
					 struct ghostcatd_profile *profile);
int ghostcatd_profile_register_buttons(struct sd_bus *bus,
				     struct ghostcatd_device *device,
				     struct ghostcatd_profile *profile);
int ghostcatd_profile_register_leds(struct sd_bus *bus,
				  struct ghostcatd_device *device,
				  struct ghostcatd_profile *profile);

int ghostcatd_for_each_profile_signal(sd_bus *bus,
				    struct ghostcatd_device *device,
				    int (*func)(sd_bus *bus,
						struct ghostcatd_profile *profile));
int ghostcatd_for_each_resolution_signal(sd_bus *bus,
				       struct ghostcatd_profile *profile,
				       int (*func)(sd_bus *bus,
						   struct ghostcatd_resolution *resolution));
int ghostcatd_for_each_button_signal(sd_bus *bus,
				   struct ghostcatd_profile *profile,
				   int (*func)(sd_bus *bus,
					       struct ghostcatd_button *button));
int ghostcatd_for_each_led_signal(sd_bus *bus,
				struct ghostcatd_profile *profile,
				int (*func)(sd_bus *bus,
					    struct ghostcatd_led *led));
int ghostcatd_profile_resync(sd_bus *bus, struct ghostcatd_profile *profile);

DEFINE_TRIVIAL_CLEANUP_FUNC(struct ghostcatd_profile *, ghostcatd_profile_free);

/*
 * Resolutions
 */
extern const sd_bus_vtable ghostcatd_resolution_vtable[];

int ghostcatd_resolution_new(struct ghostcatd_resolution **out,
			   struct ghostcatd_device *device,
			   struct ghostcatd_profile *profile,
			   struct ghostcat_resolution *lib_resolution,
			   unsigned int index);
struct ghostcatd_resolution *ghostcatd_resolution_free(struct ghostcatd_resolution *resolution);
const char *ghostcatd_resolution_get_path(struct ghostcatd_resolution *resolution);
int ghostcatd_resolution_resync(sd_bus *bus, struct ghostcatd_resolution *resolution);

DEFINE_TRIVIAL_CLEANUP_FUNC(struct ghostcatd_resolution *, ghostcatd_resolution_free);

/*
 * Buttons
 */
extern const sd_bus_vtable ghostcatd_button_vtable[];

int ghostcatd_button_new(struct ghostcatd_button **out,
		       struct ghostcatd_device *device,
		       struct ghostcatd_profile *profile,
		       struct ghostcat_button *lib_button,
		       unsigned int index);
struct ghostcatd_button *ghostcatd_button_free(struct ghostcatd_button *button);
const char *ghostcatd_button_get_path(struct ghostcatd_button *button);
int ghostcatd_button_resync(sd_bus *bus, struct ghostcatd_button *button);

DEFINE_TRIVIAL_CLEANUP_FUNC(struct ghostcatd_button *, ghostcatd_button_free);

/*
 * Leds
 */
extern const sd_bus_vtable ghostcatd_led_vtable[];

int ghostcatd_led_new(struct ghostcatd_led **out,
		    struct ghostcatd_device *device,
		    struct ghostcatd_profile *profile,
		    struct ghostcat_led *lib_led,
		    unsigned int index);
struct ghostcatd_led *ghostcatd_led_free(struct ghostcatd_led *led);
const char *ghostcatd_led_get_path(struct ghostcatd_led *led);
int ghostcatd_led_resync(sd_bus *bus, struct ghostcatd_led *led);

DEFINE_TRIVIAL_CLEANUP_FUNC(struct ghostcatd_led *, ghostcatd_led_free);

/*
 * Devices
 */

extern const sd_bus_vtable ghostcatd_device_vtable[];

int ghostcatd_device_new(struct ghostcatd_device **out,
		       struct ghostcatd *ctx,
		       const char *sysname,
		       struct ghostcat_device *lib_device);
struct ghostcatd_device *ghostcatd_device_ref(struct ghostcatd_device *device);
struct ghostcatd_device *ghostcatd_device_unref(struct ghostcatd_device *device);
const char *ghostcatd_device_get_sysname(struct ghostcatd_device *device);
const char *ghostcatd_device_get_path(struct ghostcatd_device *device);
unsigned int ghostcatd_device_get_num_buttons(struct ghostcatd_device *device);
unsigned int ghostcatd_device_get_num_leds(struct ghostcatd_device *device);
int ghostcatd_device_resync(struct ghostcatd_device *device, sd_bus *bus);
int ghostcatd_device_poll_active_resolution(struct ghostcatd_device *device, sd_bus *bus);

bool ghostcatd_device_linked(struct ghostcatd_device *device);
void ghostcatd_device_link(struct ghostcatd_device *device);
void ghostcatd_device_unlink(struct ghostcatd_device *device);

DEFINE_TRIVIAL_CLEANUP_FUNC(struct ghostcatd_device *, ghostcatd_device_unref);

struct ghostcatd_device *ghostcatd_device_lookup(struct ghostcatd *ctx,
					     const char *name);
struct ghostcatd_device *ghostcatd_device_first(struct ghostcatd *ctx);
struct ghostcatd_device *ghostcatd_device_next(struct ghostcatd_device *device);

#define GHOSTCATD_DEVICE_FOREACH(_device, _ctx)		\
	for ((_device) = ghostcatd_device_first(_ctx);	\
	     (_device);					\
	     (_device) = ghostcatd_device_next(_device))

#define GHOSTCATD_DEVICE_FOREACH_SAFE(_device, _safe, _ctx)	\
	for (_device = ghostcatd_device_first(_ctx),		\
	     _safe = (_device) ? ghostcatd_device_next(_device) : NULL; \
	     (_device);						\
	     _device = (_safe),				\
	     _safe = (_safe) ? ghostcatd_device_next(_safe) : NULL)

/* Verify that _val is not -1. This traps DBus API errors where we end up
 * sending a valid-looking index across and then fail on the other side.
 *
 * do {} while(0) so we can terminate with a ; without the compiler
 * complaining about an empty statement;
 * */
#define verify_unsigned_int(_val) \
	do { if ((int)_val == -1) { \
		log_error("%s:%d - %s: expected unsigned int, got -1\n", __FILE__, __LINE__, __func__); \
		return -EINVAL; \
	} } while(0)

/*
 * Context
 */

struct ghostcatd {
	int api_version;

	sd_event *event;
	struct ghostcat *lib_ctx;
	struct udev_monitor *monitor;
	sd_event_source *timeout_source;
	sd_event_source *monitor_source;
	sd_bus *bus;

	RBTree device_map;
	size_t n_devices;

	const char **themes; /* NULL-terminated */
};

typedef void (*ghostcatd_callback_t)(void *userdata);

void ghostcatd_schedule_task(struct ghostcatd *ctx,
			   ghostcatd_callback_t callback,
			   void *userdata);

int ghostcatd_profile_notify_dirty(sd_bus *bus,
				 struct ghostcatd_profile *profile);
