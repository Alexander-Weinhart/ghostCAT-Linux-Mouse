#pragma once
#include "marsgaming-protocol.h"
#include "libghostcat-private.h"

uint8_t
marsgaming_query_current_profile(struct ghostcat_device *device);

struct marsgaming_report_resolutions
marsgaming_query_profile_resolutions(struct ghostcat_profile *profile);

struct marsgaming_report_buttons
marsgaming_query_profile_buttons(struct ghostcat_profile *profile);

uint8_t
marsgaming_query_profile_polling_interval(struct ghostcat_profile *profile);

struct marsgaming_report_led
marsgaming_query_profile_led(struct ghostcat_profile *profile);
