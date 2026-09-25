/**
 * KaliSpout2 — Copyright (C) 2024-2026 KaleidoVR
 * Based on the OBS Spout2 plugin
 * Copyright Off World Live Ltd (https://offworld.live), 2019-2021
 *
 * Licensed under the GNU General Public License v2
 * https://www.gnu.org/licenses/old-licenses/gpl-2.0.en.html
 */

#include "win-spout-config.h"

#include <obs-frontend-api.h>
#include <util/config-file.h>

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#define SECTION_NAME "win_spout"
#define PARAM_AUTO_START "auto_start"
#define PARAM_SPOUT_OUTPUT_NAME "spout_output_name"
#define PARAM_CANVAS_UUID "canvas_uuid"
#define PARAM_CANVAS_NAME "canvas_name"
#define PARAM_CONTINUOUS_BROADCAST "continuous_broadcast"
#define PARAM_OUTPUTS_LIST "outputs_list"
#define PARAM_CONFIG_VERSION "config_version"
#define CURRENT_CONFIG_VERSION 2

win_spout_config *win_spout_config::_instance = nullptr;

win_spout_config::win_spout_config()
	: auto_start(false),
	  spout_output_name("OBS_Spout"),
	  continuous_broadcast(false),
	  config_version(0)
{
	config_t *obs_config = obs_frontend_get_user_config();

	if (obs_config) {
		config_set_default_bool(obs_config, SECTION_NAME, PARAM_AUTO_START, auto_start);
		config_set_default_string(obs_config, SECTION_NAME, PARAM_SPOUT_OUTPUT_NAME,
					  spout_output_name.toUtf8().constData());
		config_set_default_string(obs_config, SECTION_NAME, PARAM_CANVAS_UUID, "");
		config_set_default_string(obs_config, SECTION_NAME, PARAM_CANVAS_NAME, "");
		config_set_default_bool(obs_config, SECTION_NAME, PARAM_CONTINUOUS_BROADCAST, continuous_broadcast);
		config_set_default_string(obs_config, SECTION_NAME, PARAM_OUTPUTS_LIST, "");
		config_set_default_int(obs_config, SECTION_NAME, PARAM_CONFIG_VERSION, 0);
	}
}

void win_spout_config::clear_autostart_flags()
{
	auto_start = false;
}

void win_spout_config::migrate_legacy_output()
{
	if (spout_output_name.isEmpty()) {
		spout_output_name = "OBS_Spout";
	}
}

void win_spout_config::load()
{
	config_t *obs_config = obs_frontend_get_user_config();
	if (obs_config) {
		auto_start = config_get_bool(obs_config, SECTION_NAME, PARAM_AUTO_START);
		spout_output_name = config_get_string(obs_config, SECTION_NAME, PARAM_SPOUT_OUTPUT_NAME);
		canvas_uuid = config_get_string(obs_config, SECTION_NAME, PARAM_CANVAS_UUID);
		canvas_name = config_get_string(obs_config, SECTION_NAME, PARAM_CANVAS_NAME);
		continuous_broadcast = config_get_bool(obs_config, SECTION_NAME, PARAM_CONTINUOUS_BROADCAST);
		config_version = (int)config_get_int(obs_config, SECTION_NAME, PARAM_CONFIG_VERSION);

		const char *json_str = config_get_string(obs_config, SECTION_NAME, PARAM_OUTPUTS_LIST);
		if (json_str && *json_str) {
			QJsonDocument doc = QJsonDocument::fromJson(QByteArray(json_str));
			if (doc.isArray()) {
				const QJsonArray arr = doc.array();
				QJsonObject chosen;
				bool have_chosen = false;
				for (const auto &val : arr) {
					const QJsonObject obj = val.toObject();
					if (obj["spoutName"].toString().isEmpty()) {
						continue;
					}
					if (!have_chosen) {
						chosen = obj;
						have_chosen = true;
					}
					if (obj["autoStart"].toBool()) {
						chosen = obj;
						have_chosen = true;
						break;
					}
				}
				if (have_chosen) {
					if (arr.size() > 1) {
						blog(LOG_INFO,
						     "[win_spout] Multiple canvas outputs in settings; "
						     "keeping a single sender (%s)",
						     chosen["spoutName"].toString().toUtf8().constData());
					}
					canvas_uuid = chosen["canvasUuid"].toString();
					canvas_name = chosen["canvasName"].toString();
					spout_output_name = chosen["spoutName"].toString();
					auto_start = chosen["autoStart"].toBool();
				}
			}
		}

		migrate_legacy_output();

		// One-shot upgrade: clear persisted AutoStart so Tools Spout only launches
		// when the user re-checks Auto-start after this canvas-aware release (#92).
		if (config_version < CURRENT_CONFIG_VERSION) {
			blog(LOG_INFO,
			     "[win_spout] Clearing persisted Auto-start flags during config "
			     "upgrade (v%d -> v%d). Re-enable Auto-start if desired.",
			     config_version, CURRENT_CONFIG_VERSION);
			clear_autostart_flags();
			config_version = CURRENT_CONFIG_VERSION;
			save();
		}
	}
}

void win_spout_config::save()
{
	config_t *obs_config = obs_frontend_get_user_config();
	if (obs_config) {
		config_set_bool(obs_config, SECTION_NAME, PARAM_AUTO_START, auto_start);
		config_set_string(obs_config, SECTION_NAME, PARAM_SPOUT_OUTPUT_NAME,
				  spout_output_name.toUtf8().constData());
		config_set_string(obs_config, SECTION_NAME, PARAM_CANVAS_UUID, canvas_uuid.toUtf8().constData());
		config_set_string(obs_config, SECTION_NAME, PARAM_CANVAS_NAME, canvas_name.toUtf8().constData());
		config_set_bool(obs_config, SECTION_NAME, PARAM_CONTINUOUS_BROADCAST, continuous_broadcast);
		config_set_int(obs_config, SECTION_NAME, PARAM_CONFIG_VERSION, config_version);

		QJsonArray arr;
		if (!spout_output_name.isEmpty()) {
			QJsonObject obj;
			obj["canvasUuid"] = canvas_uuid;
			obj["canvasName"] = canvas_name;
			obj["spoutName"] = spout_output_name;
			obj["autoStart"] = auto_start;
			arr.append(obj);
		}
		QJsonDocument doc(arr);
		config_set_string(obs_config, SECTION_NAME, PARAM_OUTPUTS_LIST,
				  doc.toJson(QJsonDocument::Compact).constData());
		config_save(obs_config);
	}
}

win_spout_config *win_spout_config::get()
{
	if (!_instance) {
		_instance = new win_spout_config();
	}
	return _instance;
}
