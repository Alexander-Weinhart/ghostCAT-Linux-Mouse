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

#pragma once

/**
 * This file contains enum values that are used in the DBus API and thus
 * considered ABI.
 */

/**
 * @defgroup enums Enumerations used in the DBus API
 */

/**
 * @ingroup enums
 *
 * Error codes used by libratbag.
 */
enum ghostcat_error_code {
	GHOSTCAT_SUCCESS = 0,

	/**
	 * An error occurred on the device. Either the device is not a
	 * libratbag device or communication with the device failed.
	 */
	GHOSTCAT_ERROR_DEVICE = -1000,

	/**
	 * Insufficient capabilities. This error occurs when a requested change is
	 * beyond the device's capabilities.
	 */
	GHOSTCAT_ERROR_CAPABILITY = -1001,

	/**
	 * Invalid value or value range. The provided value or value range
	 * is outside of the legal or supported range.
	 */
	GHOSTCAT_ERROR_VALUE = -1002,

	/**
	 * A low-level system error has occurred, e.g. a failure to access
	 * files that should be there. This error is usually unrecoverable
	 * and libratbag will print a log message with details about the
	 * error.
	 */
	GHOSTCAT_ERROR_SYSTEM = -1003,

	/**
	 * Implementation bug, either in libratbag or in the caller. This
	 * error is usually unrecoverable and libratbag will print a log
	 * message with details about the
	 * error.
	 */
	GHOSTCAT_ERROR_IMPLEMENTATION = -1004,
};

/**
 * @ingroup enums
 */
enum ghostcat_profile_capability {
	GHOSTCAT_PROFILE_CAP_NONE = 0,

	/**
	 * This profile can be assigned as the default profile. A default
	 * profile is the profile that is selected when the device
	 * is plugged in. Where no profile is assigned as the default
	 * profile, the device either picks last-used profile or a specific
	 * profile (usually the first).
	 */
	GHOSTCAT_PROFILE_CAP_SET_DEFAULT = 101,

	/**
	 * The profile can be disabled and enabled. Profiles are not
	 * immediately deleted after being disabled, it is not guaranteed
	 * that the device will remember any disabled profiles the next time
	 * ratbag runs. Furthermore, the order of profiles may get changed
	 * the next time ratbag runs if profiles are disabled.
	 *
	 * Note that this capability only notes the general capability. A
	 * specific profile may still fail to be disabled, e.g. when it is
	 * the last enabled profile on the device.
	 */
	GHOSTCAT_PROFILE_CAP_DISABLE,

	/**
	 * The profile information cannot be queried from the hardware.
	 * Where this capability is present, libratbag cannot
	 * query the device for its current configuration and the
	 * configured resolutions and button mappings are unknown.
	 * libratbag will still provide information about the structure of
	 * the device such as the number of buttons and resolutions.
	 * Clients that encounter a device without this resolution are
	 * encouraged to upload a configuration stored on-disk to the
	 * device to reset the device to a known state.
	 *
	 * Any changes uploaded to the device will be cached in libratbag,
	 * once a client has sent a full configuration to the device
	 * libratbag can be used to query the device as normal.
	 */
	GHOSTCAT_PROFILE_CAP_WRITE_ONLY,
};

/**
 * @ingroup enums
 */
enum ghostcat_resolution_capability {
	/**
	 * The resolution can be set for x and y separately.
	 */
	GHOSTCAT_RESOLUTION_CAP_SEPARATE_XY_RESOLUTION = 1,

	/**
	 * The resolution can be disabled and enabled. This is intended for
	 * devices with a static number of resolutions that can not be
	 * changed.
	 *
	 * Note that this capability only notes the general capability. A
	 * specific resolution may still fail to be disabled, e.g. when it
	 * is the active resolution on the device.
	 *
	 * TODO: Devices with a dynamic amount of resolutions need an own
	 * capability to add or remove resolutions.
	 */
	GHOSTCAT_RESOLUTION_CAP_DISABLE,
};

/**
 * @ingroup enums
 *
 * The type assigned to a button.
 */
enum ghostcat_button_action_type {
	/**
	 * Button is disabled
	 */
	GHOSTCAT_BUTTON_ACTION_TYPE_NONE = 0,
	/**
	 * Button sends numeric button events
	 */
	GHOSTCAT_BUTTON_ACTION_TYPE_BUTTON,
	/**
	 * Button triggers a mouse-specific special function. This includes
	 * resolution changes and profile changes.
	 */
	GHOSTCAT_BUTTON_ACTION_TYPE_SPECIAL,
	/**
	 * Button sends a key or key + modifier combination
	 */
	GHOSTCAT_BUTTON_ACTION_TYPE_KEY,
	/**
	 * Button sends a user-defined key or button sequence
	 */
	GHOSTCAT_BUTTON_ACTION_TYPE_MACRO,
	/**
	 * Button action is unknown
	 */
	GHOSTCAT_BUTTON_ACTION_TYPE_UNKNOWN = 1000,
};

/**
 * @ingroup enums
 */
enum ghostcat_button_action_special {
	/**
	 * This button is not set up for a special action
	 */
	GHOSTCAT_BUTTON_ACTION_SPECIAL_INVALID = -1,
	GHOSTCAT_BUTTON_ACTION_SPECIAL_UNKNOWN = (1 << 30),

	GHOSTCAT_BUTTON_ACTION_SPECIAL_DOUBLECLICK,

	/* Wheel mappings */
	GHOSTCAT_BUTTON_ACTION_SPECIAL_WHEEL_LEFT,
	GHOSTCAT_BUTTON_ACTION_SPECIAL_WHEEL_RIGHT,
	GHOSTCAT_BUTTON_ACTION_SPECIAL_WHEEL_UP,
	GHOSTCAT_BUTTON_ACTION_SPECIAL_WHEEL_DOWN,
	GHOSTCAT_BUTTON_ACTION_SPECIAL_RATCHET_MODE_SWITCH,

	/* DPI switch */
	GHOSTCAT_BUTTON_ACTION_SPECIAL_RESOLUTION_CYCLE_UP,
	GHOSTCAT_BUTTON_ACTION_SPECIAL_RESOLUTION_CYCLE_DOWN,
	GHOSTCAT_BUTTON_ACTION_SPECIAL_RESOLUTION_UP,
	GHOSTCAT_BUTTON_ACTION_SPECIAL_RESOLUTION_DOWN,
	GHOSTCAT_BUTTON_ACTION_SPECIAL_RESOLUTION_ALTERNATE,
	GHOSTCAT_BUTTON_ACTION_SPECIAL_RESOLUTION_DEFAULT,

	/* Profile */
	GHOSTCAT_BUTTON_ACTION_SPECIAL_PROFILE_CYCLE_UP,
	GHOSTCAT_BUTTON_ACTION_SPECIAL_PROFILE_CYCLE_DOWN,
	GHOSTCAT_BUTTON_ACTION_SPECIAL_PROFILE_UP,
	GHOSTCAT_BUTTON_ACTION_SPECIAL_PROFILE_DOWN,

	/* second mode for buttons */
	GHOSTCAT_BUTTON_ACTION_SPECIAL_SECOND_MODE,

	/* battery level */
	GHOSTCAT_BUTTON_ACTION_SPECIAL_BATTERY_LEVEL,

};

/**
 * @ingroup enums
 *
 * Each LED mode has different properties, e.g. the brightness and rate are only
 * available in modes @ref GHOSTCAT_LED_CYCLE and @ref GHOSTCAT_LED_BREATHING modes
 */
enum ghostcat_led_mode {
	/**
	 * led is now off
	 */
	GHOSTCAT_LED_OFF = 0,

	/**
	 * led is on with static color
	 */
	GHOSTCAT_LED_ON,

	/**
	 * led is cycling between all colors
	 */
	GHOSTCAT_LED_CYCLE,

	/**
	 * led is pulsating with static color
	 */
	GHOSTCAT_LED_BREATHING,
};

/**
 * @ingroup enums
 */
enum ghostcat_led_colordepth {
	/**
	 * The device only supports a single color.
	 * All color components should be set to 255.
	 */
	GHOSTCAT_LED_COLORDEPTH_MONOCHROME = 0,
	/**
	 * The device supports RBG color with 8 bits per color.
	 */
	GHOSTCAT_LED_COLORDEPTH_RGB_888,
	/**
	 * The device supports RBG colors with 1 bit per color.
	 */
	GHOSTCAT_LED_COLORDEPTH_RGB_111,
};

/**
 * @ingroup enums
 *
 * Macro event types describing the event.
 */
enum ghostcat_macro_event_type {
	GHOSTCAT_MACRO_EVENT_INVALID = -1,
	GHOSTCAT_MACRO_EVENT_NONE = 0,
	GHOSTCAT_MACRO_EVENT_KEY_PRESSED,
	GHOSTCAT_MACRO_EVENT_KEY_RELEASED,
	GHOSTCAT_MACRO_EVENT_WAIT,
};

/**
 * @ingroup enums
 *
 * Device Types
 * Describes the types specified in the .device files
 */
enum ghostcat_device_type {
	/**
	 * Used when no DeviceType property could be found
	 */
	TYPE_UNSPECIFIED = 0,
	/**
	 * Used for any other device type than a mouse or a keyboard
	 * for example headsets, mousepads etc.
	 */
	TYPE_OTHER,
	/**
	 * Used for mice
	 */
	TYPE_MOUSE,
	/**
	 * Used for keyboards
	 */
	TYPE_KEYBOARD,
};
