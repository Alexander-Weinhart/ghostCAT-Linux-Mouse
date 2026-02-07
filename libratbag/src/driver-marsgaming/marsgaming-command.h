#pragma once
#include "libghostcat-private.h"

void
marsgaming_command_set_current_profile(struct ghostcat_device *device, unsigned int profile);

void
marsgaming_command_profile_set_polling_interval(struct ghostcat_profile *profile, uint8_t polling_interval);

void
marsgaming_command_profile_set_led(struct ghostcat_led *led);

void
marsgaming_command_profile_set_resolutions(struct ghostcat_profile *profile);

void
marsgaming_command_profile_set_buttons(struct ghostcat_profile *profile);
