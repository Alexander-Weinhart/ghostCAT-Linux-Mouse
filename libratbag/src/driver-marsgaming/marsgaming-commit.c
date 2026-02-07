#include "marsgaming-commit.h"

#include "marsgaming-definitions.h"
#include "marsgaming-probe.h"
#include "marsgaming-query.h"
#include "marsgaming-protocol.h"
#include "marsgaming-buttons.h"
#include "marsgaming-command.h"

static int
marsgaming_commit_button(struct ghostcat_button *button)
{
	struct marsgaming_profile_drv_data *profile_data = marsgaming_profile_get_drv_data(button->profile);
	struct marsgaming_button_info *button_info = &profile_data->buttons_report.buttons[button->index];
	const struct marsgaming_optional_button_info button_data = marsgaming_button_of_type(button);
	if (button_data.is_present)
		*button_info = button_data.button_info;
	return 0;
}

static int
marsgaming_commit_led(struct ghostcat_led *led)
{
	if (!led->dirty)
		return 0;
	marsgaming_command_profile_set_led(led);
	return 0;
}

static int
marsgaming_commit_profile_report_rate(struct ghostcat_profile *profile)
{
	if (!profile->rate_dirty)
		return 0;

	uint8_t polling_interval = 1000 / ghostcat_profile_get_report_rate(profile);
	marsgaming_command_profile_set_polling_interval(profile, polling_interval);
	return 0;
}

static int
marsgaming_commit_profile_buttons(struct ghostcat_profile *profile)
{
	bool buttons_dirty = false;
	struct ghostcat_button *button;
	ghostcat_profile_for_each_button(profile, button) {
		if (button->dirty) {
			buttons_dirty = true;
			break;
		}
	}
	if (!buttons_dirty)
		return 0;
	ghostcat_profile_for_each_button(profile, button) {
		if (!button->dirty)
			continue;
		marsgaming_commit_button(button);
	}
	marsgaming_command_profile_set_buttons(profile);
	return 0;
}

static int
marsgaming_commit_profile_leds(struct ghostcat_profile *profile)
{
	struct ghostcat_led *led;
	ghostcat_profile_for_each_led(profile, led) {
		marsgaming_commit_led(led);
	}
	return 0;
}

static int
marsgaming_commit_profile_resolutions(struct ghostcat_profile *profile)
{
	bool resolutions_dirty = false;
	struct ghostcat_resolution *resolution;
	ghostcat_profile_for_each_resolution(profile, resolution) {
		if (resolution->dirty) {
			resolutions_dirty = true;
			break;
		}
	}
	if (!resolutions_dirty)
		return 0;
	struct marsgaming_profile_drv_data *profile_data = marsgaming_profile_get_drv_data(profile);
	ghostcat_profile_for_each_resolution(profile, resolution) {
		if (!resolution->dirty)
			continue;
		// Modify the drv_data report so we can send it to the mouse
		struct marsgaming_report_resolution_info *resolution_info = &profile_data->resolutions_report.resolutions[resolution->index];
		resolution_info->enabled = true;
		resolution_info->x_res = resolution->dpi_x / MARSGAMING_MM4_RES_SCALING;
		resolution_info->y_res = resolution->dpi_y / MARSGAMING_MM4_RES_SCALING;
		resolution_info->led_bitset = ~((~0x0U) << resolution->index);
	}

	marsgaming_command_profile_set_resolutions(profile);

	return 0;
}

static int
marsgaming_commit_profile(struct ghostcat_profile *profile)
{
	if (!profile->dirty)
		return 0;
	marsgaming_commit_profile_report_rate(profile);
	marsgaming_commit_profile_resolutions(profile);
	marsgaming_commit_profile_buttons(profile);
	marsgaming_commit_profile_leds(profile);
	return 0;
}

static int
marsgaming_commit_profiles(struct ghostcat_device *device)
{
	uint8_t current_profile = marsgaming_query_current_profile(device);
	struct ghostcat_profile *profile;
	ghostcat_device_for_each_profile(device, profile) {
		// The user could change the current profile between probe and commit
		// We need to modify the active profile for the led changes to take effect
		// Unsure how this will interact with internals of ratbag
		profile->is_active = current_profile == profile->index;
		marsgaming_commit_profile(profile);
	}
	return 0;
}

int
marsgaming_commit(struct ghostcat_device *device)
{
	ghostcat_open_hidraw(device);

	marsgaming_commit_profiles(device);

	marsgaming_release_device(device);
	return 0;
}

int
marsgaming_set_active_profile(struct ghostcat_device *device, unsigned int profile)
{
	marsgaming_command_set_current_profile(device, profile);

	return 0;
}
