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

#include <stdlib.h>
#include <stdint.h>
#include <stdarg.h>
#include <stdbool.h>
#include <libudev.h>

#include "libghostcat-enums.h"

#define LIBGHOSTCAT_ATTRIBUTE_PRINTF(_format, _args) \
	__attribute__ ((format (printf, _format, _args)))
#define LIBGHOSTCAT_ATTRIBUTE_DEPRECATED __attribute__ ((deprecated))

/**
 * @defgroup base Initialization and manipulation of ratbag contexts
 * @defgroup device Querying and manipulating devices
 *
 * Device configuration is managed by "profiles" (see @ref profile).
 * In the simplest case, a device has a single profile that can be fetched,
 * queried and manipulated and then re-applied to the device. Other devices
 * may have multiple profiles, each of which can be queried and managed
 * independently.
 *
 * @defgroup profile Device profiles
 *
 * A profile on a device consists of a set of button functions and, where
 * applicable, a range of resolution settings, one of which is currently
 * active.
 *
 * @defgroup button Button configuration
 *
 * @defgroup led LED configuration
 *
 * @defgroup resolution Resolution and frequency mappings
 *
 * A device's sensor resolution and report rate can be configured per
 * profile, with each profile reporting a number of resolution modes (see
 * @ref ghostcat_resolution). The number depends on the hardware, but at least
 * one is provided by libratbag.
 *
 * Each resolution mode is a tuple of a resolution and report rate and
 * represents the modes that the mouse can switch through, usually with the
 * use of a button on the mouse to cycle through the preconfigured
 * resolutions.
 *
 * The resolutions have a default resolution and a currently active
 * resolution. The currently active one is the one used by the device now
 * and only applies if the profile is currently active too. The default
 * resolution is the one the device will chose when the profile is selected
 * next.
 */

/**
 * @ingroup base
 * @struct ghostcat
 *
 * A handle for accessing ratbag contexts. This struct is refcounted, use
 * ghostcat_ref() and ghostcat_unref().
 */
struct ghostcat;

/**
 * @ingroup device
 * @struct ghostcat_device
 *
 * A ratbag context represents one single device. This struct is
 * refcounted, use ghostcat_device_ref() and ghostcat_device_unref().
 */
struct ghostcat_device;

/**
 * @ingroup profile
 * @struct ghostcat_profile
 *
 * A handle to a profile context on devices.
 * This struct is refcounted, use ghostcat_profile_ref() and
 * ghostcat_profile_unref().
 */
struct ghostcat_profile;

/**
 * @ingroup button
 * @struct ghostcat_button
 *
 * Represents a button on the device.
 *
 * This struct is refcounted, use ghostcat_button_ref() and
 * ghostcat_button_unref().
 */
struct ghostcat_button;

/**
 * @ingroup resolution
 * @struct ghostcat_resolution
 *
 * Represents a resolution setting on the device. Most devices have multiple
 * resolutions per profile, one of which is active at a time.
 *
 * This struct is refcounted, use ghostcat_resolution_ref() and
 * ghostcat_resolution_unref().
 */
struct ghostcat_resolution;

/**
 * @ingroup led
 * @struct ghostcat_color
 *
 * Represents LED color in RGB format.
 * each color component is integer 0 - 255
 */

struct ghostcat_color {
	unsigned int red;
	unsigned int green;
	unsigned int blue;
};

/**
 * @ingroup led
 * @struct ghostcat_led
 *
 * Represents a led on the device.
 */
struct ghostcat_led;

/**
 * @ingroup button
 * @struct ghostcat_macro
 *
 * Represents a macro that can be assigned to a button.
 *
 * This struct is refcounted, use ghostcat_button_macro_ref() and
 * ghostcat_button_macro_unref().
 */
struct ghostcat_button_macro;

/**
 * @ingroup base
 *
 * Log priority for internal logging messages.
 */
enum ghostcat_log_priority {
	/**
	 * Raw protocol messages. Using this log level results in *a lot* of
	 * output.
	 */
	GHOSTCAT_LOG_PRIORITY_RAW = 10,
	GHOSTCAT_LOG_PRIORITY_DEBUG = 20,
	GHOSTCAT_LOG_PRIORITY_INFO = 30,
	GHOSTCAT_LOG_PRIORITY_ERROR = 40,
};

/**
 * @ingroup base
 *
 * Log handler type for custom logging.
 *
 * @param ratbag The ratbag context
 * @param priority The priority of the current message
 * @param format Message format in printf-style
 * @param args Message arguments
 *
 * @see ghostcat_log_set_priority
 * @see ghostcat_log_get_priority
 * @see ghostcat_log_set_handler
 */
typedef void (*ghostcat_log_handler)(struct ghostcat *ratbag,
				      enum ghostcat_log_priority priority,
				      const char *format, va_list args)
	   LIBGHOSTCAT_ATTRIBUTE_PRINTF(3, 0);

/**
 * @ingroup base
 *
 * Set the log priority for the ratbag context. Messages with priorities
 * equal to or higher than the argument will be printed to the context's
 * log handler.
 *
 * The default log priority is @ref GHOSTCAT_LOG_PRIORITY_ERROR.
 *
 * @param ratbag A previously initialized ratbag context
 * @param priority The minimum priority of log messages to print.
 *
 * @see ghostcat_log_set_handler
 * @see ghostcat_log_get_priority
 */
void
ghostcat_log_set_priority(struct ghostcat *ratbag,
			enum ghostcat_log_priority priority);

/**
 * @ingroup base
 *
 * Get the context's log priority. Messages with priorities equal to or
 * higher than the argument will be printed to the current log handler.
 *
 * The default log priority is @ref GHOSTCAT_LOG_PRIORITY_ERROR.
 *
 * @param ratbag A previously initialized ratbag context
 * @return The minimum priority of log messages to print.
 *
 * @see ghostcat_log_set_handler
 * @see ghostcat_log_set_priority
 */
enum ghostcat_log_priority
ghostcat_log_get_priority(const struct ghostcat *ratbag);

/**
 * @ingroup base
 *
 * Set the context's log handler. Messages with priorities equal to or
 * higher than the context's log priority will be passed to the given
 * log handler.
 *
 * The default log handler prints to stderr.
 *
 * @param ratbag A previously initialized ratbag context
 * @param log_handler The log handler for library messages.
 *
 * @see ghostcat_log_set_priority
 * @see ghostcat_log_get_priority
 */
void
ghostcat_log_set_handler(struct ghostcat *ratbag,
		       ghostcat_log_handler log_handler);


/**
 * @ingroup base
 * @struct ghostcat_interface
 *
 * libratbag does not open file descriptors to devices directly, instead
 * open_restricted() and close_restricted() are called for each path that
 * must be opened.
 *
 * @see ghostcat_create_context
 */
struct ghostcat_interface {
	/**
	 * Open the device at the given path with the flags provided and
	 * return the fd.
	 *
	 * @param path The device path to open
	 * @param flags Flags as defined by open(2)
	 * @param user_data The user_data provided in
	 * ghostcat_create_context()
	 *
	 * @return The file descriptor, or a negative errno on failure.
	 */
	int (*open_restricted)(const char *path, int flags, void *user_data);
	/**
	 * Close the file descriptor.
	 *
	 * @param fd The file descriptor to close
	 * @param user_data The user_data provided in
	 * ghostcat_create_context()
	 */
	void (*close_restricted)(int fd, void *user_data);
};

/**
 * @ingroup base
 *
 * Create a new ratbag context.
 *
 * The context is refcounted with an initial value of at least 1.
 * Use ghostcat_unref() to release the context.
 *
 * @return An initialized ratbag context or NULL on error
 */
struct ghostcat *
ghostcat_create_context(const struct ghostcat_interface *interface,
			 void *userdata);

/**
 * @ingroup base
 *
 * Set caller-specific data associated with this context. libratbag does
 * not manage, look at, or modify this data. The caller must ensure the
 * data is valid.
 *
 * Setting userdata overrides the one provided to ghostcat_create_context().
 *
 * @param ratbag A previously initialized ratbag context
 * @param userdata Caller-specific data passed to the various callback
 * interfaces.
 */
void
ghostcat_set_user_data(struct ghostcat *ratbag, void *userdata);

/**
 * @ingroup base
 *
 * Get the caller-specific data associated with this context, if any.
 *
 * @param ratbag A previously initialized ratbag context
 * @return The caller-specific data previously assigned in
 * ghostcat_create_context (or ghostcat_set_user_data()).
 */
void*
ghostcat_get_user_data(const struct ghostcat *ratbag);

/**
 * @ingroup base
 *
 * Add a reference to the context. A context is destroyed whenever the
 * reference count reaches 0. See @ref ghostcat_unref.
 *
 * @param ratbag A previously initialized valid ratbag context
 * @return The passed ratbag context
 */
struct ghostcat *
ghostcat_ref(struct ghostcat *ratbag);

/**
 * @ingroup base
 *
 * Dereference the ratbag context. After this, the context may have been
 * destroyed, if the last reference was dereferenced. If so, the context is
 * invalid and may not be interacted with.
 *
 * @param ratbag A previously initialized ratbag context
 * @return Always NULL
 */
struct ghostcat *
ghostcat_unref(struct ghostcat *ratbag);

/**
 * @ingroup base
 *
 * Create a new ratbag context from the given udev device.
 *
 * The device is refcounted with an initial value of at least 1.
 * Use ghostcat_device_unref() to release the device.
 *
 * @param ratbag A previously initialized ratbag context
 * @param udev_device The udev device that points at the device
 * @param device Set to a new device based on the udev device.
 *
 * @return 0 on success or the error.
 * @retval GHOSTCAT_ERROR_DEVICE The given device does not exist or is not
 * supported by libratbag.
 */
enum ghostcat_error_code
ghostcat_device_new_from_udev_device(struct ghostcat *ratbag,
				   struct udev_device *udev_device,
				   struct ghostcat_device **device);

/**
 * @ingroup device
 *
 * Add a reference to the device. A device is destroyed whenever the
 * reference count reaches 0. See @ref ghostcat_device_unref.
 *
 * @param device A previously initialized valid ratbag device
 * @return The passed ratbag device
 */
struct ghostcat_device *
ghostcat_device_ref(struct ghostcat_device *device);

/**
 * @ingroup device
 *
 * Dereference the ratbag device. When the internal refcount reaches
 * zero, all resources associated with this object are released. The object
 * must be considered invalid once unref is called.
 *
 * @param device A previously initialized ratbag device
 * @return Always NULL
 */
struct ghostcat_device *
ghostcat_device_unref(struct ghostcat_device *device);

/**
 * @ingroup device
 *
 * Set caller-specific data associated with this device. libratbag does
 * not manage, look at, or modify this data. The caller must ensure the
 * data is valid.
 *
 * @param device A previously initialized device
 * @param userdata Caller-specific data passed to the various callback
 * interfaces.
 */
void
ghostcat_device_set_user_data(struct ghostcat_device *device, void *userdata);

/**
 * @ingroup device
 *
 * Get the caller-specific data associated with this device, if any.
 *
 * @param device A previously initialized ratbag device
 * @return The caller-specific data previously assigned in
 * ghostcat_device_set_user_data().
 */
void*
ghostcat_device_get_user_data(const struct ghostcat_device *device);

/**
 * @ingroup device
 *
 * Refresh the active resolution state by re-reading from hardware.
 * This detects changes from physical DPI button presses on the device.
 *
 * @param device A previously initialized ratbag device
 *
 * @return 1 if state changed, 0 if unchanged, or negative error code
 */
int
ghostcat_device_refresh_active_resolution(struct ghostcat_device *device);

/**
 * @ingroup device
 *
 * Set the firmware version of the device.
 *
 * @param device A previously initialized ratbag device
 * @param fw The firmware version of the device.
 */
void
ghostcat_device_set_firmware_version(struct ghostcat_device *device, const char* fw);

/**
 * @ingroup device
 *
 * @param device A previously initialized ratbag device
 * @return The firmware version of the device.
 */
const char*
ghostcat_device_get_firmware_version(const struct ghostcat_device *device);

/**
 * @ingroup device
 *
 * @param device A previously initialized ratbag device
 * @return The name of the device associated with the given ratbag.
 */
const char *
ghostcat_device_get_name(const struct ghostcat_device* device);

/**
 * @ingroup device
 *
 * @param device A previously initialized ratbag device
 * @return the type of the device: unspecified, mouse, keyboard or other
 */
enum ghostcat_device_type
ghostcat_device_get_device_type(const struct ghostcat_device* device);

/**
 * @ingroup device
 *
 * Returns the bustype of the device, "usb", "bluetooth" or NULL for
 * unknown.
 */
const char *
ghostcat_device_get_bustype(const struct ghostcat_device *device);

/**
 * @ingroup device
 *
 * Returns the vendor ID of the device.
 */
uint32_t
ghostcat_device_get_vendor_id(const struct ghostcat_device *device);


/**
 * @ingroup device
 *
 * Returns the product ID of the device.
 */
uint32_t
ghostcat_device_get_product_id(const struct ghostcat_device *device);

/**
 * @ingroup device
 *
 * Returns the product version of the device. This is a made-up number,
 * internal to libratbag. It's purpose is to provide an extra bit of
 * differentiation where devices use the same VID/PID for different models.
 *
 * Devices with identical VID/PID but different versions must be considered
 * different devices.
 *
 * Usually the version will be 0.
 */
uint32_t
ghostcat_device_get_product_version(const struct ghostcat_device *device);

/**
 * @ingroup device
 *
 * Write any changes to the device. Depending on the device, this may take
 * a couple of seconds.
 *
 * @param device A previously initialized ratbag device
 * @return 0 on success or an error code otherwise
 */
enum ghostcat_error_code
ghostcat_device_commit(struct ghostcat_device *device);

/**
 * @ingroup device
 *
 * Return the number of profiles supported by this device.
 *
 * Note that the number of profiles available may be different to the number
 * of profiles currently active. This function returns the maximum number of
 * profiles available and is static for the lifetime of the device.
 *
 * A device that does not support profiles in hardware provides a single
 * profile that reflects the current settings of the device.
 *
 * @param device A previously initialized ratbag device
 * @return The number of profiles available on this device.
 */
unsigned int
ghostcat_device_get_num_profiles(const struct ghostcat_device *device);

/**
 * @ingroup device
 *
 * Return the number of buttons available on this device.
 *
 * @param device A previously initialized ratbag device
 * @return The number of buttons available on this device.
 */
unsigned int
ghostcat_device_get_num_buttons(const struct ghostcat_device *device);

/**
 * @ingroup device
 *
 * Return the number of LEDs available on this device.
 *
 * @param device A previously initialized ratbag device
 * @return The number of LEDs available on this device.
 */
unsigned int
ghostcat_device_get_num_leds(const struct ghostcat_device *device);

/**
 * @ingroup profile
 *
 * Add a reference to the profile. A profile is destroyed whenever the
 * reference count reaches 0. See @ref ghostcat_profile_unref.
 *
 * @param profile A previously initialized valid ratbag profile
 * @return The passed ratbag profile
 */
struct ghostcat_profile *
ghostcat_profile_ref(struct ghostcat_profile *profile);

/**
 * @ingroup profile
 *
 * Dereference the ratbag profile. When the internal refcount reaches
 * zero, all resources associated with this object are released. The object
 * must be considered invalid once unref is called.
 *
 * @param profile A previously initialized ratbag profile
 * @return Always NULL
 */
struct ghostcat_profile *
ghostcat_profile_unref(struct ghostcat_profile *profile);

/**
 * @ingroup profile
 *
 * Check if a profile has a specific capability.
 *
 * @return non-zero if the capability is available, zero otherwise.
 */
bool
ghostcat_profile_has_capability(const struct ghostcat_profile *profile,
			      enum ghostcat_profile_capability cap);

/**
 * @ingroup profile
 *
 * Return the ratbag profile name.
 *
 * @param profile A previously initialized ratbag profile
 * @return the profile name
 */
const char *
ghostcat_profile_get_name(const struct ghostcat_profile *profile);

/**
 * @ingroup profile
 *
 * Set the name of a ratbag profile.
 *
 * @param profile A previously initialized ratbag profile
 * @param name the profile name
 *
 * @return 0 on success or an error code otherwise
 */
int
ghostcat_profile_set_name(struct ghostcat_profile *profile,
			const char *name);

/**
 * @ingroup profile
 *
 * Enable/disable the ratbag profile. For this to work, the profile must support
 * @ref GHOSTCAT_PROFILE_CAP_DISABLE.
 *
 * @param profile A previously initialized ratbag profile
 * @param enabled Whether to enable or disable the profile
 *
 * @return 0 on success or an error code otherwise
 */
enum ghostcat_error_code
ghostcat_profile_set_enabled(struct ghostcat_profile *profile, bool enabled);

/**
 * @ingroup profile
 *
 * Check whether the ratbag profile is enabled or not. For devices that don't
 * support @ref GHOSTCAT_PROFILE_CAP_DISABLE the profile will always be
 * set to enabled.
 *
 * @param profile A previously initialized ratbag profile
 *
 * @return Whether the profile is enabled or not.
 */
bool
ghostcat_profile_is_enabled(const struct ghostcat_profile *profile);

/**
 * @ingroup profile
 *
 * Set caller-specific data associated with this profile. libratbag does
 * not manage, look at, or modify this data. The caller must ensure the
 * data is valid.
 *
 * @param profile A previously initialized profile
 * @param userdata Caller-specific data passed to the various callback
 * interfaces.
 */
void
ghostcat_profile_set_user_data(struct ghostcat_profile *profile, void *userdata);

/**
 * @ingroup profile
 *
 * Get the caller-specific data associated with this profile, if any.
 *
 * @param profile A previously initialized ratbag profile
 * @return The caller-specific data previously assigned in
 * ghostcat_profile_set_user_data().
 */
void*
ghostcat_profile_get_user_data(const struct ghostcat_profile *profile);

/**
 * @ingroup profile
 *
 * This function creates if necessary and returns a profile for the given
 * index. The index must be less than the number returned by
 * ghostcat_get_num_profiles().
 *
 * The profile is refcounted with an initial value of at least 1.
 * Use ghostcat_profile_unref() to release the profile.
 *
 * @param device A previously initialized ratbag device
 * @param index The index of the profile
 *
 * @return The profile at the given index, or NULL if the profile does not
 * exist.
 *
 * @see ghostcat_get_num_profiles
 */
struct ghostcat_profile *
ghostcat_device_get_profile(struct ghostcat_device *device, unsigned int index);

/**
 * @ingroup profile
 *
 * Check if the given profile is the currently active one. Note that some
 * devices allow switching profiles with hardware buttons thus making the
 * use of this function racy.
 *
 * @param profile A previously initialized ratbag profile
 *
 * @return non-zero if the profile is currently active, zero otherwise
 */
bool
ghostcat_profile_is_active(const struct ghostcat_profile *profile);

/**
 * @ingroup profile
 *
 * Check if the given profile is dirty, that is, the device needs committing
 * after having configuration changes.
 *
 * @param profile A previously initialized ratbag profile
 *
 * @return non-zero if the profile is dirty, zero otherwise
 */
bool
ghostcat_profile_is_dirty(const struct ghostcat_profile *profile);

/**
 * @ingroup profile
 *
 * Make the given profile the currently active profile. Note that you have to
 * commit the device before this gets applied.
 *
 * @param profile The profile to make the active profile.
 *
 * @return 0 on success or an error code otherwise
 */
enum ghostcat_error_code
ghostcat_profile_set_active(struct ghostcat_profile *profile);

/**
 * @ingroup profile
 *
 * Set the report rate in Hz for the profile.
 *
 * A value of 0 hz disables the mode.
 *
 * If the profile mode is the currently active profile,
 * the change takes effect immediately.
 *
 * @param profile A previously initialized ratbag profile
 * @param hz Set to the report rate in Hz, may be 0
 *
 * @return zero on success or an error code on failure
 */
enum ghostcat_error_code
ghostcat_profile_set_report_rate(struct ghostcat_profile *profile,
			       unsigned int hz);

/**
 * @ingroup profile
 *
 * Get the report rate in Hz for the profile.
 *
 * @param profile A previously initialized ratbag profile
 *
 * @return The report rate for this profile in Hz
 */
int
ghostcat_profile_get_report_rate(const struct ghostcat_profile *profile);

/**
 * @ingroup profile
 *
 * Get the number of report rates in Hz available for this profile.
 * The list of report rates is sorted in ascending order but may be filtered
 * by libratbag and does not necessarily reflect all report rates supported by
 * the physical device.
 *
 * This function writes at most nrates values but returns the number of
 * report rates available on this resolution. In other words, if it returns a
 * number larger than nrates, call it again with an array the size of the
 * return value.
 *
 * @param[out] rates Set to the supported report rates in ascending order
 * @param[in] nrates The number of elements in resolutions
 *
 * @return The number of valid items in rates. If the returned value
 * is larger than nrates, the list was truncated.
 */
size_t
ghostcat_profile_get_report_rate_list(const struct ghostcat_profile *profile,
				    unsigned int *rates,
				    size_t nrates);

/**
 * @ingroup profile
 *
 * Set the angle snapping option for the profile.
 *
 * A value of 0 disables the mode.
 *
 * If the profile mode is the currently active profile,
 * the change takes effect immediately.
 *
 * @param profile A previously initialized ratbag profile
 * @param value Angle snapping value
 *
 * @return zero on success or an error code on failure
 */
enum ghostcat_error_code
ghostcat_profile_set_angle_snapping(struct ghostcat_profile *profile,
				  int value);

/**
 * @ingroup profile
 *
 * Get the angle snapping option for the profile.
 *
 * @param profile A previously initialized ratbag profile
 *
 * @return Angle snapping value
 */
int
ghostcat_profile_get_angle_snapping(const struct ghostcat_profile *profile);

/**
 * @ingroup profile
 *
 * Set the debounce time in ms for the profile.
 *
 * If the profile mode is the currently active profile,
 * the change takes effect immediately.
 *
 * @param profile A previously initialized ratbag profile
 * @param value Debounce time in ms
 *
 * @return zero on success or an error code on failure
 */
enum ghostcat_error_code
ghostcat_profile_set_debounce(struct ghostcat_profile *profile,
			    int value);

/**
 * @ingroup profile
 *
 * Get the debounce time in ms for the profile.
 *
 * @param profile A previously initialized ratbag profile
 *
 * @return Debounce time for this profile in ms
 */
int
ghostcat_profile_get_debounce(const struct ghostcat_profile *profile);

/**
 * @ingroup profile
 *
 * Get the number of debounce times in ms available for this profile.
 * The list of debounce times is sorted in ascending order but may be filtered
 * by libratbag and does not necessarily reflect all debounce times supported by
 * the physical device.
 *
 * This function writes at most ndebounces values but returns the number of
 * debounce times available on this resolution. In other words, if it returns a
 * number larger than ndebounces, call it again with an array the size of the
 * return value.
 *
 * @param[out] debounces Set to the supported debounce times in ascending order
 * @param[in] ndebounces The number of elements
 *
 * @return The number of valid items in values. If the returned value
 * is larger than ndebounces, the list was truncated.
 */
size_t
ghostcat_profile_get_debounce_list(const struct ghostcat_profile *profile,
				 unsigned int *debounces,
				 size_t ndebounces);

/**
 * @ingroup profile
 *
 * Get the number of @ref ghostcat_resolution available in this profile. A
 * resolution mode is a tuple of (resolution, report rate), each mode can be
 * fetched with ghostcat_profile_get_resolution().
 *
 * The returned value is the maximum number of modes available and thus
 * identical for all profiles. However, some of the modes may not be
 * configured.
 *
 * @param profile A previously initialized ratbag profile
 *
 * @return The number of resolutions available.
 */
unsigned int
ghostcat_profile_get_num_resolutions(const struct ghostcat_profile *profile);

/**
 * @ingroup profile
 *
 * Return the resolution in DPI and the report rate in Hz for the resolution
 * mode identified by the given index. The index must be between 0 and
 * ghostcat_profile_get_num_resolution_modes().
 *
 * See ghostcat_profile_get_num_resolution_modes() for a description of
 * resolution_modes.
 *
 * Profiles available but not currently configured on the device return
 * success but set dpi and hz to 0.
 *
 * The returned struct has a refcount of at least 1, use
 * ghostcat_resolution_unref() to release the resources associated.
 *
 * @param profile A previously initialized ratbag profile
 * @param idx The index of the resolution mode to get
 *
 * @return zero on success, non-zero otherwise. On error, dpi and hz are
 * unmodified.
 */
struct ghostcat_resolution *
ghostcat_profile_get_resolution(struct ghostcat_profile *profile, unsigned int idx);

/**
 * @ingroup resolution
 *
 * Add a reference to the resolution. A resolution is destroyed whenever the
 * reference count reaches 0. See @ref ghostcat_resolution_unref.
 *
 * @param resolution A previously initialized valid ratbag resolution
 * @return The passed ratbag resolution
 */
struct ghostcat_resolution *
ghostcat_resolution_ref(struct ghostcat_resolution *resolution);

/**
 * @ingroup resolution
 *
 * Dereference the ratbag resolution. When the internal refcount reaches
 * zero, all resources associated with this object are released. The object
 * must be considered invalid once unref is called.
 *
 * @param resolution A previously initialized ratbag resolution
 * @return Always NULL
 */
struct ghostcat_resolution *
ghostcat_resolution_unref(struct ghostcat_resolution *resolution);

/**
 * @ingroup resolution
 *
 * Set caller-specific data associated with this resolution. libratbag does
 * not manage, look at, or modify this data. The caller must ensure the
 * data is valid.
 *
 * @param resolution A previously initialized resolution
 * @param userdata Caller-specific data passed to the various callback
 * interfaces.
 */
void
ghostcat_resolution_set_user_data(struct ghostcat_resolution *resolution, void *userdata);

/**
 * @ingroup resolution
 *
 * Get the caller-specific data associated with this resolution, if any.
 *
 * @param resolution A previously initialized ratbag resolution
 * @return The caller-specific data previously assigned in
 * ghostcat_resolution_set_user_data().
 */
void*
ghostcat_resolution_get_user_data(const struct ghostcat_resolution *resolution);

/**
 * @ingroup resolution
 *
 * Check if a resolution has a specific capability.
 *
 * @return non-zero if the capability is available, zero otherwise.
 */
bool
ghostcat_resolution_has_capability(const struct ghostcat_resolution *resolution,
				 enum ghostcat_resolution_capability cap);

/**
 * @ingroup resolution
 *
 * Set the resolution in DPI for the resolution mode.
 * If the resolution has the @ref
 * GHOSTCAT_RESOLUTION_CAP_SEPARATE_XY_RESOLUTION capability, this function
 * sets both x and y resolution to the given value.
 *
 * A value of 0 for dpi disables the mode.
 *
 * If the resolution mode is the currently active mode and the profile is
 * the currently active profile, the change takes effect immediately.
 *
 * @param resolution A previously initialized ratbag resolution
 * @param dpi Set to the resolution in dpi, 0 to disable
 *
 * @return zero on success or an error code on failure
 */
enum ghostcat_error_code
ghostcat_resolution_set_dpi(struct ghostcat_resolution *resolution,
			  unsigned int dpi);

/**
 * @ingroup resolution
 *
 * Set the x and y resolution in DPI for the resolution mode.
 * If the resolution does not have the @ref
 * GHOSTCAT_RESOLUTION_CAP_SEPARATE_XY_RESOLUTION capability, this function
 * returns an error and does nothing.
 *
 * A value of 0 for both x and y disables the mode. If either value is 0 and
 * the other value is non-zero, this function returns an error and does
 * nothing.
 *
 * If the resolution mode is the currently active mode and the profile is
 * the currently active profile, the change takes effect immediately.
 *
 * @param resolution A previously initialized ratbag resolution with the
 * @ref GHOSTCAT_RESOLUTION_CAP_SEPARATE_XY_RESOLUTION capability
 * @param x The x resolution in dpi
 * @param y The y resolution in dpi
 *
 * @return zero on success or an error code on failure
 */
enum ghostcat_error_code
ghostcat_resolution_set_dpi_xy(struct ghostcat_resolution *resolution,
			     unsigned int x, unsigned int y);

/**
 * @ingroup resolution
 *
 * Get the resolution in DPI for the resolution mode.
 * If the resolution has the @ref
 * GHOSTCAT_RESOLUTION_CAP_SEPARATE_XY_RESOLUTION capability, this function
 * returns the x resolution, see ghostcat_resolution_get_dpi_x().
 *
 * A value of 0 for dpi indicates the mode is disabled.
 *
 * @param resolution A previously initialized ratbag resolution
 *
 * @return The resolution in dpi
 */
int
ghostcat_resolution_get_dpi(const struct ghostcat_resolution *resolution);

/**
 * @ingroup resolution
 *
 * Get the number of resolutions in DPI available for this resolution.
 * The list of DPI values is sorted in ascending order but may be filtered
 * by libratbag and does not necessarily reflect all resolutions supported
 * by the physical device.
 *
 * This function writes at most nres values but returns the number of
 * DPI values available on this resolution. In other words, if it returns a
 * number larger than nres, call it again with an array the size of the
 * return value.
 *
 * @param[out] resolutions Set to the supported DPI values in ascending order
 * @param[in] nres The number of elements in resolutions
 *
 * @return The number of valid items in resolutions. If the returned value
 * is larger than nres, the list was truncated.
 */
size_t
ghostcat_resolution_get_dpi_list(const struct ghostcat_resolution *resolution,
			       unsigned int *resolutions,
			       size_t nres);

/**
 * @ingroup resolution
 *
 * Get the x resolution in DPI for the resolution mode. If the resolution
 * does not have the @ref GHOSTCAT_RESOLUTION_CAP_SEPARATE_XY_RESOLUTION
 * capability, this function is identical to ghostcat_resolution_get_dpi().
 *
 * A value of 0 for dpi indicates the mode is disabled.
 *
 * @param resolution A previously initialized ratbag resolution with the
 * @ref GHOSTCAT_RESOLUTION_CAP_SEPARATE_XY_RESOLUTION capability
 *
 * @return The resolution in dpi
 */
int
ghostcat_resolution_get_dpi_x(const struct ghostcat_resolution *resolution);

/**
 * @ingroup resolution
 *
 * Get the y resolution in DPI for the resolution mode. If the resolution
 * does not have the @ref GHOSTCAT_RESOLUTION_CAP_SEPARATE_XY_RESOLUTION
 * capability, this function is identical to ghostcat_resolution_get_dpi().
 *
 * A value of 0 for dpi indicates the mode is disabled.
 *
 * @param resolution A previously initialized ratbag resolution with the
 * @ref GHOSTCAT_RESOLUTION_CAP_SEPARATE_XY_RESOLUTION capability
 *
 * @return The resolution in dpi
 */
int
ghostcat_resolution_get_dpi_y(const struct ghostcat_resolution *resolution);

/**
 * @ingroup resolution
 *
 * Activate the given resolution mode. If the mode is not configured, this
 * function returns an error and the result is undefined.
 *
 * The mode must be one of the current profile, otherwise an error is
 * returned.
 *
 * @param resolution A previously initialized ratbag resolution
 *
 * @return zero on success or an error code on failure
 */
enum ghostcat_error_code
ghostcat_resolution_set_active(struct ghostcat_resolution *resolution);

/**
 * @ingroup resolution
 *
 * Check if the resolution mode is the currently active one.
 *
 * If the profile is the currently active profile, the mode is the one
 * currently active. For profiles not currently active, this always returns 0.
 *
 * @param resolution A previously initialized ratbag resolution
 *
 * @return Non-zero if the resolution mode is the active one, zero
 * otherwise.
 */
bool
ghostcat_resolution_is_active(const struct ghostcat_resolution *resolution);

/**
 * @ingroup resolution
 *
 * Set the default resolution mode for the associated profile. When the
 * device switches to the profile next, this mode will be the active
 * resolution. If the mode is not configured, this function returns an error
 * and the result is undefined.
 *
 * This only switches the default resolution, not the currently active
 * resolution. Use ghostcat_resolution_set_active() instead.
 *
 * @param resolution A previously initialized ratbag resolution
 *
 * @return zero on success or an error code on failure
 */
enum ghostcat_error_code
ghostcat_resolution_set_default(struct ghostcat_resolution *resolution);

/**
 * @ingroup resolution
 *
 * Check if the resolution mode is the default one in this profile.
 *
 * The default resolution is the one the device selects when switching to
 * the corresponding profile. It may not be the currently active resolution,
 * use ghostcat_resolution_is_active() instead.
 *
 * @param resolution A previously initialized ratbag resolution
 *
 * @return Non-zero if the resolution mode is the default one, zero
 * otherwise.
 */
bool
ghostcat_resolution_is_default(const struct ghostcat_resolution *resolution);

/**
 * @ingroup resolution
 *
 * Set this resolution as the DPI shift target. The DPI shift target is
 * the resolution used when the DPI shift button (Resolution Switch) is held.
 *
 * @param resolution A previously initialized ratbag resolution
 *
 * @return zero on success or an error code on failure
 */
enum ghostcat_error_code
ghostcat_resolution_set_dpi_shift_target(struct ghostcat_resolution *resolution);

/**
 * @ingroup resolution
 *
 * Check if this resolution is the DPI shift target.
 *
 * @param resolution A previously initialized ratbag resolution
 *
 * @return Non-zero if the resolution is the DPI shift target, zero otherwise.
 */
bool
ghostcat_resolution_is_dpi_shift_target(const struct ghostcat_resolution *resolution);

/**
 * @ingroup resolution
 *
 * Set disabled state of the given resolution mode if supported by the device.
 *
 * The mode must not be active or default, otherwise nothing
 * will be changed and an error is returned.
 *
 * @param resolution A previously initialized ratbag resolution
 * @param disable If true disable resolution, else enable
 *
 * @return zero on success or an error code on failure
 */
enum ghostcat_error_code
ghostcat_resolution_set_disabled(struct ghostcat_resolution *resolution, bool disable);

/**
 * @ingroup resolution
 *
 * Check if the resolution mode is currently disabled.
 *
 * @param resolution A previously initialized ratbag resolution
 *
 * @return Non-zero if the resolution mode is disabled, zero otherwise.
 */
bool
ghostcat_resolution_is_disabled(const struct ghostcat_resolution *resolution);

/**
 * @ingroup profile
 *
 * Return a reference to the button given by the index. The order of the
 * buttons is device-specific though indices 0, 1 and 2 should always refer
 * to left, middle, right buttons. Use ghostcat_button_get_type() to get the
 * physical type of the button.
 *
 * The button is refcounted with an initial value of at least 1.
 * Use ghostcat_button_unref() to release the button.
 *
 * @param profile A previously initialized ratbag profile
 * @param index The index of the button
 *
 * @return A button context, or NULL if the button does not exist.
 *
 * @see ghostcat_device_get_num_buttons
 */
struct ghostcat_button*
ghostcat_profile_get_button(struct ghostcat_profile *profile, unsigned int index);

/**
 * @ingroup button
 *
 * Set caller-specific data associated with this button. libratbag does
 * not manage, look at, or modify this data. The caller must ensure the
 * data is valid.
 *
 * @param button A previously initialized button
 * @param userdata Caller-specific data passed to the various callback
 * interfaces.
 */
void
ghostcat_button_set_user_data(struct ghostcat_button *button, void *userdata);

/**
 * @ingroup button
 *
 * Get the caller-specific data associated with this button, if any.
 *
 * @param button A previously initialized ratbag button
 * @return The caller-specific data previously assigned in
 * ghostcat_button_set_user_data().
 */
void*
ghostcat_button_get_user_data(const struct ghostcat_button *button);

/**
 * @ingroup button
 *
 * @return The type of the action currently configured for this button
 */
enum ghostcat_button_action_type
ghostcat_button_get_action_type(const struct ghostcat_button *button);

/**
 * @ingroup button
 *
 * Check if a button supports a specific action type. Not all devices allow
 * all buttons to be assigned any action. Ability to change a button to a
 * given action type does not guarantee that any specific action can be
 * configured.
 *
 * @note It is a client bug to pass in @ref
 * GHOSTCAT_BUTTON_ACTION_TYPE_UNKNOWN or @ref
 * GHOSTCAT_BUTTON_ACTION_TYPE_NONE.
 *
 * @param button A previously initialized button
 * @param action_type An action type
 *
 * @return non-zero if the action type is supported, zero otherwise.
 */
bool
ghostcat_button_has_action_type(const struct ghostcat_button *button,
			      enum ghostcat_button_action_type action_type);

/**
 * @ingroup button
 *
 * If a button's action is @ref GHOSTCAT_BUTTON_ACTION_TYPE_BUTTON,
 * this function returns the logical button number this button is mapped to,
 * starting at 1. The button numbers are in sequence and do not correspond
 * to any meaning other than its numeric value. It is up to the input stack
 * how to map that logical button number, but usually buttons 1, 2 and 3 are
 * mapped into left, middle, right.
 *
 * If the button's action type is not @ref GHOSTCAT_BUTTON_ACTION_TYPE_BUTTON,
 * this function returns 0.
 *
 * @return The logical button number this button sends.
 * @retval 0 This button is disabled or its action type is not @ref
 * GHOSTCAT_BUTTON_ACTION_TYPE_BUTTON.
 *
 * @see ghostcat_button_set_button
 */
unsigned int
ghostcat_button_get_button(const struct ghostcat_button *button);

/**
 * @ingroup button
 *
 * See ghostcat_button_get_button() for a description of the button number.
 *
 * @param button A previously initialized ratbag button
 * @param btn The logical button number to assign to this button.
 * @return 0 on success or an error code otherwise. On success, the button's
 * action is set to @ref GHOSTCAT_BUTTON_ACTION_TYPE_BUTTON.
 *
 * @see ghostcat_button_get_button
 */
enum ghostcat_error_code
ghostcat_button_set_button(struct ghostcat_button *button,
			 unsigned int btn);

/**
 * @ingroup button
 *
 * If a button's action is @ref GHOSTCAT_BUTTON_ACTION_TYPE_SPECIAL,
 * this function returns the special function assigned to this button.
 *
 * If the button's action type is not @ref GHOSTCAT_BUTTON_ACTION_TYPE_SPECIAL,
 * this function returns @ref GHOSTCAT_BUTTON_ACTION_SPECIAL_INVALID.
 *
 * @return The special function assigned to this button
 *
 * @see ghostcat_button_set_button
 */
enum ghostcat_button_action_special
ghostcat_button_get_special(const struct ghostcat_button *button);

/**
 * @ingroup led
 *
 * Return a reference to the LED given by the index. The order of the
 * LEDs is device-specific though.
 *
 * The LED is refcounted with an initial value of at least 1.
 * Use ghostcat_led_unref() to release the LED.
 *
 * @param profile A previously initialized ratbag profile
 * @param index The index of the LED
 *
 * @return A LED context, or NULL if the LED does not exist.
 *
 * @see ghostcat_device_get_profile
 */
struct ghostcat_led *
ghostcat_profile_get_led(struct ghostcat_profile *profile, unsigned int index);

/**
 * @ingroup led
 *
 * This function returns true if the given mode is supported by the LED, or
 * false otherwise.
 *
 * @param led A previously initialized ratbag LED
 * @param mode The LED mode @ref ghostcat_led_mode to check for
 * @return True if supported, false otherwise
 *
 * @see ghostcat_led_set_mode
 */
bool
ghostcat_led_has_mode(const struct ghostcat_led *led,
		    enum ghostcat_led_mode mode);

/**
 * @ingroup led
 *
 * This function returns the mode for ghostcat_led.
 *
 * @param led A previously initialized ratbag LED
 * @return The LED mode @ref ghostcat_led_mode
 *
 * @see ghostcat_led_set_mode
 */
enum ghostcat_led_mode
ghostcat_led_get_mode(const struct ghostcat_led *led);
/**
 * @ingroup led
 *
 * This function returns the led color.
 *
 * If any color scaling applies because of the device's color depth
 * this is not reflected in the returned value. In other words,
 * the returned value always matches the most recent value provided
 * to ghostcat_led_set_color().
 *
 * @param led A previously initialized ratbag LED
 * @return The LED color in @ref ghostcat_led_mode
 *
 * @see ghostcat_led_set_color
 */
struct ghostcat_color
ghostcat_led_get_color(const struct ghostcat_led *led);

/**
 * @ingroup led
 *
 * This function returns the color depth of this LED.
 *
 * @param led A previously initialized ratbag LED
 * @return The bit depth of this LED
 *
 * @see ghostcat_led_set_color
 */
enum ghostcat_led_colordepth
ghostcat_led_get_colordepth(const struct ghostcat_led *led);

/**
 * @ingroup led
 *
 * This function returns the LED effect duration.
 *
 * @param led A previously initialized ratbag LED
 * @return The LED duration in ms, can be 0 - 10000
 *
 * @see ghostcat_led_set_effect_duration
 */
int
ghostcat_led_get_effect_duration(const struct ghostcat_led *led);
/**
 * @ingroup led
 *
 * This function returns the LED brightness.
 *
 * @param led A previously initialized ratbag LED
 * @return The LED brightness 0 - 255
 *
 * @see ghostcat_led_get_brightness
 */
unsigned int
ghostcat_led_get_brightness(const struct ghostcat_led *led);

/**
 * @ingroup led
 *
 * this function sets the LED mode.
 *
 * @param led A previously initialized ratbag LED
 * @param mode LED mode @ref ghostcat_led_mode.
 * @return 0 on success or an error code otherwise.
 *
 * @see ghostcat_led_get_mode
 */
enum ghostcat_error_code
ghostcat_led_set_mode(struct ghostcat_led *led, enum ghostcat_led_mode mode);

/**
 * @ingroup led
 *
 * If the LED's mode is @ref GHOSTCAT_LED_ON or @ref GHOSTCAT_LED_BREATHING
 * then this function sets the LED color, otherwise it has no effect.
 *
 * The color provided has to be within the allowed color range (see @ref
 * ghostcat_color). libratbag silently scales and/or clamps this range into
 * the device's color depth. It is the caller's responsibility to set the
 * colors in a non-ambiguous way for the device's bit depth. See @ref
 * ghostcat_led_colordepth for more details.
 *
 * @param led A previously initialized ratbag LED
 * @param color A LED color.
 * @return 0 on success or an error code otherwise.
 *
 * @see ghostcat_led_get_color
 */
enum ghostcat_error_code
ghostcat_led_set_color(struct ghostcat_led *led, struct ghostcat_color color);

/**
 * @ingroup led
 *
 * If the LED's mode is @ref GHOSTCAT_LED_CYCLE or @ref GHOSTCAT_LED_BREATHING
 * then this function sets the LED duration in ms
 *
 * @param led A previously initialized ratbag LED
 * @param rate Effect duration in ms, 0 - 10000
 * @return 0 on success or an error code otherwise.
 *
 * @see ghostcat_led_get_effect_duration
 */
enum ghostcat_error_code
ghostcat_led_set_effect_duration(struct ghostcat_led *led, unsigned int rate);

/**
 * @ingroup led
 *
 * If the LED's mode is @ref GHOSTCAT_LED_CYCLE or @ref GHOSTCAT_LED_BREATHING
 * then this function sets the LED brightness, otherwise it has no effect.
 *
 * @param led A previously initialized ratbag LED
 * @param brightness Effect brightness 0 - 255
 * @return 0 on success or an error code otherwise.
 *
 * @see ghostcat_led_get_brightness
 */
enum ghostcat_error_code
ghostcat_led_set_brightness(struct ghostcat_led *led, unsigned int brightness);

/**
 * @ingroup button
 *
 * This function sets the special function assigned to this button.
 *
 * @param button A previously initialized ratbag button
 * @param action The special action to assign to this button.
 * @return 0 on success or an error code otherwise. On success, the button's
 * action is set to @ref GHOSTCAT_BUTTON_ACTION_TYPE_SPECIAL.
 *
 * @see ghostcat_button_get_button
 */
enum ghostcat_error_code
ghostcat_button_set_special(struct ghostcat_button *button,
			  enum ghostcat_button_action_special action);

/**
 * @ingroup button
 *
 * If a button's action is @ref GHOSTCAT_BUTTON_ACTION_TYPE_KEY,
 * this function returns the key or button configured for this button.
 *
 * If the button's action type is not @ref GHOSTCAT_BUTTON_ACTION_TYPE_KEY,
 * this function returns 0 and leaves modifiers and sz untouched.
 *
 * @param button A previously initialized ratbag button
 *
 * @note The caller must ensure that modifiers is large enough to accommodate
 * for the key combination.
 *
 * @return The key code as defined in linux/input-event-codes.h.
 */
unsigned int
ghostcat_button_get_key(const struct ghostcat_button *button);

/**
 * @ingroup button
 *
 * @param button A previously initialized ratbag button
 * @param key The key number to assign to this button, one of KEY_* as
 * defined in linux/input-event-codes.h
 *
 * @return 0 on success or an error code otherwise. On success, the button's
 * action is set to @ref GHOSTCAT_BUTTON_ACTION_TYPE_KEY.
 */
enum ghostcat_error_code
ghostcat_button_set_key(struct ghostcat_button *button, unsigned int key);

/**
 * @ingroup button
 *
 * @param button A previously initialized ratbag button
 *
 * @return 0 on success or an error code otherwise. On success, the button's
 * action is set to @ref GHOSTCAT_BUTTON_ACTION_TYPE_NONE.
 */
enum ghostcat_error_code
ghostcat_button_disable(struct ghostcat_button *button);

/**
 * @ingroup button
 *
 * @param macro A previously initialized ratbag button macro
 *
 * @return The name of the macro
 */
const char *
ghostcat_button_macro_get_name(const struct ghostcat_button_macro *macro);

/**
 * @ingroup button
 *
 * @param macro A previously initialized ratbag button macro
 *
 * @return The maximum number of events that can be assigned to this macro
 */
unsigned int
ghostcat_button_macro_get_num_events(const struct ghostcat_button_macro *macro);

/**
 * @ingroup button
 *
 * Returns the macro event type configured for the event at the
 * given index.
 *
 * The behavior of this function for an index equal to or greater than the
 * return value of ghostcat_button_macro_get_num_events() is undefined.
 *
 * @param macro A previously initialized ratbag button macro
 * @param index An index of the event within the macro we are interested in.
 *
 * @return The type of the event at the given index
 */
enum ghostcat_macro_event_type
ghostcat_button_macro_get_event_type(const struct ghostcat_button_macro *macro,
				   unsigned int index);

/**
 * @ingroup button
 *
 * If the event stored at the given index is @ref
 * GHOSTCAT_MACRO_EVENT_KEY_PRESSED or @ref GHOSTCAT_MACRO_EVENT_KEY_RELEASED,
 * this function returns the key code configured for the event at the given
 * index.
 *
 * The behavior of this function for an index equal to or greater than the
 * return value of ghostcat_button_macro_get_num_events() is undefined.
 *
 * @param macro A previously initialized ratbag button macro
 * @param index An index of the event within the macro we are interested in.
 *
 * @return The key of the event at the given index
 */
int
ghostcat_button_macro_get_event_key(const struct ghostcat_button_macro*macro,
				  unsigned int index);

/**
 * @ingroup button
 *
 * If the event stored at the given index is @ref GHOSTCAT_MACRO_EVENT_WAIT,
 * this function returns the timeout configured for the event at the given
 * index.
 *
 * The behavior of this function for an index equal to or greater than the
 * return value of ghostcat_button_macro_get_num_events() is undefined.
 *
 * @param macro A previously initialized ratbag button macro
 * @param index An index of the event within the macro we are interested in.
 *
 * @return The timeout of the event at the given index
 */
int
ghostcat_button_macro_get_event_timeout(const struct ghostcat_button_macro *macro,
				      unsigned int index);

/**
 * @ingroup button
 *
 * Sets the button's action to @ref GHOSTCAT_BUTTON_ACTION_TYPE_MACRO and
 * assigns the given macro to this button.
 *
 * libratbag does not use the macro struct passed in, it extracts the
 * required information from the struct. Changes to the macro after a call
 * to ghostcat_button_set_macro() are not reflected in the device until a
 * subsequent call to ghostcat_button_set_macro().
 *
 * @param button A previously initialized ratbag button
 * @param macro A fully initialized macro
 *
 * @return 0 on success or nonzero otherwise
 */
enum ghostcat_error_code
ghostcat_button_set_macro(struct ghostcat_button *button,
			const struct ghostcat_button_macro *macro);

/**
 * @ingroup button
 *
 * Initialize a new button macro.
 *
 * The macro is refcounted with an initial value of at least 1.
 * Use ghostcat_button_macro_unref() to release the macro.
 *
 * Note that some devices have limited storage for the macro names.
 * libratbag silently shortens macro names to the longest string the device
 * is capable of storing.
 *
 * @param name The name to assign to this macro.
 *
 * @return An "empty" button macro
 */
struct ghostcat_button_macro *
ghostcat_button_macro_new(const char *name);

/**
 * @ingroup button
 *
 * If a button's action is @ref GHOSTCAT_BUTTON_ACTION_TYPE_MACRO,
 * this function returns the current button macro. The macro is a copy of
 * the one used on the device, changes to the macro are not reflected on the
 * device until a subsequent call to ghostcat_button_set_macro().
 *
 * If a button's action is not @ref GHOSTCAT_BUTTON_ACTION_TYPE_MACRO,
 * this function returns NULL.
 *
 * @param button A previously initialized ratbag button
 */
struct ghostcat_button_macro *
ghostcat_button_get_macro(struct ghostcat_button *button);

/**
 * @ingroup button
 *
 * Sets the macro's event at the given index to the given type with the
 * key code or timeout given.
 *
 * The behavior of this function for an index equal to or greater than the
 * return value of ghostcat_button_macro_get_num_events() is undefined.
 */
enum ghostcat_error_code
ghostcat_button_macro_set_event(struct ghostcat_button_macro *macro,
			      unsigned int index,
			      enum ghostcat_macro_event_type type,
			      unsigned int data);

/**
 * @ingroup button
 *
 * Add a reference to the macro. A macro is destroyed whenever the
 * reference count reaches 0. See @ref ghostcat_button_macro_unref.
 *
 * @param macro A previously initialized valid ratbag button macro
 * @return The passed ratbag macro
 */
struct ghostcat_button_macro *
ghostcat_button_macro_ref(struct ghostcat_button_macro *macro);

/**
 * @ingroup button
 *
 * Dereference the ratbag button macro. When the internal refcount reaches
 * zero, all resources associated with this object are released. The object
 * must be considered invalid once unref is called.
 *
 * @param macro A previously initialized ratbag button macro
 * @return Always NULL
 */
struct ghostcat_button_macro *
ghostcat_button_macro_unref(struct ghostcat_button_macro *macro);

/**
 * @ingroup button
 *
 * Add a reference to the button. A button is destroyed whenever the
 * reference count reaches 0. See @ref ghostcat_button_unref.
 *
 * @param button A previously initialized valid ratbag button
 * @return The passed ratbag button
 */
struct ghostcat_button *
ghostcat_button_ref(struct ghostcat_button *button);

/**
 * @ingroup button
 *
 * Dereference the ratbag button. When the internal refcount reaches
 * zero, all resources associated with this object are released. The object
 * must be considered invalid once unref is called.
 *
 * @param button A previously initialized ratbag button
 * @return Always NULL
 */
struct ghostcat_button *
ghostcat_button_unref(struct ghostcat_button *button);

/**
 * @ingroup led
 *
 * Add a reference to the led. A led is destroyed whenever the
 * reference count reaches 0. See @ref ghostcat_led_unref.
 *
 * @param led A previously initialized valid ratbag led
 * @return The passed ratbag led
 */
struct ghostcat_led *
ghostcat_led_ref(struct ghostcat_led *led);

/**
 * @ingroup led
 *
 * Dereference the ratbag led. When the internal refcount reaches
 * zero, all resources associated with this object are released. The object
 * must be considered invalid once unref is called.
 *
 * @param led A previously initialized ratbag led
 * @return Always NULL
 */
struct ghostcat_led *
ghostcat_led_unref(struct ghostcat_led *led);
