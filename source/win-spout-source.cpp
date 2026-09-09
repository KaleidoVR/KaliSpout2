/**
 * Copyright Off World Live Ltd (https://offworld.live), 2019-2021
 *
 * and licenced under the GPL v2 (https://www.gnu.org/licenses/old-licenses/gpl-2.0.en.html)
 *
 * Many thanks to authors of https://github.com/baffler/OBS-OpenVR-Input-Plugin which
 * was used as guidance to working with the OBS Studio APIs
 */

#include <obs-module.h>
#include <util/threading.h>
#include "win-spout.h"

#include "SpoutLibrary.h"
#pragma comment(lib, "SpoutLibrary.lib")

#define debug(message, ...) blog(LOG_DEBUG, "[%s] " message, obs_source_get_name(context->source), ##__VA_ARGS__)
#define info(message, ...) blog(LOG_INFO, "[%s] " message, obs_source_get_name(context->source), ##__VA_ARGS__)
#define warn(message, ...) blog(LOG_WARNING, "[%s] " message, obs_source_get_name(context->source), ##__VA_ARGS__)

#define SPOUT_SENDER_LIST "spoutsenders"
#define USE_FIRST_AVAILABLE_SENDER "usefirstavailablesender"
#define SPOUT_TICK_SPEED_LIMIT "tickspeedlimit"
#define SPOUT_COMPOSITE_MODE "compositemode"

#define COMPOSITE_MODE_OPAQUE 1
#define COMPOSITE_MODE_ALPHA 2
#define COMPOSITE_MODE_DEFAULT 3
#define COMPOSITE_MODE_PREMULTIPLIED 4

struct spout_source {
	obs_source_t *source;
	char senderName[256];
	bool useFirstSender;
	gs_texture_t *texture;
	HANDLE dxHandle;
	DWORD dxFormat;
	ULONGLONG lastCheckTick;
	int width;
	int height;
	bool initialized;
	ULONGLONG tick_speed_limit;
	ULONGLONG composite_mode;
	int spout_status;
	int render_status;
	int tick_status;
	int sender_info_fail_count;
	bool pending_reset;
	SPOUTHANDLE spout_receiver_ptr;
	pthread_mutex_t mutex;
};

/**
 * Writes sender texture details (width & height) to the context
 * @return bool success
 */
static bool win_spout_source_store_sender_info(spout_source *context)
{
	unsigned int width, height;
	// get info about this active sender:
	if (!context->spout_receiver_ptr->GetSenderInfo(context->senderName, width, height, context->dxHandle,
							context->dxFormat)) {
		return false;
	}

	context->width = width;
	context->height = height;
	return true;
}

static void win_spout_source_init(void *data, bool forced = false)
{
	struct spout_source *context = (spout_source *)data;
	if (context->initialized) {
		context->spout_status = 0;
		return;
	}

	ULONG64 tickDelta = GetTickCount64() - context->lastCheckTick;

	if (tickDelta < context->tick_speed_limit && !forced) {
		return;
	}
	context->lastCheckTick = GetTickCount64();

	if (context->spout_receiver_ptr == NULL) {
		if (context->spout_status != -1) {
			warn("Spout pointer didn't exist");
			context->spout_status = -1;
		}
		return;
	}

	int totalSenders = context->spout_receiver_ptr->GetSenderCount();

	if (totalSenders == 0) {
		if (context->spout_status != -2) {
			info("No active Spout cameras");
			context->spout_status = -2;
		}
		return;
	}

	if (context->useFirstSender) {
		if (context->spout_receiver_ptr->GetSender(0, context->senderName)) {
			// Do not call SetActiveSender: that mutates global Spout state and
			// races when the same source is drawn on additional OBS canvases.
		} else {
			if (context->spout_status != -3) {
				info("Strange , there is a sender without name ?");
				context->spout_status = -3;
			}
			return;
		}
	} else {
		int index;
		char senderName[256];
		bool exists = false;
		// then get the name of each sender from SPOUT
		for (index = 0; index < totalSenders; index++) {
			context->spout_receiver_ptr->GetSender(index, senderName);
			if (strcmp(senderName, context->senderName) == 0) {
				exists = true;
				break;
			}
		}
		if (!exists) {
			if (context->spout_status != -5) {
				info("Sorry, Sender Name %s not found", context->senderName);
				context->spout_status = -5;
			}
			return;
		} else {
			context->spout_status = 0;
		}
	}

	debug("Getting info for sender %s", context->senderName);
	if (!win_spout_source_store_sender_info(context)) {
		warn("Named %s sender not found", context->senderName);
	} else {
		debug("Sender %s is of dimensions %d x %d", context->senderName, context->width, context->height);
	};

	obs_enter_graphics();
	gs_texture_t *old_tex = context->texture;
	context->texture = gs_texture_open_shared((uint32_t)(uintptr_t)context->dxHandle);
	if (old_tex) {
		gs_texture_destroy(old_tex);
	}
	obs_leave_graphics();

	context->sender_info_fail_count = 0;
	context->initialized = true;
}

static void win_spout_source_deinit(void *data)
{
	struct spout_source *context = (spout_source *)data;
	context->initialized = false;
	gs_texture_t *tex = context->texture;
	context->texture = NULL;
	if (tex) {
		obs_enter_graphics();
		gs_texture_destroy(tex);
		obs_leave_graphics();
	}
}

static void win_spout_source_update(void *data, obs_data_t *settings)
{
	struct spout_source *context = (spout_source *)data;

	auto selectedSender = obs_data_get_string(settings, SPOUT_SENDER_LIST);

	if (strcmp(selectedSender, USE_FIRST_AVAILABLE_SENDER) == 0) {
		context->useFirstSender = true;
	} else {
		context->useFirstSender = false;
		memset(context->senderName, 0, 256);
		strcpy(context->senderName, selectedSender);
	}

	auto selectedSpeed = obs_data_get_int(settings, SPOUT_TICK_SPEED_LIMIT);
	context->tick_speed_limit = selectedSpeed;

	auto compositeMode = obs_data_get_int(settings, SPOUT_COMPOSITE_MODE);
	context->composite_mode = compositeMode;

	if (context->initialized) {
		context->pending_reset = true;
	}
}

static const char *win_spout_source_get_name(void *unused)
{
	UNUSED_PARAMETER(unused);
	return obs_module_text("sourcename");
}

// Create our context struct which will be passed to each
// of the plugin functions as void *data
static void *win_spout_source_create(obs_data_t *settings, obs_source_t *source)
{
	struct spout_source *context = (spout_source *)bzalloc(sizeof(spout_source));
	context->spout_receiver_ptr = GetSpout();
	context->source = source;
	// Name may still be unset during create; keep this at debug to avoid [(null)] info spam (#89).
	debug("initialising spout source");
	context->useFirstSender = true;
	context->initialized = false;
	context->tick_speed_limit = 0;
	context->texture = NULL;
	context->dxHandle = NULL;
	context->sender_info_fail_count = 0;
	context->pending_reset = false;
	pthread_mutex_init_value(&context->mutex);
	pthread_mutex_init(&context->mutex, NULL);

	// set the initial size as 100x100 until we
	// have the actual dimensions from SPOUT
	context->width = context->height = 100;

	win_spout_source_update(context, settings);
	return context;
}

static void win_spout_source_destroy(void *data)
{
	struct spout_source *context = (spout_source *)data;

	win_spout_source_deinit(data);

	if (context->spout_receiver_ptr != NULL) {
		context->spout_receiver_ptr->Release();
		context->spout_receiver_ptr = nullptr;
	}

	pthread_mutex_destroy(&context->mutex);
	bfree(context);
}

static void win_spout_source_defaults(obs_data_t *settings)
{
	obs_data_set_default_string(settings, SPOUT_SENDER_LIST, USE_FIRST_AVAILABLE_SENDER);
	obs_data_set_default_int(settings, "tickspeedlimit", 100);
}

static void win_spout_source_show(void *data)
{
	win_spout_source_init(data, true); // When showing do forced init without delay
}

static void win_spout_source_hide(void *data)
{
	struct spout_source *context = (spout_source *)data;
	// Additional canvases can hide the source on one mix while it is still
	// shown on another. Only tear down GPU resources when nothing is showing.
	if (context->source && obs_source_showing(context->source)) {
		return;
	}
	win_spout_source_deinit(data);
}

static uint32_t win_spout_source_getwidth(void *data)
{
	struct spout_source *context = (spout_source *)data;
	return context->width;
}

static uint32_t win_spout_source_getheight(void *data)
{
	struct spout_source *context = (spout_source *)data;
	return context->height;
}

static void win_spout_source_render(void *data, gs_effect_t *effect)
{
	struct spout_source *context = (spout_source *)data;

	if (context->pending_reset) {
		context->pending_reset = false;
		win_spout_source_deinit(data);
		win_spout_source_init(data, true);
	}

	// Do not force-init every render: with multiple canvases video_render runs
	// once per mix and was re-opening shared textures / spamming the log (#89).
	if (!context->initialized) {
		win_spout_source_init(data, false);
		if (!context->initialized) {
			if (context->render_status != -1) {
				debug("uninit'd");
				context->render_status = -1;
			}
			return;
		}
	}

	pthread_mutex_lock(&context->mutex);
	gs_texture_t *texture = context->texture;
	const ULONGLONG composite_mode = context->composite_mode;
	pthread_mutex_unlock(&context->mutex);

	if (!texture) {
		if (context->render_status != -2) {
			debug("no texture");
			context->render_status = -2;
		}
		return;
	}

	if (context->render_status != 0) {
		info("rendering context->texture");
		context->render_status = 0;
	}

	switch (composite_mode) {
	case COMPOSITE_MODE_OPAQUE:
		effect = obs_get_base_effect(OBS_EFFECT_OPAQUE);
		break;
	case COMPOSITE_MODE_ALPHA:
		effect = obs_get_base_effect(
			OBS_EFFECT_PREMULTIPLIED_ALPHA); // Converts premultiplied to regular alpha before blending it as regular transparency.
		break;
	case COMPOSITE_MODE_PREMULTIPLIED:
		effect = obs_get_base_effect(OBS_EFFECT_DEFAULT);
		// Proper blending of premultiplied alpha needs a modified blend function and then works with the default blending effect.
		gs_blend_state_push();
		gs_blend_function(GS_BLEND_ONE, GS_BLEND_INVSRCALPHA);
		break;
	case COMPOSITE_MODE_DEFAULT:
		effect = obs_get_base_effect(OBS_EFFECT_DEFAULT);
		break;
	default:
		effect = obs_get_base_effect(OBS_EFFECT_OPAQUE);
		break;
	}

	while (gs_effect_loop(effect, "Draw")) {
		obs_source_draw(texture, 0, 0, 0, 0, false);
	}

	if (composite_mode == COMPOSITE_MODE_PREMULTIPLIED) {
		gs_blend_state_pop();
	}
}

static bool win_spout_sender_exists(spout_source *context)
{
	if (!context->spout_receiver_ptr) {
		return false;
	}
	int totalSenders = context->spout_receiver_ptr->GetSenderCount();
	char senderName[256];
	for (int index = 0; index < totalSenders; index++) {
		if (!context->spout_receiver_ptr->GetSender(index, senderName)) {
			continue;
		}
		if (strcmp(senderName, context->senderName) == 0) {
			return true;
		}
	}
	return false;
}

/**
 * Updates sender texture details on the context
 * and works out whether any of this data has changed
 *
 * @return bool sender data has changed
 */
static bool win_spout_sender_has_changed(spout_source *context)
{
	if (!win_spout_sender_exists(context)) {
		if (!context->initialized) {
			return false;
		}
		// Extra canvases can briefly report no senders while the shared
		// texture is still valid. Require several misses before reset (#89).
		context->sender_info_fail_count++;
		if (context->sender_info_fail_count < 8) {
			return false;
		}
		return true;
	}

	DWORD oldFormat = context->dxFormat;
	auto oldWidth = context->width;
	auto oldHeight = context->height;

	if (!win_spout_source_store_sender_info(context)) {
		// GetSenderInfo can fail transiently when extra canvases are rendering
		// the same shared texture. Do not tear down on a single miss.
		context->sender_info_fail_count++;
		if (context->sender_info_fail_count < 8) {
			return false;
		}
		return true;
	}
	context->sender_info_fail_count = 0;
	if (context->width != oldWidth || context->height != oldHeight || oldFormat != context->dxFormat) {
		return true;
	}
	return false;
}

static void win_spout_source_tick(void *data, float seconds)
{
	UNUSED_PARAMETER(seconds);

	struct spout_source *context = (spout_source *)data;

	pthread_mutex_lock(&context->mutex);
	bool changed = win_spout_sender_has_changed(context);
	pthread_mutex_unlock(&context->mutex);

	if (changed) {
		// Demote to debug: with multiple canvases this used to flood the log
		// even when the sender was only briefly unreachable (#89).
		if (context->tick_status != -1) {
			debug("Sender %s has changed / gone away. Resetting", context->senderName);
			context->tick_status = -1;
		}
		context->pending_reset = true;
		return;
	}
	if (!context->initialized) {
		if (context->tick_status != -2) {
			context->tick_status = -2;
		}
		// Poll with tick_speed_limit instead of forcing a reset every frame.
		win_spout_source_init(data, false);
	}
	if (context->initialized && context->tick_status != 0) {
		context->tick_status = 0;
	}
}

static void fill_senders(SPOUTHANDLE spoutptr, obs_property_t *list)
{
	// clear the list first
	obs_property_list_clear(list);

	// first option in the list should be "Take whatever is available"
	obs_property_list_add_string(list, obs_module_text("usefirstavailablesender"), USE_FIRST_AVAILABLE_SENDER);
	int totalSenders = spoutptr->GetSenderCount();
	if (totalSenders == 0) {
		return;
	}
	int index;
	char senderName[256];
	// then get the name of each sender from SPOUT
	for (index = 0; index < totalSenders; index++) {
		spoutptr->GetSender(index, senderName);
		obs_property_list_add_string(list, senderName, senderName);
	}
}

// initialise the gui fields
static obs_properties_t *win_spout_properties(void *data)
{
	struct spout_source *context = (spout_source *)data;

	obs_properties_t *props = obs_properties_create();

	obs_property_t *sender_list = obs_properties_add_list(props, SPOUT_SENDER_LIST, obs_module_text("SpoutSenders"),
							      OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_STRING);

	fill_senders(context->spout_receiver_ptr, sender_list);

	obs_property_t *composite_mode_list = obs_properties_add_list(props, SPOUT_COMPOSITE_MODE,
								      obs_module_text("compositemode"),
								      OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_INT);
	obs_property_list_add_int(composite_mode_list, obs_module_text("compositemodeopaque"), COMPOSITE_MODE_OPAQUE);
	obs_property_list_add_int(composite_mode_list, obs_module_text("compositemodealpha"), COMPOSITE_MODE_ALPHA);
	obs_property_list_add_int(composite_mode_list, obs_module_text("compositemodedefault"), COMPOSITE_MODE_DEFAULT);
	obs_property_list_add_int(composite_mode_list, obs_module_text("compositemodepremultiplied"),
				  COMPOSITE_MODE_PREMULTIPLIED);

	obs_property_t *tick_speed_limit_list = obs_properties_add_list(props, SPOUT_TICK_SPEED_LIMIT,
									obs_module_text("tickspeedlimit"),
									OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_INT);
	obs_property_list_add_int(tick_speed_limit_list, obs_module_text("tickspeedcrazy"), 1);
	obs_property_list_add_int(tick_speed_limit_list, obs_module_text("tickspeedfast"), 100);
	obs_property_list_add_int(tick_speed_limit_list, obs_module_text("tickspeednormal"), 500);
	obs_property_list_add_int(tick_speed_limit_list, obs_module_text("tickspeedslow"), 1000);

	return props;
}

struct obs_source_info create_spout_source_info()
{
	struct obs_source_info spout_source_info = {};
	spout_source_info.id = "spout_capture";
	spout_source_info.type = OBS_SOURCE_TYPE_INPUT;
	spout_source_info.output_flags = OBS_SOURCE_VIDEO | OBS_SOURCE_CUSTOM_DRAW;
	spout_source_info.get_name = win_spout_source_get_name;
	spout_source_info.create = win_spout_source_create;
	spout_source_info.destroy = win_spout_source_destroy;
	spout_source_info.update = win_spout_source_update;
	spout_source_info.get_defaults = win_spout_source_defaults;
	spout_source_info.show = win_spout_source_show;
	spout_source_info.hide = win_spout_source_hide;
	spout_source_info.get_width = win_spout_source_getwidth;
	spout_source_info.get_height = win_spout_source_getheight;

	spout_source_info.video_render = win_spout_source_render;
	spout_source_info.video_tick = win_spout_source_tick;
	spout_source_info.get_properties = win_spout_properties;

	return spout_source_info;
}
