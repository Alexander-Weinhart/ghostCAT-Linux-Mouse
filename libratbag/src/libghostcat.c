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

#include "config.h"
#include <assert.h>
#include <errno.h>
#include <libudev.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <limits.h>

#include "usb-ids.h"
#include "libghostcat-private.h"
#include "libghostcat-util.h"
#include "libghostcat-data.h"

static enum ghostcat_error_code
error_code(enum ghostcat_error_code code)
{
	switch(code) {
	case GHOSTCAT_SUCCESS:
	case GHOSTCAT_ERROR_DEVICE:
	case GHOSTCAT_ERROR_CAPABILITY:
	case GHOSTCAT_ERROR_VALUE:
	case GHOSTCAT_ERROR_SYSTEM:
	case GHOSTCAT_ERROR_IMPLEMENTATION:
		break;
	default:
		assert(!"Invalid error code. This is a library bug.");
	}
	return code;
}

static void
ghostcat_profile_destroy(struct ghostcat_profile *profile);
static void
ghostcat_button_destroy(struct ghostcat_button *button);
static void
ghostcat_led_destroy(struct ghostcat_led *led);
static void
ghostcat_resolution_destroy(struct ghostcat_resolution *resolution);

static void
ghostcat_default_log_func(struct ghostcat *ratbag,
			enum ghostcat_log_priority priority,
			const char *format, va_list args)
{
	const char *prefix;
	FILE *out = stdout;

	switch(priority) {
	case GHOSTCAT_LOG_PRIORITY_RAW:
		prefix = "raw";
		break;
	case GHOSTCAT_LOG_PRIORITY_DEBUG:
		prefix = "debug";
		break;
	case GHOSTCAT_LOG_PRIORITY_INFO:
		prefix = "info";
		break;
	case GHOSTCAT_LOG_PRIORITY_ERROR:
		prefix = "error";
		out = stderr;
		break;
	default:
		prefix="<invalid priority>";
		break;
	}

	fprintf(out, "ratbag %s: ", prefix);
	vfprintf(out, format, args);
}

void
log_msg_va(struct ghostcat *ratbag,
	   enum ghostcat_log_priority priority,
	   const char *format,
	   va_list args)
{
	if (ratbag->log_handler &&
	    ratbag->log_priority <= priority)
		ratbag->log_handler(ratbag, priority, format, args);
}

void
log_msg(struct ghostcat *ratbag,
	enum ghostcat_log_priority priority,
	const char *format, ...)
{
	va_list args;

	va_start(args, format);
	log_msg_va(ratbag, priority, format, args);
	va_end(args);
}

void
log_buffer(struct ghostcat *ratbag,
	enum ghostcat_log_priority priority,
	const char *header,
	const uint8_t *buf, size_t len)
{
	_cleanup_free_ char *output_buf = NULL;
	char *sep = "";
	unsigned int i, n;
	unsigned int buf_len;

	if (ratbag->log_handler &&
	    ratbag->log_priority > priority)
		return;

	buf_len = header ? strlen(header) : 0;
	buf_len += len * 3;
	buf_len += 1; /* terminating '\0' */

	output_buf = zalloc(buf_len);
	n = 0;
	if (header)
		n += snprintf_safe(output_buf, buf_len - n, "%s", header);

	for (i = 0; i < len; ++i) {
		n += snprintf_safe(&output_buf[n], buf_len - n, "%s%02x", sep, buf[i] & 0xFF);
		sep = " ";
	}

	log_msg(ratbag, priority, "%s\n", output_buf);
}

LIBGHOSTCAT_EXPORT void
ghostcat_log_set_priority(struct ghostcat *ratbag,
			enum ghostcat_log_priority priority)
{
	switch (priority) {
	case GHOSTCAT_LOG_PRIORITY_RAW:
	case GHOSTCAT_LOG_PRIORITY_DEBUG:
	case GHOSTCAT_LOG_PRIORITY_INFO:
	case GHOSTCAT_LOG_PRIORITY_ERROR:
		break;
	default:
		log_bug_client(ratbag,
			       "Invalid log priority %d. Using INFO instead\n",
			       priority);
		priority = GHOSTCAT_LOG_PRIORITY_INFO;
	}
	ratbag->log_priority = priority;
}

LIBGHOSTCAT_EXPORT enum ghostcat_log_priority
ghostcat_log_get_priority(const struct ghostcat *ratbag)
{
	return ratbag->log_priority;
}

LIBGHOSTCAT_EXPORT void
ghostcat_log_set_handler(struct ghostcat *ratbag,
		       ghostcat_log_handler log_handler)
{
	ratbag->log_handler = log_handler;
}

struct ghostcat_device*
ghostcat_device_new(struct ghostcat *ratbag, struct udev_device *udev_device,
		  const char *name, const struct input_id *id)
{
	struct ghostcat_device *device = NULL;

	device = zalloc(sizeof(*device));
	device->name = strdup_safe(name);
	device->ratbag = ghostcat_ref(ratbag);
	device->refcount = 1;
	device->udev_device = udev_device_ref(udev_device);
	device->ids = *id;
	device->data = ghostcat_device_data_new_for_id(ratbag, id);

	if (device->data != NULL)
		device->devicetype = ghostcat_device_data_get_device_type(device->data);

	list_init(&device->profiles);

	list_insert(&ratbag->devices, &device->link);

	return device;
}

void
ghostcat_device_destroy(struct ghostcat_device *device)
{
	struct ghostcat_profile *profile, *next;

	if (!device)
		return;

	/* if we get to the point where the device is destroyed, profiles,
	 * buttons, etc. are at a refcount of 0, so we can destroy
	 * everything */
	if (device->driver && device->driver->remove)
		device->driver->remove(device);

	list_for_each_safe(profile, next, &device->profiles, link)
		ghostcat_profile_destroy(profile);

	if (device->udev_device)
		udev_device_unref(device->udev_device);

	list_remove(&device->link);

	ghostcat_unref(device->ratbag);
	ghostcat_device_data_unref(device->data);
	free(device->name);
	free(device->firmware_version);
	free(device);
}

static inline bool
ghostcat_sanity_check_device(struct ghostcat_device *device)
{
	struct ghostcat *ratbag = device->ratbag;
	struct ghostcat_profile *profile = NULL;
	bool has_active = false;
	unsigned int nres;
	bool rc = false;

	/* arbitrary number: max 16 profiles, does any mouse have more? but
	 * since we have num_profiles unsigned, it also checks for
	 * accidental negative */
	if (device->num_profiles == 0 || device->num_profiles > 16) {
		log_bug_libratbag(ratbag,
				  "%s: invalid number of profiles (%d)\n",
				  device->name,
				  device->num_profiles);
		goto out;
	}

	ghostcat_device_for_each_profile(device, profile) {
		struct ghostcat_resolution *resolution;
		unsigned int vals[300];
		unsigned int nvals = ARRAY_LENGTH(vals);

		/* Allow max 1 active profile */
		if (profile->is_active) {
			if (has_active) {
				log_bug_libratbag(ratbag,
						  "%s: multiple active profiles\n",
						  device->name);
				goto out;
			}
			has_active = true;
		}

		nres = ghostcat_profile_get_num_resolutions(profile);
		if (nres > 16) {
				log_bug_libratbag(ratbag,
						  "%s: invalid number of resolutions (%d)\n",
						  device->name,
						  nres);
				goto out;
		}

		ghostcat_profile_for_each_resolution(profile, resolution) {
			nvals = ghostcat_resolution_get_dpi_list(resolution, vals, nvals);
			if (nvals == 0) {
				log_bug_libratbag(ratbag,
						  "%s: invalid dpi list\n",
						  device->name);
				goto out;
			}

		}

		nvals = ghostcat_profile_get_report_rate_list(profile, vals, nvals);
		if (nvals == 0) {
			log_bug_libratbag(ratbag,
					  "%s: invalid report rate list\n",
					  device->name);
			goto out;
		}

		if (profile->dirty) {
			log_bug_libratbag(ratbag,
					  "%s: profile is dirty while probing\n",
					  device->name);
			/* Don't bail yet, as we may have some drivers that do this. */
		}
	}

	/* Require 1 active profile */
	if (!has_active) {
		log_bug_libratbag(ratbag,
				  "%s: no profile set as active profile\n",
				  device->name);
		goto out;
	}

	rc = true;

out:
	return rc;
}

static inline bool
ghostcat_try_driver(struct ghostcat_device *device,
		   const struct input_id *dev_id,
		   const char *driver_name,
		   const struct ghostcat_test_device *test_device)
{
	struct ghostcat *ratbag = device->ratbag;
	struct ghostcat_driver *driver;
	int rc;

	list_for_each(driver, &ratbag->drivers, link) {
		if (streq(driver->id, driver_name)) {
			device->driver = driver;
			break;
		}
	}

	if (!device->driver) {
		log_error(ratbag, "%s: driver '%s' does not exist\n",
			  device->name, driver_name);
		goto error;
	}

	if (test_device)
		rc = device->driver->test_probe(device, test_device);
	else
		rc = device->driver->probe(device);
	if (rc == 0) {
		if (!ghostcat_sanity_check_device(device)) {
			goto error;
		} else {
			log_debug(ratbag,
				  "driver match found: %s\n",
				  device->driver->name);
			return true;
		}
	}

	if (rc != -ENODEV)
		log_error(ratbag, "%s: error opening hidraw node (%s)\n",
			  device->name, strerror(-rc));

	device->driver = NULL;
error:
	return false;
}

bool
ghostcat_assign_driver(struct ghostcat_device *device,
		     const struct input_id *dev_id,
		     const struct ghostcat_test_device *test_device)
{
	const char *driver_name;

	if (!test_device) {
		driver_name = ghostcat_device_data_get_driver(device->data);
	} else {
		log_debug(device->ratbag, "This is a test device\n");
		driver_name = "test_driver";
	}

	log_debug(device->ratbag, "device assigned driver %s\n", driver_name);
	return ghostcat_try_driver(device, dev_id, driver_name, test_device);
}

static char *
get_device_name(struct udev_device *device)
{
	const char *prop;

	prop = udev_prop_value(device, "HID_NAME");

	return strdup_safe(prop);
}

static inline int
get_product_id(struct udev_device *device, struct input_id *id)
{
	const char *product;
	struct input_id ids  = {0};
	int rc;

	product = udev_prop_value(device, "HID_ID");
	if (!product)
		return -1;

	rc = sscanf(product, "%hx:%hx:%hx", &ids.bustype, &ids.vendor, &ids.product);
	if (rc != 3)
		return -1;

	*id = ids;
	return 0;
}

LIBGHOSTCAT_EXPORT enum ghostcat_error_code
ghostcat_device_new_from_udev_device(struct ghostcat *ratbag,
				   struct udev_device *udev_device,
				   struct ghostcat_device **device_out)
{
	struct ghostcat_device *device = NULL;
	enum ghostcat_error_code error = GHOSTCAT_ERROR_DEVICE;
	_cleanup_free_ char *name = NULL;
	struct input_id id;

	assert(ratbag != NULL);
	assert(udev_device != NULL);
	assert(device_out != NULL);

	if (get_product_id(udev_device, &id) != 0)
		goto out_err;

	if ((name = get_device_name(udev_device)) == 0)
		goto out_err;

	log_debug(ratbag, "New device: %s\n", name);

	device = ghostcat_device_new(ratbag, udev_device, name, &id);
	if (!device || !device->data)
		goto out_err;

	if (!ghostcat_assign_driver(device, &device->ids, NULL))
		goto out_err;

	error = GHOSTCAT_SUCCESS;

out_err:

	if (error != GHOSTCAT_SUCCESS)
		ghostcat_device_destroy(device);
	else
		*device_out = device;

	return error_code(error);
}

LIBGHOSTCAT_EXPORT struct ghostcat_device *
ghostcat_device_ref(struct ghostcat_device *device)
{
	assert(device->refcount < INT_MAX);

	device->refcount++;
	return device;
}

LIBGHOSTCAT_EXPORT struct ghostcat_device *
ghostcat_device_unref(struct ghostcat_device *device)
{
	if (device == NULL)
		return NULL;

	assert(device->refcount > 0);
	device->refcount--;
	if (device->refcount == 0)
		ghostcat_device_destroy(device);

	return NULL;
}

LIBGHOSTCAT_EXPORT const char *
ghostcat_device_get_name(const struct ghostcat_device* device)
{
	return device->name;
}

LIBGHOSTCAT_EXPORT enum ghostcat_device_type
ghostcat_device_get_device_type(const struct ghostcat_device *device)
{
	return device->devicetype;
}

LIBGHOSTCAT_EXPORT const char *
ghostcat_device_get_bustype(const struct ghostcat_device *device)
{
	switch (device->ids.bustype) {
	case BUS_USB:
		return "usb";
	case BUS_BLUETOOTH:
		return "bluetooth";
	default:
		return NULL;
	}
}

LIBGHOSTCAT_EXPORT uint32_t
ghostcat_device_get_vendor_id(const struct ghostcat_device *device)
{
	return device->ids.vendor;
}

LIBGHOSTCAT_EXPORT uint32_t
ghostcat_device_get_product_id(const struct ghostcat_device *device)
{
	return device->ids.product;
}

LIBGHOSTCAT_EXPORT uint32_t
ghostcat_device_get_product_version(const struct ghostcat_device *device)
{
	/* change this when we have a need for it, i.e. when we start supporting devices
	 * where the USB ID gets reused */
	return 0;
}

void
ghostcat_register_driver(struct ghostcat *ratbag, struct ghostcat_driver *driver)
{
	if (!driver->name) {
		log_bug_libratbag(ratbag, "Driver is missing name\n");
		return;
	}

	if (!driver->probe || !driver->remove) {
		log_bug_libratbag(ratbag, "Driver %s is incomplete.\n", driver->name);
		return;
	}
	list_insert(&ratbag->drivers, &driver->link);
}

LIBGHOSTCAT_EXPORT struct ghostcat *
ghostcat_create_context(const struct ghostcat_interface *interface,
		      void *userdata)
{
	struct ghostcat *ratbag;

	assert(interface != NULL);
	assert(interface->open_restricted != NULL);
	assert(interface->close_restricted != NULL);

	ratbag = zalloc(sizeof(*ratbag));
	ratbag->refcount = 1;
	ratbag->interface = interface;
	ratbag->userdata = userdata;

	list_init(&ratbag->drivers);
	list_init(&ratbag->devices);
	ratbag->udev = udev_new();
	if (!ratbag->udev) {
		free(ratbag);
		return NULL;
	}

	ratbag->log_handler = ghostcat_default_log_func;
	ratbag->log_priority = GHOSTCAT_LOG_PRIORITY_INFO;

	ghostcat_register_driver(ratbag, &etekcity_driver);
	ghostcat_register_driver(ratbag, &hidpp20_driver);
	ghostcat_register_driver(ratbag, &hidpp10_driver);
	ghostcat_register_driver(ratbag, &logitech_g300_driver);
	ghostcat_register_driver(ratbag, &logitech_g600_driver);
	ghostcat_register_driver(ratbag, &marsgaming_driver);
	ghostcat_register_driver(ratbag, &roccat_driver);
	ghostcat_register_driver(ratbag, &roccat_kone_pure_driver);
	ghostcat_register_driver(ratbag, &roccat_emp_driver);
	ghostcat_register_driver(ratbag, &gskill_driver);
	ghostcat_register_driver(ratbag, &steelseries_driver);
	ghostcat_register_driver(ratbag, &asus_driver);
	ghostcat_register_driver(ratbag, &sinowealth_driver);
	ghostcat_register_driver(ratbag, &sinowealth_nubwo_driver);
	ghostcat_register_driver(ratbag, &openinput_driver);

	return ratbag;
}

LIBGHOSTCAT_EXPORT struct ghostcat *
ghostcat_ref(struct ghostcat *ratbag)
{
	ratbag->refcount++;
	return ratbag;
}

LIBGHOSTCAT_EXPORT struct ghostcat *
ghostcat_unref(struct ghostcat *ratbag)
{
	if (ratbag == NULL)
		return NULL;

	assert(ratbag->refcount > 0);
	ratbag->refcount--;
	if (ratbag->refcount == 0) {
		ratbag->udev = udev_unref(ratbag->udev);
		free(ratbag);
	}

	return NULL;
}

static struct ghostcat_button *
ghostcat_create_button(struct ghostcat_profile *profile, unsigned int index)
{
	struct ghostcat_button *button;

	button = zalloc(sizeof(*button));
	button->refcount = 0;
	button->profile = profile;
	button->index = index;

	list_append(&profile->buttons, &button->link);

	return button;
}

static struct ghostcat_led *
ghostcat_create_led(struct ghostcat_profile *profile, unsigned int index)
{
	struct ghostcat_led *led;

	led = zalloc(sizeof(*led));
	led->refcount = 0;
	led->profile = profile;
	led->index = index;
	led->colordepth = GHOSTCAT_LED_COLORDEPTH_RGB_888;

	list_append(&profile->leds, &led->link);

	return led;
}

LIBGHOSTCAT_EXPORT bool
ghostcat_profile_has_capability(const struct ghostcat_profile *profile,
			      enum ghostcat_profile_capability cap)
{
	if (cap == GHOSTCAT_PROFILE_CAP_NONE || cap >= MAX_CAP)
		abort();

	return long_bit_is_set(profile->capabilities, cap);
}

static inline void
ghostcat_create_resolution(struct ghostcat_profile *profile, int index)
{
	struct ghostcat_resolution *res;

	res = zalloc(sizeof(*res));
	res->refcount = 0;
	res->profile = profile;
	res->index = index;

	list_append(&profile->resolutions, &res->link);

	profile->num_resolutions++;
}


static struct ghostcat_profile *
ghostcat_create_profile(struct ghostcat_device *device,
		      unsigned int index,
		      unsigned int num_resolutions,
		      unsigned int num_buttons,
		      unsigned int num_leds)
{
	struct ghostcat_profile *profile;
	unsigned i;

	profile = zalloc(sizeof(*profile));
	profile->refcount = 0;
	profile->device = device;
	profile->index = index;
	profile->num_resolutions = 0;
	profile->is_enabled = true;
	profile->name = NULL;
	profile->angle_snapping = -1;
	profile->debounce = -1;

	list_append(&device->profiles, &profile->link);
	list_init(&profile->buttons);
	list_init(&profile->leds);
	list_init(&profile->resolutions);

	profile->device->num_buttons = num_buttons;
	profile->device->num_leds = num_leds;

	for (i = 0; i < num_resolutions; i++)
		ghostcat_create_resolution(profile, i);

	for (i = 0; i < num_buttons; i++)
		ghostcat_create_button(profile, i);

	for (i = 0; i < num_leds; i++)
		ghostcat_create_led(profile, i);

	return profile;
}

int
ghostcat_device_init_profiles(struct ghostcat_device *device,
			    unsigned int num_profiles,
			    unsigned int num_resolutions,
			    unsigned int num_buttons,
			    unsigned int num_leds)
{
	unsigned int i;

	for (i = 0; i < num_profiles; i++) {
		ghostcat_create_profile(device, i, num_resolutions, num_buttons, num_leds);
	}

	device->num_profiles = num_profiles;

	return 0;
}

LIBGHOSTCAT_EXPORT struct ghostcat_profile *
ghostcat_profile_ref(struct ghostcat_profile *profile)
{
	assert(profile->refcount < INT_MAX);

	ghostcat_device_ref(profile->device);
	profile->refcount++;
	return profile;
}

static void
ghostcat_profile_destroy(struct ghostcat_profile *profile)
{
	struct ghostcat_button *button, *b_next;
	struct ghostcat_led *led, *l_next;
	struct ghostcat_resolution *res, *r_next;

	/* if we get to the point where the profile is destroyed, buttons,
	 * resolutions , etc. are at a refcount of 0, so we can destroy
	 * everything */
	list_for_each_safe(button, b_next, &profile->buttons, link)
		ghostcat_button_destroy(button);

	list_for_each_safe(led, l_next, &profile->leds, link)
		ghostcat_led_destroy(led);

	list_for_each_safe(res, r_next, &profile->resolutions, link)
		ghostcat_resolution_destroy(res);

	free(profile->name);

	list_remove(&profile->link);
	free(profile);
}

LIBGHOSTCAT_EXPORT struct ghostcat_profile *
ghostcat_profile_unref(struct ghostcat_profile *profile)
{
	if (profile == NULL)
		return NULL;

	assert(profile->refcount > 0);
	profile->refcount--;

	ghostcat_device_unref(profile->device);

	return NULL;
}

LIBGHOSTCAT_EXPORT struct ghostcat_profile *
ghostcat_device_get_profile(struct ghostcat_device *device, unsigned int index)
{
	struct ghostcat_profile *profile;

	if (index >= ghostcat_device_get_num_profiles(device)) {
		log_bug_client(device->ratbag, "Requested invalid profile %d\n", index);
		return NULL;
	}

	list_for_each(profile, &device->profiles, link) {
		if (profile->index == index)
			return ghostcat_profile_ref(profile);
	}

	log_bug_libratbag(device->ratbag, "Profile %d not found\n", index);

	return NULL;
}

LIBGHOSTCAT_EXPORT enum ghostcat_error_code
ghostcat_profile_set_enabled(struct ghostcat_profile *profile, bool enabled)
{
	if (!ghostcat_profile_has_capability(profile, GHOSTCAT_PROFILE_CAP_DISABLE))
		return GHOSTCAT_ERROR_CAPABILITY;

	if (profile->is_active && !enabled)
		return GHOSTCAT_ERROR_VALUE;

	profile->is_enabled = enabled;
	profile->dirty = true;

	return GHOSTCAT_SUCCESS;
}

LIBGHOSTCAT_EXPORT bool
ghostcat_profile_is_active(const struct ghostcat_profile *profile)
{
	return !!profile->is_active;
}

LIBGHOSTCAT_EXPORT bool
ghostcat_profile_is_dirty(const struct ghostcat_profile *profile)
{
	return !!profile->dirty;
}

LIBGHOSTCAT_EXPORT bool
ghostcat_profile_is_enabled(const struct ghostcat_profile *profile)
{
	return !!profile->is_enabled;
}

LIBGHOSTCAT_EXPORT unsigned int
ghostcat_device_get_num_profiles(const struct ghostcat_device *device)
{
	return device->num_profiles;
}

LIBGHOSTCAT_EXPORT unsigned int
ghostcat_device_get_num_buttons(const struct ghostcat_device *device)
{
	return device->num_buttons;
}

LIBGHOSTCAT_EXPORT unsigned int
ghostcat_device_get_num_leds(const struct ghostcat_device *device)
{
	return device->num_leds;
}

LIBGHOSTCAT_EXPORT enum ghostcat_error_code
ghostcat_device_commit(struct ghostcat_device *device)
{
	struct ghostcat_profile *profile;
	struct ghostcat_button *button;
	struct ghostcat_led *led;
	struct ghostcat_resolution *resolution;
	int rc;

	if (device->driver->commit == NULL) {
		log_error(device->ratbag,
			  "Trying to commit with a driver that doesn't support committing\n");
		return GHOSTCAT_ERROR_CAPABILITY;
	}

	rc = device->driver->commit(device);
	if (rc)
		return GHOSTCAT_ERROR_DEVICE;

	list_for_each(profile, &device->profiles, link) {
		profile->dirty = false;

		profile->angle_snapping_dirty = false;
		profile->debounce_dirty = false;
		profile->rate_dirty = false;

		list_for_each(button, &profile->buttons, link)
			button->dirty = false;

		list_for_each(led, &profile->leds, link)
			led->dirty = false;

		list_for_each(resolution, &profile->resolutions, link)
			resolution->dirty = false;

		/* TODO: think if this should be moved into `driver-commit`. */
		if (profile->is_active_dirty && profile->is_active) {
			if (device->driver->set_active_profile == NULL)
				return GHOSTCAT_ERROR_IMPLEMENTATION;

			rc = device->driver->set_active_profile(device, profile->index);
			if (rc)
				return GHOSTCAT_ERROR_DEVICE;
		}
		profile->is_active_dirty = false;
	}

	return GHOSTCAT_SUCCESS;
}

LIBGHOSTCAT_EXPORT enum ghostcat_error_code
ghostcat_profile_set_active(struct ghostcat_profile *profile)
{
	struct ghostcat_device *device = profile->device;
	struct ghostcat_profile *p;

	if (!profile->is_enabled)
		return GHOSTCAT_ERROR_VALUE;

	if (device->num_profiles == 1)
		return GHOSTCAT_SUCCESS;

	list_for_each(p, &device->profiles, link) {
		if (p->is_active) {
			p->is_active = false;
			p->is_active_dirty = true;
			p->dirty = true;
		}
	}

	profile->is_active = true;
	profile->is_active_dirty = true;
	profile->dirty = true;
	return GHOSTCAT_SUCCESS;
}

LIBGHOSTCAT_EXPORT unsigned int
ghostcat_profile_get_num_resolutions(const struct ghostcat_profile *profile)
{
	return profile->num_resolutions;
}

LIBGHOSTCAT_EXPORT struct ghostcat_resolution *
ghostcat_profile_get_resolution(struct ghostcat_profile *profile, unsigned int idx)
{
	struct ghostcat_device *device = profile->device;
	struct ghostcat_resolution *res;
	unsigned max = ghostcat_profile_get_num_resolutions(profile);

	if (idx >= max) {
		log_bug_client(profile->device->ratbag,
			       "Requested invalid resolution %d\n", idx);
		return NULL;
	}

	ghostcat_profile_for_each_resolution(profile, res) {
		if (res->index == idx)
			return ghostcat_resolution_ref(res);
	}

	log_bug_libratbag(device->ratbag, "Resolution %d, profile %d not found\n",
			  idx, profile->index);

	return NULL;
}

LIBGHOSTCAT_EXPORT struct ghostcat_resolution *
ghostcat_resolution_ref(struct ghostcat_resolution *resolution)
{
	assert(resolution->refcount < INT_MAX);

	ghostcat_profile_ref(resolution->profile);
	resolution->refcount++;
	return resolution;
}

LIBGHOSTCAT_EXPORT struct ghostcat_resolution *
ghostcat_resolution_unref(struct ghostcat_resolution *resolution)
{
	if (resolution == NULL)
		return NULL;

	assert(resolution->refcount > 0);
	resolution->refcount--;

	ghostcat_profile_unref(resolution->profile);

	return NULL;
}

LIBGHOSTCAT_EXPORT bool
ghostcat_resolution_has_capability(const struct ghostcat_resolution *resolution,
				 enum ghostcat_resolution_capability cap)
{
	assert(cap <= GHOSTCAT_RESOLUTION_CAP_DISABLE);

	return !!(resolution->capabilities & (1 << cap));
}

static inline bool
resolution_has_dpi(const struct ghostcat_resolution *resolution,
		   unsigned int dpi)
{
	for (size_t i = 0; i < resolution->ndpis; i++) {
		if (dpi == resolution->dpis[i])
			return true;
	}

	return false;
}

LIBGHOSTCAT_EXPORT enum ghostcat_error_code
ghostcat_resolution_set_dpi(struct ghostcat_resolution *resolution,
			  unsigned int dpi)
{
	struct ghostcat_profile *profile = resolution->profile;

	if (!resolution_has_dpi(resolution, dpi))
		return GHOSTCAT_ERROR_VALUE;

	if (resolution->dpi_x != dpi || resolution->dpi_y != dpi) {
		resolution->dpi_x = dpi;
		resolution->dpi_y = dpi;
		resolution->dirty = true;
		profile->dirty = true;
	}

	return GHOSTCAT_SUCCESS;
}

LIBGHOSTCAT_EXPORT enum ghostcat_error_code
ghostcat_resolution_set_dpi_xy(struct ghostcat_resolution *resolution,
			     unsigned int x, unsigned int y)
{
	struct ghostcat_profile *profile = resolution->profile;

	if (!ghostcat_resolution_has_capability(resolution,
					      GHOSTCAT_RESOLUTION_CAP_SEPARATE_XY_RESOLUTION))
		return GHOSTCAT_ERROR_CAPABILITY;

	if ((x == 0 && y != 0) || (x != 0 && y == 0))
		return GHOSTCAT_ERROR_VALUE;

	if (!resolution_has_dpi(resolution, x) || !resolution_has_dpi(resolution, y))
		return GHOSTCAT_ERROR_VALUE;

	if (resolution->dpi_x != x || resolution->dpi_y != y) {
		resolution->dpi_x = x;
		resolution->dpi_y = y;
		resolution->dirty = true;
		profile->dirty = true;
	}

	return GHOSTCAT_SUCCESS;
}

LIBGHOSTCAT_EXPORT enum ghostcat_error_code
ghostcat_profile_set_report_rate(struct ghostcat_profile *profile,
			       unsigned int hz)
{
	if (profile->hz != hz) {
		profile->hz = hz;
		profile->dirty = true;
		profile->rate_dirty = true;
	}

	return GHOSTCAT_SUCCESS;
}

LIBGHOSTCAT_EXPORT enum ghostcat_error_code
ghostcat_profile_set_angle_snapping(struct ghostcat_profile *profile,
				  int value)
{
	if (profile->angle_snapping != value) {
		profile->angle_snapping = value;
		profile->dirty = true;
		profile->angle_snapping_dirty = true;
	}

	return GHOSTCAT_SUCCESS;
}

LIBGHOSTCAT_EXPORT enum ghostcat_error_code
ghostcat_profile_set_debounce(struct ghostcat_profile *profile,
			    int value)
{
	if (profile->debounce != value) {
		profile->debounce = value;
		profile->dirty = true;
		profile->debounce_dirty = true;
	}

	return GHOSTCAT_SUCCESS;
}

LIBGHOSTCAT_EXPORT int
ghostcat_resolution_get_dpi(const struct ghostcat_resolution *resolution)
{
	return resolution->dpi_x;
}

LIBGHOSTCAT_EXPORT size_t
ghostcat_resolution_get_dpi_list(const struct ghostcat_resolution *resolution,
			       unsigned int *resolutions,
			       size_t nres)
{
	_Static_assert(sizeof(*resolutions) == sizeof(*resolution->dpis), "type mismatch");

	assert(nres > 0);

	if (resolution->ndpis == 0)
		return 0;

	memcpy(resolutions, resolution->dpis,
	       sizeof(unsigned int) * min(nres, resolution->ndpis));

	return resolution->ndpis;
}

LIBGHOSTCAT_EXPORT int
ghostcat_resolution_get_dpi_x(const struct ghostcat_resolution *resolution)
{
	return resolution->dpi_x;
}

LIBGHOSTCAT_EXPORT int
ghostcat_resolution_get_dpi_y(const struct ghostcat_resolution *resolution)
{
	return resolution->dpi_y;
}

LIBGHOSTCAT_EXPORT int
ghostcat_profile_get_report_rate(const struct ghostcat_profile *profile)
{
	return profile->hz;
}

LIBGHOSTCAT_EXPORT int
ghostcat_profile_get_angle_snapping(const struct ghostcat_profile *profile)
{
	return profile->angle_snapping;
}

LIBGHOSTCAT_EXPORT int
ghostcat_profile_get_debounce(const struct ghostcat_profile *profile)
{
	return profile->debounce;
}

LIBGHOSTCAT_EXPORT size_t
ghostcat_profile_get_debounce_list(const struct ghostcat_profile *profile,
				 unsigned int *debounces,
				 size_t ndebounces)
{
	_Static_assert(sizeof(*debounces) == sizeof(*profile->debounces), "type mismatch");

	assert(ndebounces > 0);

	if (profile->ndebounces == 0)
		return 0;

	memcpy(debounces, profile->debounces,
	       sizeof(unsigned int) * min(ndebounces, profile->ndebounces));

	return profile->ndebounces;
}

LIBGHOSTCAT_EXPORT size_t
ghostcat_profile_get_report_rate_list(const struct ghostcat_profile *profile,
				    unsigned int *rates,
				    size_t nrates)
{
	_Static_assert(sizeof(*rates) == sizeof(*profile->rates), "type mismatch");

	assert(nrates > 0);

	if (profile->nrates == 0)
		return 0;

	memcpy(rates, profile->rates,
	       sizeof(unsigned int) * min(nrates, profile->nrates));

	return profile->nrates;
}

LIBGHOSTCAT_EXPORT bool
ghostcat_resolution_is_active(const struct ghostcat_resolution *resolution)
{
	return !!resolution->is_active;
}

LIBGHOSTCAT_EXPORT enum ghostcat_error_code
ghostcat_resolution_set_active(struct ghostcat_resolution *resolution)
{
	struct ghostcat_profile *profile = resolution->profile;
	struct ghostcat_resolution *res;

	if (resolution->is_disabled) {
		log_error(profile->device->ratbag, "%s: setting the active resolution to a disabled resolution is not allowed\n", profile->device->name);
		return GHOSTCAT_ERROR_VALUE;
	}

	ghostcat_profile_for_each_resolution(profile, res)
		res->is_active = false;

	resolution->is_active = true;
	resolution->dirty = true;
	profile->dirty = true;
	return GHOSTCAT_SUCCESS;
}

LIBGHOSTCAT_EXPORT bool
ghostcat_resolution_is_default(const struct ghostcat_resolution *resolution)
{
	return !!resolution->is_default;
}

LIBGHOSTCAT_EXPORT enum ghostcat_error_code
ghostcat_resolution_set_default(struct ghostcat_resolution *resolution)
{
	struct ghostcat_profile *profile = resolution->profile;
	struct ghostcat_resolution *other;

	if (resolution->is_disabled) {
		log_error(profile->device->ratbag, "%s: setting the default resolution to a disabled resolution is not allowed\n", profile->device->name);
		return GHOSTCAT_ERROR_VALUE;
	}

	/* Unset the default on the other resolutions */
	ghostcat_profile_for_each_resolution(profile, other) {
		if (other == resolution || !other->is_default)
			continue;

		other->is_default = false;
		resolution->dirty = true;
		profile->dirty = true;
	}

	if (!resolution->is_default) {
		resolution->is_default = true;
		resolution->dirty = true;
		profile->dirty = true;
	}

	return GHOSTCAT_SUCCESS;
}

LIBGHOSTCAT_EXPORT bool
ghostcat_resolution_is_dpi_shift_target(const struct ghostcat_resolution *resolution)
{
	return !!resolution->is_dpi_shift_target;
}

LIBGHOSTCAT_EXPORT enum ghostcat_error_code
ghostcat_resolution_set_dpi_shift_target(struct ghostcat_resolution *resolution)
{
	struct ghostcat_profile *profile = resolution->profile;
	struct ghostcat_resolution *other;

	if (resolution->is_disabled) {
		log_error(profile->device->ratbag, "%s: setting the DPI shift target to a disabled resolution is not allowed\n", profile->device->name);
		return GHOSTCAT_ERROR_VALUE;
	}

	/* Unset the dpi_shift_target on the other resolutions */
	ghostcat_profile_for_each_resolution(profile, other) {
		if (other == resolution || !other->is_dpi_shift_target)
			continue;

		other->is_dpi_shift_target = false;
		other->dirty = true;
		profile->dirty = true;
	}

	if (!resolution->is_dpi_shift_target) {
		resolution->is_dpi_shift_target = true;
		resolution->dirty = true;
		profile->dirty = true;
	}

	return GHOSTCAT_SUCCESS;
}

LIBGHOSTCAT_EXPORT bool
ghostcat_resolution_is_disabled(const struct ghostcat_resolution *resolution)
{
	return !!resolution->is_disabled;
}

LIBGHOSTCAT_EXPORT enum ghostcat_error_code
ghostcat_resolution_set_disabled(struct ghostcat_resolution *resolution, bool disable)
{
	struct ghostcat_profile *profile = resolution->profile;

	if (!ghostcat_resolution_has_capability(resolution, GHOSTCAT_RESOLUTION_CAP_DISABLE))
		return GHOSTCAT_ERROR_CAPABILITY;

	if (disable) {
		if (resolution->is_active) {
			log_error(profile->device->ratbag, "%s: disabling the active resolution is not allowed\n", profile->device->name);
			return GHOSTCAT_ERROR_VALUE;
		}
		if (resolution->is_default) {
			log_error(profile->device->ratbag, "%s: disabling the default resolution is not allowed\n", profile->device->name);
			return GHOSTCAT_ERROR_VALUE;
		}
	}

	resolution->is_disabled = disable;
	resolution->dirty = true;
	profile->dirty = true;

	return GHOSTCAT_SUCCESS;
}

LIBGHOSTCAT_EXPORT struct ghostcat_button*
ghostcat_profile_get_button(struct ghostcat_profile *profile,
				   unsigned int index)
{
	struct ghostcat_device *device = profile->device;
	struct ghostcat_button *button;

	if (index >= ghostcat_device_get_num_buttons(device)) {
		log_bug_client(device->ratbag, "Requested invalid button %d\n", index);
		return NULL;
	}

	list_for_each(button, &profile->buttons, link) {
		if (button->index == index)
			return ghostcat_button_ref(button);
	}

	log_bug_libratbag(device->ratbag, "Button %d, profile %d not found\n",
			  index, profile->index);

	return NULL;
}

LIBGHOSTCAT_EXPORT enum ghostcat_button_action_type
ghostcat_button_get_action_type(const struct ghostcat_button *button)
{
	return button->action.type;
}

LIBGHOSTCAT_EXPORT bool
ghostcat_button_has_action_type(const struct ghostcat_button *button,
			      enum ghostcat_button_action_type action_type)
{
	switch (action_type) {
	case GHOSTCAT_BUTTON_ACTION_TYPE_NONE:
	case GHOSTCAT_BUTTON_ACTION_TYPE_BUTTON:
	case GHOSTCAT_BUTTON_ACTION_TYPE_SPECIAL:
	case GHOSTCAT_BUTTON_ACTION_TYPE_KEY:
	case GHOSTCAT_BUTTON_ACTION_TYPE_MACRO:
		break;
	default:
		return 0;
	}

	return !!(button->action_caps & (1 << action_type));
}

LIBGHOSTCAT_EXPORT unsigned int
ghostcat_button_get_button(const struct ghostcat_button *button)
{
	if (button->action.type != GHOSTCAT_BUTTON_ACTION_TYPE_BUTTON)
		return 0;

	return button->action.action.button;
}

void
ghostcat_button_set_action(struct ghostcat_button *button,
			 const struct ghostcat_button_action *action)
{
	struct ghostcat_macro *macro = button->action.macro;

	button->action = *action;
	if (action->type != GHOSTCAT_BUTTON_ACTION_TYPE_MACRO)
		button->action.macro = macro;
}

LIBGHOSTCAT_EXPORT enum ghostcat_error_code
ghostcat_button_set_button(struct ghostcat_button *button, unsigned int btn)
{
	struct ghostcat_button_action action = {0};

	if (!ghostcat_button_has_action_type(button,
					   GHOSTCAT_BUTTON_ACTION_TYPE_BUTTON))
		return GHOSTCAT_ERROR_CAPABILITY;

	action.type = GHOSTCAT_BUTTON_ACTION_TYPE_BUTTON;
	action.action.button = btn;

	ghostcat_button_set_action(button, &action);
	button->dirty = true;
	button->profile->dirty = true;

	return GHOSTCAT_SUCCESS;
}

LIBGHOSTCAT_EXPORT enum ghostcat_button_action_special
ghostcat_button_get_special(const struct ghostcat_button *button)
{
	if (button->action.type != GHOSTCAT_BUTTON_ACTION_TYPE_SPECIAL)
		return GHOSTCAT_BUTTON_ACTION_SPECIAL_INVALID;

	return button->action.action.special;
}

LIBGHOSTCAT_EXPORT enum ghostcat_error_code
ghostcat_button_set_special(struct ghostcat_button *button,
			  enum ghostcat_button_action_special act)
{
	struct ghostcat_button_action action = {0};

	/* FIXME: range checks */

	if (!ghostcat_button_has_action_type(button,
					   GHOSTCAT_BUTTON_ACTION_TYPE_SPECIAL))
		return GHOSTCAT_ERROR_CAPABILITY;

	action.type = GHOSTCAT_BUTTON_ACTION_TYPE_SPECIAL;
	action.action.special = act;

	ghostcat_button_set_action(button, &action);
	button->dirty = true;
	button->profile->dirty = true;

	return GHOSTCAT_SUCCESS;
}

LIBGHOSTCAT_EXPORT unsigned int
ghostcat_button_get_key(const struct ghostcat_button *button)
{
	if (button->action.type != GHOSTCAT_BUTTON_ACTION_TYPE_KEY)
		return 0;

	return button->action.action.key;
}

LIBGHOSTCAT_EXPORT enum ghostcat_error_code
ghostcat_button_set_key(struct ghostcat_button *button, unsigned int key)
{
	struct ghostcat_button_action action = {0};

	/* FIXME: range checks */

	if (!ghostcat_button_has_action_type(button,
					   GHOSTCAT_BUTTON_ACTION_TYPE_KEY))
		return GHOSTCAT_ERROR_CAPABILITY;

	action.type = GHOSTCAT_BUTTON_ACTION_TYPE_KEY;
	action.action.key = key;

	ghostcat_button_set_action(button, &action);
	button->dirty = true;
	button->profile->dirty = true;

	return GHOSTCAT_SUCCESS;
}

LIBGHOSTCAT_EXPORT enum ghostcat_error_code
ghostcat_button_disable(struct ghostcat_button *button)
{
	if (!ghostcat_button_has_action_type(button,
					   GHOSTCAT_BUTTON_ACTION_TYPE_NONE))
		return GHOSTCAT_ERROR_CAPABILITY;

	struct ghostcat_button_action action = {0};

	action.type = GHOSTCAT_BUTTON_ACTION_TYPE_NONE;

	ghostcat_button_set_action(button, &action);
	button->dirty = true;
	button->profile->dirty = true;

	return GHOSTCAT_SUCCESS;
}

LIBGHOSTCAT_EXPORT struct ghostcat_button *
ghostcat_button_ref(struct ghostcat_button *button)
{
	assert(button->refcount < INT_MAX);

	ghostcat_profile_ref(button->profile);
	button->refcount++;
	return button;
}

LIBGHOSTCAT_EXPORT struct ghostcat_led *
ghostcat_led_ref(struct ghostcat_led *led)
{
	assert(led->refcount < INT_MAX);

	ghostcat_profile_ref(led->profile);
	led->refcount++;
	return led;
}

static void
ghostcat_button_destroy(struct ghostcat_button *button)
{
	list_remove(&button->link);
	if (button->action.macro) {
		free(button->action.macro->name);
		free(button->action.macro->group);
		free(button->action.macro);
	}
	free(button);
}

static void
ghostcat_led_destroy(struct ghostcat_led *led)
{
	list_remove(&led->link);
	free(led);
}

static void
ghostcat_resolution_destroy(struct ghostcat_resolution *res)
{
	list_remove(&res->link);
	free(res);
}

LIBGHOSTCAT_EXPORT struct ghostcat_button *
ghostcat_button_unref(struct ghostcat_button *button)
{
	if (button == NULL)
		return NULL;

	assert(button->refcount > 0);
	button->refcount--;

	ghostcat_profile_unref(button->profile);

	return NULL;
}

LIBGHOSTCAT_EXPORT struct ghostcat_led *
ghostcat_led_unref(struct ghostcat_led *led)
{
	if (led == NULL)
		return NULL;

	assert(led->refcount > 0);
	led->refcount--;

	ghostcat_profile_unref(led->profile);

	return NULL;
}

LIBGHOSTCAT_EXPORT enum ghostcat_led_mode
ghostcat_led_get_mode(const struct ghostcat_led *led)
{
	return led->mode;
}

LIBGHOSTCAT_EXPORT bool
ghostcat_led_has_mode(const struct ghostcat_led *led,
		    enum ghostcat_led_mode mode)
{
	assert(mode <= GHOSTCAT_LED_BREATHING);

	if (mode == GHOSTCAT_LED_OFF)
		return 1;

	return !!(led->modes & (1 << mode));
}

LIBGHOSTCAT_EXPORT struct ghostcat_color
ghostcat_led_get_color(const struct ghostcat_led *led)
{
	return led->color;
}

LIBGHOSTCAT_EXPORT int
ghostcat_led_get_effect_duration(const struct ghostcat_led *led)
{
	return led->ms;
}

LIBGHOSTCAT_EXPORT unsigned int
ghostcat_led_get_brightness(const struct ghostcat_led *led)
{
	return led->brightness;
}

LIBGHOSTCAT_EXPORT enum ghostcat_error_code
ghostcat_led_set_mode(struct ghostcat_led *led, enum ghostcat_led_mode mode)
{
	led->mode = mode;
	led->dirty = true;
	led->profile->dirty = true;
	return GHOSTCAT_SUCCESS;
}

LIBGHOSTCAT_EXPORT enum ghostcat_error_code
ghostcat_led_set_color(struct ghostcat_led *led, struct ghostcat_color color)
{
	led->color = color;
	led->dirty = true;
	led->profile->dirty = true;
	return GHOSTCAT_SUCCESS;
}

LIBGHOSTCAT_EXPORT enum ghostcat_led_colordepth
ghostcat_led_get_colordepth(const struct ghostcat_led *led)
{
	return led->colordepth;
}

LIBGHOSTCAT_EXPORT enum ghostcat_error_code
ghostcat_led_set_effect_duration(struct ghostcat_led *led, unsigned int ms)
{
	led->ms = ms;
	led->dirty = true;
	led->profile->dirty = true;
	return GHOSTCAT_SUCCESS;
}

LIBGHOSTCAT_EXPORT enum ghostcat_error_code
ghostcat_led_set_brightness(struct ghostcat_led *led, unsigned int brightness)
{
	led->brightness = brightness;
	led->dirty = true;
	led->profile->dirty = true;
	return GHOSTCAT_SUCCESS;
}

LIBGHOSTCAT_EXPORT struct ghostcat_led *
ghostcat_profile_get_led(struct ghostcat_profile *profile,
		       unsigned int index)
{
	struct ghostcat_device *device = profile->device;
	struct ghostcat_led *led;

	if (index >= ghostcat_device_get_num_leds(device)) {
		log_bug_client(device->ratbag, "Requested invalid led %d\n", index);
		return NULL;
	}

	list_for_each(led, &profile->leds, link) {
		if (led->index == index)
			return ghostcat_led_ref(led);
	}

	log_bug_libratbag(device->ratbag, "Led %d, profile %d not found\n",
			  index, profile->index);

	return NULL;
}

LIBGHOSTCAT_EXPORT const char *
ghostcat_profile_get_name(const struct ghostcat_profile *profile)
{
	return profile->name;
}

LIBGHOSTCAT_EXPORT int
ghostcat_profile_set_name(struct ghostcat_profile *profile,
			const char *name)
{
	char *name_copy;

	if (!profile->name)
		return GHOSTCAT_ERROR_CAPABILITY;

	name_copy = strdup_safe(name);
	if (profile->name)
		free(profile->name);

	profile->name = name_copy;
	profile->dirty = true;

	return 0;
}

LIBGHOSTCAT_EXPORT void
ghostcat_set_user_data(struct ghostcat *ratbag, void *userdata)
{
	ratbag->userdata = userdata;
}

LIBGHOSTCAT_EXPORT void
ghostcat_device_set_user_data(struct ghostcat_device *ghostcat_device, void *userdata)
{
	ghostcat_device->userdata = userdata;
}

LIBGHOSTCAT_EXPORT void
ghostcat_profile_set_user_data(struct ghostcat_profile *ghostcat_profile, void *userdata)
{
	ghostcat_profile->userdata = userdata;
}

LIBGHOSTCAT_EXPORT void
ghostcat_button_set_user_data(struct ghostcat_button *ghostcat_button, void *userdata)
{
	ghostcat_button->userdata = userdata;
}

LIBGHOSTCAT_EXPORT void
ghostcat_resolution_set_user_data(struct ghostcat_resolution *ghostcat_resolution, void *userdata)
{
	ghostcat_resolution->userdata = userdata;
}

LIBGHOSTCAT_EXPORT void*
ghostcat_get_user_data(const struct ghostcat *ratbag)
{
	return ratbag->userdata;
}

LIBGHOSTCAT_EXPORT void*
ghostcat_device_get_user_data(const struct ghostcat_device *ghostcat_device)
{
	return ghostcat_device->userdata;
}

LIBGHOSTCAT_EXPORT int
ghostcat_device_refresh_active_resolution(struct ghostcat_device *device)
{
	if (!device->driver || !device->driver->refresh_active_resolution)
		return 0;

	return device->driver->refresh_active_resolution(device);
}

LIBGHOSTCAT_EXPORT const char*
ghostcat_device_get_firmware_version(const struct ghostcat_device *ghostcat_device)
{
	return ghostcat_device->firmware_version;
}

LIBGHOSTCAT_EXPORT void
ghostcat_device_set_firmware_version(struct ghostcat_device *device, const char* fw)
{
	free(device->firmware_version);
	device->firmware_version = strdup_safe(fw);
}

LIBGHOSTCAT_EXPORT void*
ghostcat_profile_get_user_data(const struct ghostcat_profile *ghostcat_profile)
{
	return ghostcat_profile->userdata;
}

LIBGHOSTCAT_EXPORT void*
ghostcat_button_get_user_data(const struct ghostcat_button *ghostcat_button)
{
	return ghostcat_button->userdata;
}

LIBGHOSTCAT_EXPORT void*
ghostcat_resolution_get_user_data(const struct ghostcat_resolution *ghostcat_resolution)
{
	return ghostcat_resolution->userdata;
}

LIBGHOSTCAT_EXPORT struct ghostcat_button_macro *
ghostcat_button_get_macro(struct ghostcat_button *button)
{
	struct ghostcat_button_macro *macro;

	if (button->action.type != GHOSTCAT_BUTTON_ACTION_TYPE_MACRO)
		return NULL;

	macro = ghostcat_button_macro_new(button->action.macro->name);
	memcpy(macro->macro.events,
	       button->action.macro->events,
	       sizeof(macro->macro.events));

	return macro;
}

void
ghostcat_button_copy_macro(struct ghostcat_button *button,
			 const struct ghostcat_button_macro *macro)
{
	if (!button->action.macro)
		button->action.macro = zalloc(sizeof(struct ghostcat_macro));
	else {
		free(button->action.macro->name);
		free(button->action.macro->group);
		memset(button->action.macro, 0, sizeof(struct ghostcat_macro));
	}

	button->action.type = GHOSTCAT_BUTTON_ACTION_TYPE_MACRO;
	memcpy(button->action.macro->events,
	       macro->macro.events,
	       sizeof(macro->macro.events));
	button->action.macro->name = strdup_safe(macro->macro.name);
	button->action.macro->group = strdup_safe(macro->macro.group);
}

LIBGHOSTCAT_EXPORT enum ghostcat_error_code
ghostcat_button_set_macro(struct ghostcat_button *button,
			const struct ghostcat_button_macro *macro)
{
	if (!ghostcat_button_has_action_type(button,
					   GHOSTCAT_BUTTON_ACTION_TYPE_MACRO))
		return GHOSTCAT_ERROR_CAPABILITY;

	ghostcat_button_copy_macro(button, macro);
	button->dirty = true;
	button->profile->dirty = true;

	return GHOSTCAT_SUCCESS;
}

LIBGHOSTCAT_EXPORT enum ghostcat_error_code
ghostcat_button_macro_set_event(struct ghostcat_button_macro *m,
			      unsigned int index,
			      enum ghostcat_macro_event_type type,
			      unsigned int data)
{
	struct ghostcat_macro *macro = &m->macro;

	if (index >= MAX_MACRO_EVENTS)
		return GHOSTCAT_ERROR_VALUE;

	switch (type) {
	case GHOSTCAT_MACRO_EVENT_KEY_PRESSED:
	case GHOSTCAT_MACRO_EVENT_KEY_RELEASED:
		macro->events[index].type = type;
		macro->events[index].event.key = data;
		break;
	case GHOSTCAT_MACRO_EVENT_WAIT:
		macro->events[index].type = type;
		macro->events[index].event.timeout = data;
		break;
	case GHOSTCAT_MACRO_EVENT_NONE:
		macro->events[index].type = type;
		break;
	default:
		return GHOSTCAT_ERROR_VALUE;
	}

	return 0;
}

LIBGHOSTCAT_EXPORT enum ghostcat_macro_event_type
ghostcat_button_macro_get_event_type(const struct ghostcat_button_macro *macro,
				   unsigned int index)
{
	if (index >= MAX_MACRO_EVENTS)
		return GHOSTCAT_MACRO_EVENT_INVALID;

	return macro->macro.events[index].type;
}

LIBGHOSTCAT_EXPORT int
ghostcat_button_macro_get_event_key(const struct ghostcat_button_macro *m,
				  unsigned int index)
{
	const struct ghostcat_macro *macro = &m->macro;

	if (index >= MAX_MACRO_EVENTS)
		return 0;

	if (macro->events[index].type != GHOSTCAT_MACRO_EVENT_KEY_PRESSED &&
	    macro->events[index].type != GHOSTCAT_MACRO_EVENT_KEY_RELEASED)
		return -EINVAL;

	return macro->events[index].event.key;
}

LIBGHOSTCAT_EXPORT int
ghostcat_button_macro_get_event_timeout(const struct ghostcat_button_macro *m,
				      unsigned int index)
{
	const struct ghostcat_macro *macro = &m->macro;

	if (index >= MAX_MACRO_EVENTS)
		return 0;

	if (macro->events[index].type != GHOSTCAT_MACRO_EVENT_WAIT)
		return 0;

	return macro->events[index].event.timeout;
}

LIBGHOSTCAT_EXPORT unsigned int
ghostcat_button_macro_get_num_events(const struct ghostcat_button_macro *macro)
{
	return MAX_MACRO_EVENTS;
}

LIBGHOSTCAT_EXPORT const char *
ghostcat_button_macro_get_name(const struct ghostcat_button_macro *macro)
{
	return macro->macro.name;
}

static void
ghostcat_button_macro_destroy(struct ghostcat_button_macro *macro)
{
	assert(macro->refcount == 0);
	free(macro->macro.name);
	free(macro->macro.group);
	free(macro);
}

LIBGHOSTCAT_EXPORT struct ghostcat_button_macro *
ghostcat_button_macro_ref(struct ghostcat_button_macro *macro)
{
	assert(macro->refcount < INT_MAX);

	macro->refcount++;
	return macro;
}

LIBGHOSTCAT_EXPORT struct ghostcat_button_macro *
ghostcat_button_macro_unref(struct ghostcat_button_macro *macro)
{
	if (macro == NULL)
		return NULL;

	assert(macro->refcount > 0);
	macro->refcount--;
	if (macro->refcount == 0)
		ghostcat_button_macro_destroy(macro);

	return NULL;
}

LIBGHOSTCAT_EXPORT struct ghostcat_button_macro *
ghostcat_button_macro_new(const char *name)
{
	struct ghostcat_button_macro *macro;

	macro = zalloc(sizeof *macro);
	macro->refcount = 1;
	macro->macro.name = strdup_safe(name);

	return macro;
}

struct ghostcat_modifier_mapping {
	unsigned int modifier_mask;
	unsigned int key;
};

struct ghostcat_modifier_mapping ghostcat_modifier_mapping[] = {
	{ MODIFIER_LEFTCTRL, KEY_LEFTCTRL },
	{ MODIFIER_LEFTSHIFT, KEY_LEFTSHIFT },
	{ MODIFIER_LEFTALT, KEY_LEFTALT },
	{ MODIFIER_LEFTMETA, KEY_LEFTMETA },
	{ MODIFIER_RIGHTCTRL, KEY_RIGHTCTRL },
	{ MODIFIER_RIGHTSHIFT, KEY_RIGHTSHIFT },
	{ MODIFIER_RIGHTALT, KEY_RIGHTALT },
	{ MODIFIER_RIGHTMETA, KEY_RIGHTMETA },
};

int
ghostcat_button_macro_new_from_keycode(struct ghostcat_button *button,
				     unsigned int key,
				     unsigned int modifiers)
{
	struct ghostcat_button_macro *macro;
	struct ghostcat_modifier_mapping *mapping;
	int i;

	macro = ghostcat_button_macro_new("key");
	i = 0;

	ARRAY_FOR_EACH(ghostcat_modifier_mapping, mapping) {
		if (modifiers & mapping->modifier_mask)
			ghostcat_button_macro_set_event(macro,
						      i++,
						      GHOSTCAT_MACRO_EVENT_KEY_PRESSED,
						      mapping->key);
	}

	ghostcat_button_macro_set_event(macro,
				      i++,
				      GHOSTCAT_MACRO_EVENT_KEY_PRESSED,
				      key);
	ghostcat_button_macro_set_event(macro,
				      i++,
				      GHOSTCAT_MACRO_EVENT_KEY_RELEASED,
				      key);

	ARRAY_FOR_EACH(ghostcat_modifier_mapping, mapping) {
		if (modifiers & mapping->modifier_mask)
			ghostcat_button_macro_set_event(macro,
						      i++,
						      GHOSTCAT_MACRO_EVENT_KEY_RELEASED,
						      mapping->key);
	}

	ghostcat_button_copy_macro(button, macro);
	ghostcat_button_macro_unref(macro);

	return 0;
}

int
ghostcat_action_macro_num_keys(const struct ghostcat_button_action *action)
{
	const struct ghostcat_macro *macro = action->macro;
	int count = 0;
	for (int i = 0; i < MAX_MACRO_EVENTS; i++) {
		struct ghostcat_macro_event event = macro->events[i];
		if (event.type == GHOSTCAT_MACRO_EVENT_NONE ||
		    event.type == GHOSTCAT_MACRO_EVENT_INVALID) {
			break;
		}
		if (ghostcat_key_is_modifier(event.event.key)) {
			continue;
		}
		if (event.type == GHOSTCAT_MACRO_EVENT_KEY_PRESSED) {
			count += 1;
		}
	}
	return count;
}

static bool
ghostcat_action_is_single_modifier_key(const struct ghostcat_button_action *action)
{
	const struct ghostcat_macro *macro = action->macro;
	int modifier_key_count = 0;
	int action_key_count = 0;
	for (int i = 0; i < MAX_MACRO_EVENTS; i++) {
		struct ghostcat_macro_event event = macro->events[i];
		if (event.type == GHOSTCAT_MACRO_EVENT_NONE ||
		    event.type == GHOSTCAT_MACRO_EVENT_INVALID) {
			break;
		}
		if (ghostcat_key_is_modifier(event.event.key) && event.type == GHOSTCAT_MACRO_EVENT_KEY_PRESSED) {
			modifier_key_count += 1;
		}
		if (!ghostcat_key_is_modifier(event.event.key) && event.type == GHOSTCAT_MACRO_EVENT_KEY_PRESSED) {
			action_key_count += 1;
		}
	}
	return modifier_key_count == 1 && action_key_count == 0;
}

int
ghostcat_action_keycode_from_macro(const struct ghostcat_button_action *action,
				 unsigned int *key_out,
				 unsigned int *modifiers_out)
{
	const struct ghostcat_macro *macro = action->macro;
	unsigned int key = KEY_RESERVED;
	unsigned int modifiers = 0;
	unsigned int i;

	if (!macro || action->type != GHOSTCAT_BUTTON_ACTION_TYPE_MACRO)
		return -EINVAL;

	if (macro->events[0].type == GHOSTCAT_MACRO_EVENT_NONE)
		return -EINVAL;

	if (ghostcat_action_is_single_modifier_key(action)) {
		key = macro->events[0].event.key;
		*key_out = key;
		*modifiers_out = 0;
		return 1;
	}

	if (ghostcat_action_macro_num_keys(action) != 1)
		return -EINVAL;

	for (i = 0; i < MAX_MACRO_EVENTS; i++) {
		struct ghostcat_macro_event event;

		event = macro->events[i];
		switch (event.type) {
		case GHOSTCAT_MACRO_EVENT_INVALID:
			return -EINVAL;
		case GHOSTCAT_MACRO_EVENT_NONE:
			return 0;
		case GHOSTCAT_MACRO_EVENT_KEY_PRESSED:
			switch(event.event.key) {
			case KEY_LEFTCTRL: modifiers |= MODIFIER_LEFTCTRL; break;
			case KEY_LEFTSHIFT: modifiers |= MODIFIER_LEFTSHIFT; break;
			case KEY_LEFTALT: modifiers |= MODIFIER_LEFTALT; break;
			case KEY_LEFTMETA: modifiers |= MODIFIER_LEFTMETA; break;
			case KEY_RIGHTCTRL: modifiers |= MODIFIER_RIGHTCTRL; break;
			case KEY_RIGHTSHIFT: modifiers |= MODIFIER_RIGHTSHIFT; break;
			case KEY_RIGHTALT: modifiers |= MODIFIER_RIGHTALT; break;
			case KEY_RIGHTMETA: modifiers |= MODIFIER_RIGHTMETA; break;
			default:
				if (key != KEY_RESERVED)
					return -EINVAL;

				key = event.event.key;
			}
			break;
		case GHOSTCAT_MACRO_EVENT_KEY_RELEASED:
			switch(event.event.key) {
			case KEY_LEFTCTRL: modifiers &= ~MODIFIER_LEFTCTRL; break;
			case KEY_LEFTSHIFT: modifiers &= ~MODIFIER_LEFTSHIFT; break;
			case KEY_LEFTALT: modifiers &= ~MODIFIER_LEFTALT; break;
			case KEY_LEFTMETA: modifiers &= ~MODIFIER_LEFTMETA; break;
			case KEY_RIGHTCTRL: modifiers &= ~MODIFIER_RIGHTCTRL; break;
			case KEY_RIGHTSHIFT: modifiers &= ~MODIFIER_RIGHTSHIFT; break;
			case KEY_RIGHTALT: modifiers &= ~MODIFIER_RIGHTALT; break;
			case KEY_RIGHTMETA: modifiers &= ~MODIFIER_RIGHTMETA; break;
			default:
				if (event.event.key != key)
					return -EINVAL;

				*key_out = key;
				*modifiers_out = modifiers;
				return 1;
			}
		case GHOSTCAT_MACRO_EVENT_WAIT:
			break;
		default:
			return -EINVAL;
		}
	}

	return -EINVAL;
}
