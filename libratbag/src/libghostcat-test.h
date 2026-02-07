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

#include <stdint.h>

#include "libghostcat.h"

#define GHOSTCAT_TEST_MAX_PROFILES 12
#define GHOSTCAT_TEST_MAX_BUTTONS 25
#define GHOSTCAT_TEST_MAX_RESOLUTIONS 8
#define GHOSTCAT_TEST_MAX_LEDS 8

struct ghostcat_test_macro_event {
	enum ghostcat_macro_event_type type;
	unsigned int value;
};

struct ghostcat_test_button {
	enum ghostcat_button_action_type action_type;
	union {
		int button;
		int key;
		enum ghostcat_button_action_special special;
		struct ghostcat_test_macro_event macro[24];
	};
};

struct ghostcat_test_resolution {
	int xres, yres;
	bool active;
	bool dflt;
	bool disabled;
	uint32_t caps[10];

	int dpi_min, dpi_max;
};

struct ghostcat_test_color {
	unsigned short red;
	unsigned short green;
	unsigned short blue;
};

struct ghostcat_test_led {
	enum ghostcat_led_mode mode;
	struct ghostcat_test_color color;
	unsigned int ms;
	unsigned int brightness;
};

struct ghostcat_test_profile {
	char *name;
	struct ghostcat_test_button buttons[GHOSTCAT_TEST_MAX_BUTTONS];
	struct ghostcat_test_resolution resolutions[GHOSTCAT_TEST_MAX_RESOLUTIONS];
	struct ghostcat_test_led leds[GHOSTCAT_TEST_MAX_LEDS];
	bool active;
	bool dflt;
	bool disabled;
	uint32_t caps[10];

	int hz;
	unsigned int report_rates[5];
};

struct ghostcat_test_device {
	unsigned int num_profiles;
	unsigned int num_resolutions;
	unsigned int num_buttons;
	unsigned int num_leds;
	struct ghostcat_test_profile profiles[GHOSTCAT_TEST_MAX_PROFILES];
	void (*destroyed)(struct ghostcat_device *device, void *data);
	void *destroyed_data;
};

struct ghostcat_device* ghostcat_device_new_test_device(struct ghostcat *ratbag,
						    const struct ghostcat_test_device *test_device);

