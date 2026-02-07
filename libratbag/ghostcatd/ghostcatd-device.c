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
#include <limits.h>
#include <libratbag.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <systemd/sd-bus.h>
#include <systemd/sd-event.h>
#include "ghostcatd.h"
#include "shared-macro.h"
#include <rbtree/shared-rbtree.h>

#include "libghostcat-util.h"

struct ghostcatd_device {
	unsigned int refcount;

	struct ghostcatd *ctx;
	RBNode node;
	char *sysname;
	char *path;
	struct ghostcat_device *lib_device;

	sd_bus_slot *profile_vtable_slot;
	sd_bus_slot *profile_enum_slot;
	unsigned int n_profiles;
	struct ghostcatd_profile **profiles;
};

#define ghostcatd_device_from_node(_ptr) \
		rbnode_of((_ptr), struct ghostcatd_device, node)

static int ghostcatd_device_find_profile(sd_bus *bus,
				       const char *path,
				       const char *interface,
				       void *userdata,
				       void **found,
				       sd_bus_error *error)
{
	_cleanup_(freep) char *name = NULL;
	struct ghostcatd_device *device = userdata;
	unsigned int index = 0;
	int r;

	r = sd_bus_path_decode_many(path,
				    GHOSTCATD_OBJ_ROOT "/profile/%/p%",
				    NULL,
				    &name);
	if (r <= 0)
		return r;

	r = safe_atou(name, &index);
	if (r < 0)
		return 0;

	if (index >= device->n_profiles || !device->profiles[index])
		return 0;

	*found = device->profiles[index];
	return 1;
}

static int ghostcatd_device_list_profiles(sd_bus *bus,
					const char *path,
					void *userdata,
					char ***paths,
					sd_bus_error *error)
{
	struct ghostcatd_device *device = userdata;
	struct ghostcatd_profile *profile;
	char **profiles;
	unsigned int i;

	profiles = zalloc((device->n_profiles + 1) * sizeof(char *));

	for (i = 0; i < device->n_profiles; ++i) {
		profile = device->profiles[i];
		if (!profile)
			continue;

		profiles[i] = strdup_safe(ghostcatd_profile_get_path(profile));
	}

	profiles[i] = NULL;
	*paths = profiles;
	return 1;
}

static int ghostcatd_device_get_device_name(sd_bus *bus,
					  const char *path,
					  const char *interface,
					  const char *property,
					  sd_bus_message *reply,
					  void *userdata,
					  sd_bus_error *error)
{
	struct ghostcatd_device *device = userdata;
	const char *name;

	name = ghostcat_device_get_name(device->lib_device);
	if (!name) {
		log_error("%s: failed to fetch name\n",
			  ghostcatd_device_get_sysname(device));
		name = "";
	}

	CHECK_CALL(sd_bus_message_append(reply, "s", name));

	return 0;
}

static int ghostcatd_device_get_device_type(sd_bus *bus,
					  const char *path,
					  const char *interface,
					  const char *property,
					  sd_bus_message *reply,
					  void *userdata,
					  sd_bus_error *error)
{
	struct ghostcatd_device *device = userdata;
	enum ghostcat_device_type devicetype;

	devicetype = ghostcat_device_get_device_type(device->lib_device);
	if (!devicetype) {
		log_error("%s: device type unspecified\n",
			  ghostcatd_device_get_sysname(device));
	}

	CHECK_CALL(sd_bus_message_append(reply, "u", devicetype));

	return 0;
}

static int ghostcatd_device_get_profiles(sd_bus *bus,
				       const char *path,
				       const char *interface,
				       const char *property,
				       sd_bus_message *reply,
				       void *userdata,
				       sd_bus_error *error)
{
	struct ghostcatd_device *device = userdata;
	struct ghostcatd_profile *profile;
	unsigned int i;

	CHECK_CALL(sd_bus_message_open_container(reply, 'a', "o"));

	for (i = 0; i < device->n_profiles; ++i) {
		profile = device->profiles[i];
		if (!profile)
			continue;

		CHECK_CALL(sd_bus_message_append(reply,
						 "o",
						 ghostcatd_profile_get_path(profile)));
	}

	CHECK_CALL(sd_bus_message_close_container(reply));

	return 0;
}

static void ghostcatd_device_commit_pending(void *data)
{
	struct ghostcatd_device *device = data;
	int r;

	r = ghostcat_device_commit(device->lib_device);
	if (r)
		log_error("error committing device (%d)\n", r);
	if (r < 0)
		ghostcatd_device_resync(device, device->ctx->bus);

	ghostcatd_for_each_profile_signal(device->ctx->bus,
					device,
					ghostcatd_profile_notify_dirty);

	ghostcatd_device_unref(device);
}

static int ghostcatd_device_commit(sd_bus_message *m,
				 void *userdata,
				 sd_bus_error *error)
{
	struct ghostcatd_device *device = userdata;

	ghostcatd_schedule_task(device->ctx,
			      ghostcatd_device_commit_pending,
			      ghostcatd_device_ref(device));

	CHECK_CALL(sd_bus_reply_method_return(m, "u", 0));

	return 0;
}

static int
ghostcatd_device_get_model(sd_bus *bus,
			 const char *path,
			 const char *interface,
			 const char *property,
			 sd_bus_message *reply,
			 void *userdata,
			 sd_bus_error *error)
{
	struct ghostcatd_device *device = userdata;
	struct ghostcat_device *lib_device = device->lib_device;
	const char *bustype = ghostcat_device_get_bustype(lib_device);
	uint32_t vid = ghostcat_device_get_vendor_id(lib_device),
		 pid = ghostcat_device_get_product_id(lib_device),
		 version = ghostcat_device_get_product_version(lib_device);
	char model[64];

	if (!bustype)
		return sd_bus_message_append(reply, "s", "unknown");

	snprintf(model, sizeof(model), "%s:%04x:%04x:%d",
		 bustype, vid, pid, version);

	return sd_bus_message_append(reply, "s", model);
}

static int
ghostcatd_device_get_firmware_version(sd_bus *bus,
				    const char *path,
				    const char *interface,
				    const char *property,
				    sd_bus_message *reply,
				    void *userdata,
				    sd_bus_error *error)
{
	struct ghostcatd_device *device = userdata;
	struct ghostcat_device *lib_device = device->lib_device;
	const char* version = ghostcat_device_get_firmware_version(lib_device);

	return sd_bus_message_append(reply, "s", version);
}

const sd_bus_vtable ghostcatd_device_vtable[] = {
	SD_BUS_VTABLE_START(0),
	SD_BUS_PROPERTY("Model", "s", ghostcatd_device_get_model, 0, SD_BUS_VTABLE_PROPERTY_CONST),
	SD_BUS_PROPERTY("DeviceType", "u", ghostcatd_device_get_device_type, 0, SD_BUS_VTABLE_PROPERTY_CONST),
	SD_BUS_PROPERTY("Name", "s", ghostcatd_device_get_device_name, 0, SD_BUS_VTABLE_PROPERTY_CONST),
	SD_BUS_PROPERTY("FirmwareVersion", "s", ghostcatd_device_get_firmware_version, 0, SD_BUS_VTABLE_PROPERTY_CONST),
	SD_BUS_PROPERTY("Profiles", "ao", ghostcatd_device_get_profiles, 0, SD_BUS_VTABLE_PROPERTY_CONST),
	SD_BUS_METHOD("Commit", "", "u", ghostcatd_device_commit, SD_BUS_VTABLE_UNPRIVILEGED),
	SD_BUS_SIGNAL("Resync", "", 0),
	SD_BUS_VTABLE_END,
};

int ghostcatd_device_new(struct ghostcatd_device **out,
		       struct ghostcatd *ctx,
		       const char *sysname,
		       struct ghostcat_device *lib_device)
{
	_cleanup_(ghostcatd_device_unrefp) struct ghostcatd_device *device = NULL;
	struct ghostcat_profile *profile;
	unsigned int i;
	int r;

	assert(out);
	assert(ctx);
	assert(sysname);

	device = zalloc(sizeof(*device));
	device->refcount = 1;
	device->ctx = ctx;
	rbnode_init(&device->node);
	device->lib_device = ghostcat_device_ref(lib_device);

	device->sysname = strdup_safe(sysname);

	r = sd_bus_path_encode(GHOSTCATD_OBJ_ROOT "/device",
			       device->sysname,
			       &device->path);
	if (r < 0)
		return r;


	device->n_profiles = ghostcat_device_get_num_profiles(device->lib_device);
	device->profiles = zalloc(device->n_profiles * sizeof(*device->profiles));

	log_info("%s: \"%s\", %d profiles\n",
		 sysname,
		 ghostcat_device_get_name(lib_device),
		 device->n_profiles);

	for (i = 0; i < device->n_profiles; ++i) {
		profile = ghostcat_device_get_profile(device->lib_device, i);
		if (!profile)
			continue;

		r = ghostcatd_profile_new(&device->profiles[i],
					device,
					profile,
					i);
		if (r < 0) {
			errno = -r;
			log_error("%s: failed to allocate profile: %m\n",
				  device->sysname);
		}
	}

	*out = device;
	device = NULL;
	return 0;
}

static void ghostcatd_device_free(struct ghostcatd_device *device)
{
	unsigned int i;

	if (!device)
		return;

	assert(!ghostcatd_device_linked(device));

	for (i = 0; i < device->n_profiles; ++i)
		device->profiles[i] = ghostcatd_profile_free(device->profiles[i]);

	device->profiles = mfree(device->profiles);
	device->lib_device = ghostcat_device_unref(device->lib_device);
	device->path = mfree(device->path);
	device->sysname = mfree(device->sysname);

	assert(!device->lib_device); /* ratbag yields !NULL if still pinned */

	mfree(device);
}

struct ghostcatd_device *ghostcatd_device_ref(struct ghostcatd_device *device)
{
	assert(device->refcount > 0);

	++device->refcount;
	return device;
}

struct ghostcatd_device *ghostcatd_device_unref(struct ghostcatd_device *device)
{
	assert(device->refcount > 0);

	--device->refcount;
	if (device->refcount == 0)
		ghostcatd_device_free(device);

	return NULL;
}

const char *ghostcatd_device_get_sysname(struct ghostcatd_device *device)
{
	assert(device);
	return device->sysname;
}

const char *ghostcatd_device_get_path(struct ghostcatd_device *device)
{
	assert(device);
	return device->path;
}

unsigned int ghostcatd_device_get_num_buttons(struct ghostcatd_device *device)
{
	assert(device);
	return ghostcat_device_get_num_buttons(device->lib_device);
}

unsigned int ghostcatd_device_get_num_leds(struct ghostcatd_device *device)
{
	assert(device);
	return ghostcat_device_get_num_leds(device->lib_device);
}

int ghostcatd_device_resync(struct ghostcatd_device *device, sd_bus *bus)
{
	assert(device);
	assert(bus);

	ghostcatd_for_each_profile_signal(bus, device,
					ghostcatd_profile_resync);

	return sd_bus_emit_signal(bus,
				  device->path,
				  GHOSTCATD_NAME_ROOT ".Device",
				  "Resync",
				  NULL);
}

int ghostcatd_device_poll_active_resolution(struct ghostcatd_device *device, sd_bus *bus)
{
	int changed;

	assert(device);

	changed = ghostcat_device_refresh_active_resolution(device->lib_device);
	if (changed > 0) {
		/* Active resolution changed, emit signals */
		ghostcatd_for_each_profile_signal(bus, device,
						ghostcatd_profile_resync);
	}

	return changed;
}

bool ghostcatd_device_linked(struct ghostcatd_device *device)
{
	return device && rbnode_linked(&device->node);
}

void ghostcatd_device_link(struct ghostcatd_device *device)
{
	_cleanup_(freep) char *prefix = NULL;
	struct ghostcatd_device *iter;
	RBNode **node, *parent;
	int r, v;
	unsigned int i;

	assert(device);
	assert(!ghostcatd_device_linked(device));

	/* find place to link it to */
	parent = NULL;
	node = &device->ctx->device_map.root;
	while (*node) {
		parent = *node;
		iter = ghostcatd_device_from_node(parent);
		v = strcmp(device->sysname, iter->sysname);

		/* if there's a duplicate, the caller screwed up */
		assert(v != 0);

		if (v < 0)
			node = &parent->left;
		else /* if (v > 0) */
			node = &parent->right;
	}

	/* link into context */
	rbtree_add(&device->ctx->device_map, parent, node, &device->node);
	++device->ctx->n_devices;

	/* register profile interfaces */
	r = sd_bus_path_encode_many(&prefix,
				    GHOSTCATD_OBJ_ROOT "/profile/%",
				    device->sysname);
	if (r >= 0) {
		r = sd_bus_add_fallback_vtable(device->ctx->bus,
					       &device->profile_vtable_slot,
					       prefix,
					       GHOSTCATD_NAME_ROOT ".Profile",
					       ghostcatd_profile_vtable,
					       ghostcatd_device_find_profile,
					       device);
		if (r >= 0)
			r = sd_bus_add_node_enumerator(device->ctx->bus,
						       &device->profile_enum_slot,
						       prefix,
						       ghostcatd_device_list_profiles,
						       device);
	}
	if (r < 0) {
		errno = -r;
		log_error("%s: failed to register profile interfaces: %m\n",
			  device->sysname);
		return;
	}

	for (i = 0; i < device->n_profiles; i++) {
		r = ghostcatd_profile_register_resolutions(device->ctx->bus,
							 device,
							 device->profiles[i]);
		if (r < 0) {
			log_error("%s: failed to register resolutions: %m\n",
				  device->sysname);
		}

		r = ghostcatd_profile_register_buttons(device->ctx->bus,
						     device,
						     device->profiles[i]);
		if (r < 0) {
			log_error("%s: failed to register buttons: %m\n",
				  device->sysname);
		}

		r = ghostcatd_profile_register_leds(device->ctx->bus,
						  device,
						  device->profiles[i]);
		if (r < 0) {
			log_error("%s: failed to register leds: %m\n",
				  device->sysname);
		}
	}
}

void ghostcatd_device_unlink(struct ghostcatd_device *device)
{
	if (!ghostcatd_device_linked(device))
		return;

	device->profile_enum_slot = sd_bus_slot_unref(device->profile_enum_slot);
	device->profile_vtable_slot = sd_bus_slot_unref(device->profile_vtable_slot);

	/* unlink from context */
	--device->ctx->n_devices;
	rbtree_remove(&device->ctx->device_map, &device->node);
	rbnode_init(&device->node);
}

struct ghostcatd_device *ghostcatd_device_lookup(struct ghostcatd *ctx,
					     const char *name)
{
	struct ghostcatd_device *device;
	RBNode *node;
	int v;

	assert(ctx);
	assert(name);

	node = ctx->device_map.root;
	while (node) {
		device = ghostcatd_device_from_node(node);
		v = strcmp(name, device->sysname);
		if (!v)
			return device;
		else if (v < 0)
			node = node->left;
		else /* if (v > 0) */
			node = node->right;
	}

	return NULL;
}

struct ghostcatd_device *ghostcatd_device_first(struct ghostcatd *ctx)
{
	struct RBNode *node;

	assert(ctx);

	node = rbtree_first(&ctx->device_map);
	return node ? ghostcatd_device_from_node(node) : NULL;
}

struct ghostcatd_device *ghostcatd_device_next(struct ghostcatd_device *device)
{
	struct RBNode *node;

	assert(device);

	node = rbnode_next(&device->node);
	return node ? ghostcatd_device_from_node(node) : NULL;
}

int ghostcatd_for_each_profile_signal(sd_bus *bus,
				    struct ghostcatd_device *device,
				    int (*func)(sd_bus *bus,
						struct ghostcatd_profile *profile))
{
	int rc = 0;

	for (size_t i = 0; rc == 0 && i < device->n_profiles; i++)
		rc = func(bus, device->profiles[i]);

	return rc;
}
