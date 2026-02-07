/*
 * Copyright © 2015 Red Hat, Inc.
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

#pragma once

#include "config.h"
#include <linux/input.h>
#include <stdint.h>
#include <stdbool.h>
#include <assert.h>

#include "libghostcat.h"
#include "libghostcat-util.h"
#include "libghostcat-hidraw.h"

#ifdef NDEBUG
#error "libratbag relies on assert(). Do not define NDEBUG"
#endif

void
log_msg_va(struct ghostcat *ratbag,
	   enum ghostcat_log_priority priority,
	   const char *format,
	   va_list args)
	LIBGHOSTCAT_ATTRIBUTE_PRINTF(3, 0);
void log_msg(struct ghostcat *ratbag,
	enum ghostcat_log_priority priority,
	const char *format, ...)
	LIBGHOSTCAT_ATTRIBUTE_PRINTF(3, 4);
void
log_buffer(struct ghostcat *ratbag,
	enum ghostcat_log_priority priority,
	const char *header,
	const uint8_t *buf, size_t len);

#define log_raw(li_, ...) log_msg((li_), GHOSTCAT_LOG_PRIORITY_RAW, __VA_ARGS__)
#define log_debug(li_, ...) log_msg((li_), GHOSTCAT_LOG_PRIORITY_DEBUG, __VA_ARGS__)
#define log_info(li_, ...) log_msg((li_), GHOSTCAT_LOG_PRIORITY_INFO, __VA_ARGS__)
#define log_error(li_, ...) log_msg((li_), GHOSTCAT_LOG_PRIORITY_ERROR, __VA_ARGS__)
#define log_bug_kernel(li_, ...) log_msg((li_), GHOSTCAT_LOG_PRIORITY_ERROR, "kernel bug: " __VA_ARGS__)
#define log_bug_libratbag(li_, ...) log_msg((li_), GHOSTCAT_LOG_PRIORITY_ERROR, "libratbag bug: " __VA_ARGS__)
#define log_bug_client(li_, ...) log_msg((li_), GHOSTCAT_LOG_PRIORITY_ERROR, "client bug: " __VA_ARGS__)
#define log_buf_raw(li_, h_, buf_, len_) log_buffer(li_, GHOSTCAT_LOG_PRIORITY_RAW, h_, buf_, len_)
#define log_buf_debug(li_, h_, buf_, len_) log_buffer(li_, GHOSTCAT_LOG_PRIORITY_DEBUG, h_, buf_, len_)
#define log_buf_info(li_, h_, buf_, len_) log_buffer(li_, GHOSTCAT_LOG_PRIORITY_INFO, h_, buf_, len_)
#define log_buf_error(li_, h_, buf_, len_) log_buffer(li_, GHOSTCAT_LOG_PRIORITY_ERROR, h_, buf_, len_)

static inline void
cleanup_device(struct ghostcat_device **d)
{
	ghostcat_device_unref(*d);
}

static inline void
cleanup_profile(struct ghostcat_profile **p)
{
	ghostcat_profile_unref(*p);
}

static inline void
cleanup_resolution(struct ghostcat_resolution **r)
{
	ghostcat_resolution_unref(*r);
}

static inline void
cleanup_button(struct ghostcat_button **b)
{
	ghostcat_button_unref(*b);
}

static inline void
cleanup_led(struct ghostcat_led **l)
{
	ghostcat_led_unref(*l);
}

#define _cleanup_device_ _cleanup_(cleanup_device)
#define _cleanup_profile_ _cleanup_(cleanup_profile)
#define _cleanup_resolution_ _cleanup_(cleanup_resolution)
#define _cleanup_button_ _cleanup_(cleanup_button)
#define _cleanup_led_ _cleanup_(cleanup_led)

#define BUS_ANY					0xffff
#define VENDOR_ANY				0xffff
#define PRODUCT_ANY				0xffff
#define VERSION_ANY				0xffff

/* This struct is used by the test suite only */
struct ghostcat_test_device;

struct ghostcat_driver;
struct ghostcat_button_action;

struct ghostcat {
	const struct ghostcat_interface *interface;
	void *userdata;

	struct udev *udev;
	struct list drivers;
	struct list devices;

	int refcount;
	ghostcat_log_handler log_handler;
	enum ghostcat_log_priority log_priority;
};

#define MAX_CAP 1000

struct ghostcat_device {
	char *name;
	void *userdata;
	enum ghostcat_device_type devicetype;

	struct udev_device *udev_device;
	struct ghostcat_hidraw hidraw[MAX_HIDRAW];
	int refcount;
	struct input_id ids;
	struct ghostcat_driver *driver;
	struct ghostcat *ratbag;
	struct ghostcat_device_data *data;

	unsigned num_profiles;
	struct list profiles;

	unsigned num_buttons;
	unsigned num_leds;

	char* firmware_version;

	void *drv_data;

	struct list link;
};

/**
 * struct ghostcat_driver - user space driver for a ratbag device
 */
struct ghostcat_driver {
	/** A human-readable name of the driver */
	char *name;

	/** The id of the driver used to match with GHOSTCAT_DRIVER in udev */
	char *id;

	/**
	 * Callback called while trying to open a device by libratbag.
	 * This function should decide whether or not this driver will
	 * handle the given device.
	 *
	 * Return -ENODEV to ignore the device and let other drivers
	 * probe the device. Any other error code will stop the probing.
	 *
	 * If a matching driver is found it should initialize itself
	 * and synchronize all profiles with the current state on the device
	 * in this callback.
	 */
	int (*probe)(struct ghostcat_device *device);

	/**
	 * Callback called right before the struct ghostcat_device is
	 * unref-ed.
	 *
	 * In this callback, the extra memory allocated in probe should
	 * be freed.
	 */
	void (*remove)(struct ghostcat_device *device);

	/**
	 * Callback called when the driver should write any profiles that
	 * were modified back to the device.
	 *
	 * Both profile and button structs have a dirty variable that can
	 * be used to tell whether or not they've actually changed since
	 * the last commit. In order to reduce the amount of time
	 * committing takes, drivers should use this information to avoid
	 * writing back profiles and buttons that haven't actually changed.
	 */
	int (*commit)(struct ghostcat_device *device);

	/**
	 * Called to mark a previously written profile as active.
	 *
	 * There should be no need to write the profile here, a
	 * .write_profile() call is issued before calling this.
	 */
	int (*set_active_profile)(struct ghostcat_device *device, unsigned int index);

	/**
	 * Optional callback to refresh the active resolution state from
	 * hardware. Called periodically by ratbagd to detect physical
	 * DPI button presses. Returns 1 if state changed, 0 if unchanged,
	 * or a negative error code.
	 */
	int (*refresh_active_resolution)(struct ghostcat_device *device);

	/* private */
	int (*test_probe)(struct ghostcat_device *device, const void *data);

	struct list link;
};

struct ghostcat_resolution {
	struct ghostcat_profile *profile;
	int refcount;
	void *userdata;
	struct list link;
	unsigned index;

	unsigned int dpis[300];
	size_t ndpis;

	unsigned int dpi_x;	/**< x resolution in dpi */
	unsigned int dpi_y;	/**< y resolution in dpi */


	bool is_active;
	bool is_default;
	bool is_disabled;
	bool is_dpi_shift_target;
	bool dirty;
	uint32_t capabilities;
};

struct ghostcat_led {
	int refcount;
	void *userdata;
	struct list link;
	struct ghostcat_profile *profile;
	unsigned index;
	enum ghostcat_led_mode mode;
	uint32_t modes;		      /**< supported modes */
	struct ghostcat_color color;
	enum ghostcat_led_colordepth colordepth;
	unsigned int ms;              /**< duration of action in ms */
	unsigned int brightness;      /**< brightness of the LED */
	bool dirty;
};

struct ghostcat_profile {
	int refcount;
	void *userdata;
	char *name;

	struct list link;
	unsigned index;
	struct ghostcat_device *device;
	struct list buttons;
	void *drv_data;
	void *user_data;
	struct list resolutions;
	struct list leds;

	unsigned int hz;	/**< report rate in Hz */
	unsigned int rates[8];	/**< report rates available */
	size_t nrates;		/**< number of entries in rates */
	bool rate_dirty;

	int angle_snapping;
	bool angle_snapping_dirty;

	int debounce;	/**< debounce time in ms */
	bool debounce_dirty;
	unsigned int debounces[8];	/**< debounce times available */
	size_t ndebounces;		/**< number of entries in debounces */

	unsigned int num_resolutions;

	bool is_active;		/**< profile is the currently active one */
	bool is_active_dirty;

	bool is_enabled;
	bool dirty;       /**< profile changed since last commit */
	unsigned long capabilities[NLONGS(MAX_CAP)];
};

#define ghostcat_device_for_each_profile(device_, profile_) \
	list_for_each(profile_, &(device_)->profiles, link)

#define ghostcat_profile_for_each_button(profile_, button_) \
	list_for_each(button_, &(profile_)->buttons, link) \

#define ghostcat_profile_for_each_led(profile_, led_) \
	list_for_each(led_, &(profile_)->leds, link)

#define ghostcat_profile_for_each_resolution(profile_, resolution_) \
	list_for_each(resolution_, &(profile_)->resolutions, link)

#define BUTTON_ACTION_NONE \
 { .type = GHOSTCAT_BUTTON_ACTION_TYPE_NONE }
#define BUTTON_ACTION_UNKNOWN \
 { .type = GHOSTCAT_BUTTON_ACTION_TYPE_UNKNOWN }
#define BUTTON_ACTION_BUTTON(num_) \
 { .type = GHOSTCAT_BUTTON_ACTION_TYPE_BUTTON, \
	.action.button = num_ }
#define BUTTON_ACTION_SPECIAL(sp_) \
 { .type = GHOSTCAT_BUTTON_ACTION_TYPE_SPECIAL, \
	.action.special = sp_ }
#define BUTTON_ACTION_KEY(k_) \
 { .type = GHOSTCAT_BUTTON_ACTION_TYPE_KEY, \
	.action.key = k_ }
#define BUTTON_ACTION_MACRO \
 { .type = GHOSTCAT_BUTTON_ACTION_TYPE_MACRO, \
	/* FIXME: add the macro keys */ }

struct ghostcat_macro_event {
	enum ghostcat_macro_event_type type;
	union ghostcat_macro_evnt {
		unsigned int key;
		unsigned int timeout;
	} event;
};

#define MAX_MACRO_EVENTS 256
struct ghostcat_macro {
	char *name;
	char *group;
	struct ghostcat_macro_event events[MAX_MACRO_EVENTS];
};

struct ghostcat_button_macro {
	int refcount;
	struct ghostcat_macro macro;
};

#define MODIFIER_LEFTCTRL (1 << 0)
#define MODIFIER_LEFTSHIFT (1 << 1)
#define MODIFIER_LEFTALT (1 << 2)
#define MODIFIER_LEFTMETA (1 << 3)
#define MODIFIER_RIGHTCTRL (1 << 4)
#define MODIFIER_RIGHTSHIFT (1 << 5)
#define MODIFIER_RIGHTALT (1 << 6)
#define MODIFIER_RIGHTMETA (1 << 7)

struct ghostcat_button_action {
	enum ghostcat_button_action_type type;
	union ghostcat_btn_action {
		unsigned int button; /* action_type == button */
		enum ghostcat_button_action_special special; /* action_type == special */
		unsigned int key; /* action_type == key */
	} action;
	struct ghostcat_macro *macro; /* dynamically allocated, so kept aside */
};

struct ghostcat_button {
	int refcount;
	void *userdata;
	struct list link;
	struct ghostcat_profile *profile;
	unsigned index;
	struct ghostcat_button_action action;
	uint32_t action_caps;
	bool dirty; /* changed since last commit to device */
};

void
ghostcat_button_set_action(struct ghostcat_button *button,
			 const struct ghostcat_button_action *action);

static inline void
ghostcat_button_enable_action_type(struct ghostcat_button *button,
				 enum ghostcat_button_action_type type)
{
	button->action_caps |= 1 << type;
}

static inline int
ghostcat_open_path(struct ghostcat_device *device, const char *path, int flags)
{
	struct ghostcat *ratbag = device->ratbag;

	return ratbag->interface->open_restricted(path, flags, ratbag->userdata);
}

static inline void
ghostcat_close_fd(struct ghostcat_device *device, int fd)
{
	struct ghostcat *ratbag = device->ratbag;

	return ratbag->interface->close_restricted(fd, ratbag->userdata);
}

static inline void
ghostcat_set_drv_data(struct ghostcat_device *device, void *drv_data)
{
	device->drv_data = drv_data;
}

static inline void *
ghostcat_get_drv_data(const struct ghostcat_device *device)
{
	return device->drv_data;
}

int
ghostcat_device_init_profiles(struct ghostcat_device *device,
			    unsigned int num_profiles,
			    unsigned int num_resolutions,
			    unsigned int num_buttons,
			    unsigned int num_leds);

static inline void
ghostcat_profile_set_drv_data(struct ghostcat_profile *profile, void *drv_data)
{
	profile->drv_data = drv_data;
}

static inline void *
ghostcat_profile_get_drv_data(struct ghostcat_profile *profile)
{
	return profile->drv_data;
}

static inline void
ghostcat_profile_set_cap(struct ghostcat_profile *profile,
		       enum ghostcat_profile_capability cap)
{
	assert(cap <= MAX_CAP);

	long_set_bit(profile->capabilities, cap);
}

static inline int
ghostcat_button_action_match(const struct ghostcat_button_action *action,
			   const struct ghostcat_button_action *match)
{
	if (action->type != match->type)
		return 0;

	switch (action->type) {
	case GHOSTCAT_BUTTON_ACTION_TYPE_NONE:
		return 1;
	case GHOSTCAT_BUTTON_ACTION_TYPE_BUTTON:
		return match->action.button == action->action.button;
	case GHOSTCAT_BUTTON_ACTION_TYPE_KEY:
		return match->action.key == action->action.key;
	case GHOSTCAT_BUTTON_ACTION_TYPE_SPECIAL:
		return match->action.special == action->action.special;
	case GHOSTCAT_BUTTON_ACTION_TYPE_MACRO:
		/* TODO: check if events match. */
		return 1;
	default:
		break;
	}

	return 0;
}

int
ghostcat_action_macro_num_keys(const struct ghostcat_button_action *action);

int
ghostcat_button_macro_new_from_keycode(struct ghostcat_button *button,
				     unsigned int key,
				     unsigned int modifiers);

int
ghostcat_action_keycode_from_macro(const struct ghostcat_button_action *action,
				 unsigned int *key_out,
				 unsigned int *modifiers_out);

static inline void
ghostcat_resolution_set_resolution(struct ghostcat_resolution *res,
				 int dpi_x, int dpi_y)
{
	res->dpi_x = dpi_x;
	res->dpi_y = dpi_y;
}

static inline void
ghostcat_resolution_set_dpi_list_from_range(struct ghostcat_resolution *res,
					  unsigned int min, unsigned int max)
{
	unsigned int stepsize = 50;
	unsigned int dpi = min;
	bool maxed_out = false;

	res->ndpis = 0;

	while (res->ndpis < ARRAY_LENGTH(res->dpis)) {
		if (dpi > (unsigned)max) {
			maxed_out = true;
			break;
		}

		res->dpis[res->ndpis] = dpi;
		res->ndpis++;

		if (dpi < 1000)
			stepsize = 50;
		else if (dpi < 2600)
			stepsize = 100;
		else if (dpi < 5000)
			stepsize = 200;
		else
			stepsize = 500;

		dpi += stepsize;
	}

	if (!maxed_out)
		log_bug_libratbag(res->profile->device->ratbag,
				  "%s: resolution range exceeds available space.\n",
				  res->profile->device->name);
}

static inline void
ghostcat_resolution_set_dpi_list(struct ghostcat_resolution *res,
			       const unsigned int *dpis,
			       size_t ndpis)
{
	assert(ndpis <= ARRAY_LENGTH(res->dpis));
	_Static_assert(sizeof(*dpis) == sizeof(*res->dpis), "Mismatching size");

	for (size_t i = 0; i < ndpis; i++) {
		res->dpis[i] = dpis[i];
		if (i > 0)
			assert(dpis[i] > dpis[i - 1]);
	}
	res->ndpis = ndpis;
}

static inline void
ghostcat_profile_set_report_rate_list(struct ghostcat_profile *profile,
				    const unsigned int *rates,
				    size_t nrates)
{
	assert(nrates <= ARRAY_LENGTH(profile->rates));
	_Static_assert(sizeof(*rates) == sizeof(*profile->rates), "Mismatching size");

	for (size_t i = 0; i < nrates; i++) {
		profile->rates[i] = rates[i];
		if (i > 0)
			assert(rates[i] > rates[i - 1]);
	}
	profile->nrates = nrates;
}

static inline void
ghostcat_profile_set_debounce_list(struct ghostcat_profile *profile,
				 const unsigned int *values,
				 size_t nvalues)
{
	assert(nvalues <= ARRAY_LENGTH(profile->debounces));
	_Static_assert(sizeof(*values) == sizeof(*profile->debounces), "Mismatching size");

	for (size_t i = 0; i < nvalues; i++) {
		profile->debounces[i] = values[i];
		if (i > 0)
			assert(values[i] > values[i - 1]);
	}
	profile->ndebounces = nvalues;
}

static inline void
ghostcat_resolution_set_cap(struct ghostcat_resolution *res,
			  enum ghostcat_resolution_capability cap)
{
	assert(cap <= GHOSTCAT_RESOLUTION_CAP_DISABLE);

	res->capabilities |= (1 << cap);
}

static inline void
ghostcat_led_set_mode_capability(struct ghostcat_led *led,
			       enum ghostcat_led_mode mode)
{
	assert(mode <= GHOSTCAT_LED_BREATHING);
	assert(mode < sizeof(led->modes) * 8);

	led->modes |= (1 << mode);
}

/* list of all supported drivers */
extern struct ghostcat_driver etekcity_driver;
extern struct ghostcat_driver hidpp20_driver;
extern struct ghostcat_driver hidpp10_driver;
extern struct ghostcat_driver logitech_g300_driver;
extern struct ghostcat_driver logitech_g600_driver;
extern struct ghostcat_driver marsgaming_driver;
extern struct ghostcat_driver roccat_driver;
extern struct ghostcat_driver roccat_kone_pure_driver;
extern struct ghostcat_driver roccat_emp_driver;
extern struct ghostcat_driver gskill_driver;
extern struct ghostcat_driver steelseries_driver;
extern struct ghostcat_driver asus_driver;
extern struct ghostcat_driver sinowealth_driver;
extern struct ghostcat_driver sinowealth_nubwo_driver;
extern struct ghostcat_driver openinput_driver;

struct ghostcat_device*
ghostcat_device_new(struct ghostcat *ratbag, struct udev_device *udev_device,
		  const char *name, const struct input_id *id);
void
ghostcat_device_destroy(struct ghostcat_device *device);

const char *
ghostcat_device_get_udev_property(const struct ghostcat_device* device,
				const char *name);

bool
ghostcat_assign_driver(struct ghostcat_device *device,
		     const struct input_id *dev_id,
		     const struct ghostcat_test_device *test_device);

void
ghostcat_register_driver(struct ghostcat *ratbag, struct ghostcat_driver *driver);

void
ghostcat_button_copy_macro(struct ghostcat_button *button,
			 const struct ghostcat_button_macro *macro);
