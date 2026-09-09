/**
 * KaliSpout2 — Copyright (C) 2024-2026 KaleidoVR
 * Based on the OBS Spout2 plugin
 * Copyright Off World Live Ltd (https://offworld.live), 2019-2021
 *
 * Licensed under the GNU General Public License v2
 * https://www.gnu.org/licenses/old-licenses/gpl-2.0.en.html
 */

#ifndef WINSPOUT_H
#define WINSPOUT_H

#include <string>
#include <vector>

#define blog(log_level, message, ...) blog(log_level, "[win_spout] " message, ##__VA_ARGS__)

struct SpoutCanvasInfo {
	std::string uuid;
	std::string name;
	bool is_main;
};

struct obs_canvas;
typedef struct obs_canvas obs_canvas_t;

std::vector<SpoutCanvasInfo> spout_get_canvas_list();
obs_canvas_t *spout_find_canvas(const char *uuid, const char *name);

bool spout_output_start(const char *canvasUuid, const char *canvasName, const char *SpoutName);
void spout_output_stop(const char *canvasUuid, const char *canvasName);
bool spout_output_is_active(const char *canvasUuid, const char *canvasName);
void spout_output_stop_all();

// Legacy single-output helpers (main canvas)
void spout_output_start(const char *SpoutName);
void spout_output_stop();

void spout_schedule_autostart();

#endif // WINSPOUT_H
