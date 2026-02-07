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

#include <linux/input.h>
#include <stdio.h>

#include "libghostcat-private.h"
#include "libghostcat-util.h"
#include "libghostcat-test.h"

extern struct ghostcat_driver test_driver;

static inline void
ghostcat_register_test_drivers(struct ghostcat *ratbag)
{
	struct ghostcat_driver *driver;

	/* Don't use a static variable here, otherwise the CK_FORK=no case
	 * will fail */
	list_for_each(driver, &ratbag->drivers, link) {
		if (streq(driver->name, test_driver.name))
			return;
	}

	ghostcat_register_driver(ratbag, &test_driver);
}

LIBGHOSTCAT_EXPORT struct ghostcat_device*
ghostcat_device_new_test_device(struct ghostcat *ratbag,
			      const struct ghostcat_test_device *test_device)
{
	struct ghostcat_device* device = NULL;
#if BUILD_TESTS

	struct input_id id = {
		.bustype = 0x00,
		.vendor = 0x00,
		.product = 0x00,
		.version = 0x00,
	};

	ghostcat_register_test_drivers(ratbag);

	if (getenv("GHOSTCAT_TEST") == NULL) {
		fprintf(stderr, "GHOSTCAT_TEST environment variable not set\n");
		abort();
	}

	device = ghostcat_device_new(ratbag, NULL, "Test device", &id);

	device->devicetype = TYPE_MOUSE;

	if (!ghostcat_assign_driver(device, &device->ids, test_device)) {
		ghostcat_device_destroy(device);
		return NULL;
	}
#endif

	return device;
}
