/**
 * Copyright Off World Live Ltd (https://offworld.live), 2019-2021
 *
 * and licenced under the GPL v2 (https://www.gnu.org/licenses/old-licenses/gpl-2.0.en.html)
 *
 * Many thanks to authors of https://github.com/baffler/OBS-OpenVR-Input-Plugin which
 * was used as guidance to working with the OBS Studio APIs
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
		config_set_default_bool(obs_config, SECTION_NAME, PARAM_CONTINUOUS_BROADCAST, continuous_broadcast);
		config_set_default_string(obs_config, SECTION_NAME, PARAM_OUTPUTS_LIST, "");
		config_set_default_int(obs_config, SECTION_NAME, PARAM_CONFIG_VERSION, 0);
	}
}

void win_spout_config::clear_autostart_flags()
{
	auto_start = false;
	for (auto &conf : outputs) {
		conf.autoStart = false;
	}
}

void win_spout_config::migrate_legacy_output()
{
	if (!outputs.isEmpty()) {
		return;
	}

	if (spout_output_name.isEmpty()) {
		return;
	}

	SpoutOutputConfig conf;
	conf.canvasUuid = QString();
	conf.canvasName = QString();
	conf.spoutName = spout_output_name;
	// Never inherit legacy auto_start. Stale true values survive OBS reinstalls when
	// the Spout plugin/user config remain, then surprise-start Tools output (#92).
	conf.autoStart = false;
	outputs.append(conf);
	auto_start = false;
}

void win_spout_config::load()
{
	config_t *obs_config = obs_frontend_get_user_config();
	if (obs_config) {
		auto_start = config_get_bool(obs_config, SECTION_NAME, PARAM_AUTO_START);
		spout_output_name = config_get_string(obs_config, SECTION_NAME, PARAM_SPOUT_OUTPUT_NAME);
		continuous_broadcast = config_get_bool(obs_config, SECTION_NAME, PARAM_CONTINUOUS_BROADCAST);
		config_version = (int)config_get_int(obs_config, SECTION_NAME, PARAM_CONFIG_VERSION);

		outputs.clear();
		const char *json_str = config_get_string(obs_config, SECTION_NAME, PARAM_OUTPUTS_LIST);
		if (json_str && *json_str) {
			QJsonDocument doc = QJsonDocument::fromJson(QByteArray(json_str));
			if (doc.isArray()) {
				const QJsonArray arr = doc.array();
				for (const auto &val : arr) {
					const QJsonObject obj = val.toObject();
					SpoutOutputConfig conf;
					conf.canvasUuid = obj["canvasUuid"].toString();
					conf.canvasName = obj["canvasName"].toString();
					conf.spoutName = obj["spoutName"].toString();
					conf.autoStart = obj["autoStart"].toBool();
					if (!conf.spoutName.isEmpty()) {
						outputs.append(conf);
					}
				}
			}
		}

		migrate_legacy_output();

		// One-shot upgrade: clear persisted AutoStart so Tools Spout only launches
		// when the user re-checks Auto-start after this canvas-aware release (#92).
		if (config_version < CURRENT_CONFIG_VERSION) {
			blog(LOG_INFO,
			     "[win_spout] Clearing persisted Auto-start flags during config "
			     "upgrade (v%d -> v%d). Re-enable Auto-start per canvas if desired.",
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
		config_set_bool(obs_config, SECTION_NAME, PARAM_CONTINUOUS_BROADCAST, continuous_broadcast);
		config_set_int(obs_config, SECTION_NAME, PARAM_CONFIG_VERSION, config_version);

		QJsonArray arr;
		for (const auto &conf : outputs) {
			QJsonObject obj;
			obj["canvasUuid"] = conf.canvasUuid;
			obj["canvasName"] = conf.canvasName;
			obj["spoutName"] = conf.spoutName;
			obj["autoStart"] = conf.autoStart;
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
