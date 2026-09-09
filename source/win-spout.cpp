/**
 * Copyright Off World Live Ltd (https://offworld.live), 2019-2021
 *
 * and licenced under the GPL v2 (https://www.gnu.org/licenses/old-licenses/gpl-2.0.en.html)
 *
 * Many thanks to authors of https://github.com/baffler/OBS-OpenVR-Input-Plugin which
 * was used as guidance to working with the OBS Studio APIs
 */

#include <obs-module.h>
#include <obs-frontend-api.h>
#include <QAction>
#include <QMainWindow>
#include <QMenu>
#include <QMenuBar>
#include <QMetaObject>
#include <QTimer>
#include <cstdio>
#include <map>
#include <mutex>
#include <string>

#include "win-spout.h"
#include "ui/win-spout-output-settings.h"
#include "win-spout-config.h"

OBS_DECLARE_MODULE()
OBS_MODULE_AUTHOR("Off World Live / KaleidoVR")
OBS_MODULE_USE_DEFAULT_LOCALE("win-spout", "en-US")

extern struct obs_source_info create_spout_source_info();
struct obs_source_info spout_source_info;

extern struct obs_output_info create_spout_output_info();
struct obs_output_info spout_output_info;

extern struct obs_source_info create_spout_filter_info();
struct obs_source_info spout_filter_info;

win_spout_output_settings *spout_output_settings;

struct SpoutActiveOutput {
	obs_output_t *output = nullptr;
	std::string uuid;
	std::string name;
};

static std::mutex outputs_mutex;
static std::map<std::string, SpoutActiveOutput> active_outputs;
static bool obs_finished_loading = false;
static int autostart_retry_count = 0;
static QAction *spout_menu_action = nullptr;

static const char *MAIN_CANVAS_KEY = "__obs_main_canvas__";
static const char *KALEIDOVR_MENU_TITLE = "KaleidoVR";
static const int AUTOSTART_MAX_RETRIES = 20;
static const int AUTOSTART_RETRY_MS = 500;

static bool canvas_name_is_main(const char *name)
{
	if (!name || !*name) {
		return true;
	}
	return strcmp(name, "Main") == 0 || strcmp(name, "main") == 0;
}

static std::string output_key(const char *uuid, const char *name)
{
	if (uuid && *uuid) {
		return std::string("uuid:") + uuid;
	}
	if (name && *name && !canvas_name_is_main(name)) {
		return std::string("name:") + name;
	}
	return MAIN_CANVAS_KEY;
}

static bool enum_canvas_names(void *data, obs_canvas_t *canvas)
{
	auto *list = static_cast<std::vector<SpoutCanvasInfo> *>(data);
	if (!canvas) {
		return true;
	}

	SpoutCanvasInfo info;
	const char *uuid = obs_canvas_get_uuid(canvas);
	const char *name = obs_canvas_get_name(canvas);
	info.uuid = uuid ? uuid : "";
	info.name = name ? name : "";
	info.is_main = (obs_canvas_get_flags(canvas) & MAIN) != 0;
	if (info.name.empty() && info.is_main) {
		info.name = "Main";
	}

	for (const auto &existing : *list) {
		if (!info.uuid.empty() && existing.uuid == info.uuid) {
			return true;
		}
	}
	list->push_back(info);
	return true;
}

std::vector<SpoutCanvasInfo> spout_get_canvas_list()
{
	std::vector<SpoutCanvasInfo> list;

	struct obs_frontend_canvas_list frontend_list = {0};
	obs_frontend_get_canvases(&frontend_list);
	for (size_t i = 0; i < frontend_list.canvases.num; i++) {
		enum_canvas_names(&list, frontend_list.canvases.array[i]);
	}
	obs_frontend_canvas_list_free(&frontend_list);

	obs_enum_canvases(enum_canvas_names, &list);

	obs_canvas_t *main = obs_get_main_canvas();
	if (main) {
		enum_canvas_names(&list, main);
		obs_canvas_release(main);
	}

	if (list.empty()) {
		SpoutCanvasInfo info;
		info.uuid = "";
		info.name = "Main";
		info.is_main = true;
		list.push_back(info);
	}

	return list;
}

obs_canvas_t *spout_find_canvas(const char *uuid, const char *name)
{
	if (uuid && *uuid) {
		obs_canvas_t *canvas = obs_get_canvas_by_uuid(uuid);
		if (canvas) {
			return canvas;
		}
	}

	if (name && *name && !canvas_name_is_main(name)) {
		obs_canvas_t *canvas = obs_get_canvas_by_name(name);
		if (canvas) {
			return canvas;
		}
		return nullptr;
	}

	return obs_get_main_canvas();
}

static video_t *video_from_canvas(obs_canvas_t *canvas)
{
	if (!canvas) {
		return nullptr;
	}
	if (!obs_canvas_has_video(canvas)) {
		return nullptr;
	}
	return obs_canvas_get_video(canvas);
}

static void bind_output_to_canvas(obs_output_t *output, obs_canvas_t *canvas)
{
	video_t *video = video_from_canvas(canvas);

	// Never fall back to another canvas's mix or to obs_get_video() for non-main
	// canvases. Binding the wrong mix (or converting the global mix to BGRA) can
	// black out Aitum Vertical / Stream Suite canvases.
	if (!video && canvas && (obs_canvas_get_flags(canvas) & MAIN)) {
		video = obs_get_video();
	}

	obs_output_set_media(output, video, obs_get_audio());
}

bool spout_output_start(const char *canvasUuid, const char *canvasName, const char *SpoutName)
{
	if (!SpoutName || !*SpoutName) {
		blog(LOG_ERROR, "Cannot start Spout output with empty sender name");
		return false;
	}

	const std::string key = output_key(canvasUuid, canvasName);

	obs_canvas_t *canvas = spout_find_canvas(canvasUuid, canvasName);
	if (!canvas) {
		blog(LOG_ERROR, "Cannot start Spout output: canvas not found (uuid=%s name=%s)",
		     canvasUuid ? canvasUuid : "", canvasName ? canvasName : "");
		return false;
	}

	obs_output_t *output = nullptr;
	{
		std::lock_guard<std::mutex> lock(outputs_mutex);
		auto it = active_outputs.find(key);
		if (it != active_outputs.end()) {
			output = it->second.output;
		}
	}

	if (!output) {
		char output_name[256];
		snprintf(output_name, sizeof(output_name), "Spout Output (%s)",
			 canvasName && *canvasName ? canvasName : "Main");
		obs_data_t *settings = obs_data_create();
		obs_data_set_string(settings, "senderName", SpoutName);
		output = obs_output_create("spout_output", output_name, settings, nullptr);
		obs_data_release(settings);
		if (!output) {
			obs_canvas_release(canvas);
			blog(LOG_ERROR, "Failed to create Spout output for canvas '%s'",
			     canvasName ? canvasName : "Main");
			return false;
		}

		std::lock_guard<std::mutex> lock(outputs_mutex);
		SpoutActiveOutput entry;
		entry.output = output;
		entry.uuid = canvasUuid ? canvasUuid : "";
		entry.name = canvasName ? canvasName : "";
		active_outputs[key] = entry;
	}

	obs_data_t *settings = obs_output_get_settings(output);
	obs_data_set_string(settings, "senderName", SpoutName);
	obs_output_update(output, settings);
	obs_data_release(settings);

	bind_output_to_canvas(output, canvas);
	obs_canvas_release(canvas);

	if (!obs_output_video(output)) {
		blog(LOG_WARNING,
		     "Canvas '%s' has no video mix yet; Spout start deferred (refusing to bind another mix)",
		     canvasName && *canvasName ? canvasName : "Main");
		return false;
	}

	if (obs_output_active(output)) {
		return true;
	}

	const bool started = obs_output_start(output);
	if (!started) {
		blog(LOG_ERROR, "Failed to start Spout output for canvas '%s' sender '%s'",
		     canvasName && *canvasName ? canvasName : "Main", SpoutName);
	} else {
		blog(LOG_INFO, "Started Spout output for canvas '%s' as sender '%s'",
		     canvasName && *canvasName ? canvasName : "Main", SpoutName);
	}
	return started;
}

void spout_output_stop(const char *canvasUuid, const char *canvasName)
{
	const std::string key = output_key(canvasUuid, canvasName);
	obs_output_t *output = nullptr;
	{
		std::lock_guard<std::mutex> lock(outputs_mutex);
		auto it = active_outputs.find(key);
		if (it == active_outputs.end()) {
			return;
		}
		output = it->second.output;
		active_outputs.erase(it);
	}

	if (output) {
		obs_output_stop(output);
		obs_output_release(output);
	}
}

bool spout_output_is_active(const char *canvasUuid, const char *canvasName)
{
	const std::string key = output_key(canvasUuid, canvasName);
	std::lock_guard<std::mutex> lock(outputs_mutex);
	auto it = active_outputs.find(key);
	if (it == active_outputs.end() || !it->second.output) {
		return false;
	}
	return obs_output_active(it->second.output);
}

void spout_output_stop_all()
{
	std::map<std::string, SpoutActiveOutput> snapshot;
	{
		std::lock_guard<std::mutex> lock(outputs_mutex);
		snapshot.swap(active_outputs);
	}
	for (auto &[key, entry] : snapshot) {
		UNUSED_PARAMETER(key);
		if (entry.output) {
			obs_output_stop(entry.output);
			obs_output_release(entry.output);
		}
	}
}

void spout_output_start(const char *SpoutName)
{
	spout_output_start(nullptr, nullptr, SpoutName);
}

void spout_output_stop()
{
	spout_output_stop(nullptr, nullptr);
}

void spout_schedule_autostart()
{
	if (!obs_finished_loading) {
		return;
	}

	QMainWindow *main_window = (QMainWindow *)obs_frontend_get_main_window();
	if (!main_window) {
		return;
	}

	// Queue onto the UI thread after FINISHED_LOADING so Aitum / other canvas
	// plugins can call obs_canvas_reset_video first. Starting a raw output
	// during module load marks video active and leaves extra canvases black.
	QMetaObject::invokeMethod(
		main_window,
		[]() {
			win_spout_config *config = win_spout_config::get();
			bool pending = false;
			for (const auto &conf : config->outputs) {
				if (!conf.autoStart || conf.spoutName.isEmpty()) {
					continue;
				}
				if (spout_output_is_active(conf.canvasUuid.toUtf8().constData(),
							   conf.canvasName.toUtf8().constData())) {
					continue;
				}
				if (!spout_output_start(conf.canvasUuid.toUtf8().constData(),
							conf.canvasName.toUtf8().constData(),
							conf.spoutName.toUtf8().constData())) {
					pending = true;
				}
			}

			if (pending && autostart_retry_count < AUTOSTART_MAX_RETRIES) {
				autostart_retry_count++;
				QMainWindow *window = (QMainWindow *)obs_frontend_get_main_window();
				if (window) {
					QTimer::singleShot(AUTOSTART_RETRY_MS, window, []() { spout_schedule_autostart(); });
				}
			}
		},
		Qt::QueuedConnection);
}

static QString spout_strip_mnemonics(QString title)
{
	return title.remove('&');
}

static QMenu *spout_find_kaleidovr_menu(QMainWindow *main_window, bool create_if_missing)
{
	if (!main_window || !main_window->menuBar()) {
		return nullptr;
	}

	QMenuBar *bar = main_window->menuBar();
	QMenu *empty_match = nullptr;
	QMenu *populated_match = nullptr;

	for (QAction *action : bar->actions()) {
		QMenu *menu = action->menu();
		if (!menu) {
			continue;
		}
		if (spout_strip_mnemonics(menu->title()).compare(QString::fromUtf8(KALEIDOVR_MENU_TITLE),
								 Qt::CaseInsensitive) != 0) {
			continue;
		}
		if (!menu->actions().isEmpty()) {
			populated_match = menu;
		} else if (!empty_match) {
			empty_match = menu;
		}
	}

	if (populated_match) {
		return populated_match;
	}
	if (empty_match) {
		return empty_match;
	}
	if (create_if_missing) {
		return bar->addMenu(QString::fromUtf8(KALEIDOVR_MENU_TITLE));
	}
	return nullptr;
}

static void spout_open_settings_dialog()
{
	if (!spout_output_settings) {
		QMainWindow *main_window = (QMainWindow *)obs_frontend_get_main_window();
		if (!main_window) {
			blog(LOG_ERROR, "Can't get main window!");
			return;
		}
		obs_frontend_push_ui_translation(obs_module_get_string);
		spout_output_settings = new win_spout_output_settings(main_window);
		obs_frontend_pop_ui_translation();
		spout_output_settings->show();
	} else {
		spout_output_settings->show();
		spout_output_settings->raise();
		spout_output_settings->activateWindow();
	}
}

static void spout_ensure_kaleidovr_menu_action()
{
	QMainWindow *main_window = (QMainWindow *)obs_frontend_get_main_window();
	if (!main_window) {
		return;
	}

	QMenu *menu = spout_find_kaleidovr_menu(main_window, true);
	if (!menu) {
		return;
	}

	const QString label = QString::fromUtf8(obs_module_text("toolslabel"));
	const QString label_plain = spout_strip_mnemonics(label);

	// Prefer an existing KaleidoVR menu that already has other KaleidoVR items
	// (e.g. App Autostarter). If our action was created earlier under an empty
	// duplicate menu, move it.
	if (spout_menu_action) {
		QWidget *parent_menu = spout_menu_action->parentWidget();
		if (parent_menu != menu) {
			menu->addAction(spout_menu_action);
		}
		return;
	}

	for (QAction *action : menu->actions()) {
		if (spout_strip_mnemonics(action->text()).compare(label_plain, Qt::CaseInsensitive) == 0) {
			spout_menu_action = action;
			QObject::connect(spout_menu_action, &QAction::triggered, spout_open_settings_dialog,
					 Qt::UniqueConnection);
			return;
		}
	}

	spout_menu_action = menu->addAction(label);
	spout_menu_action->setMenuRole(QAction::NoRole);
	QObject::connect(spout_menu_action, &QAction::triggered, spout_open_settings_dialog);
}

static void spout_obs_event(enum obs_frontend_event event, void *)
{
	if (event == OBS_FRONTEND_EVENT_FINISHED_LOADING) {
		obs_finished_loading = true;
		autostart_retry_count = 0;
		spout_ensure_kaleidovr_menu_action();
		spout_schedule_autostart();
		if (spout_output_settings) {
			spout_output_settings->refresh_canvases();
		}
	} else if (event == OBS_FRONTEND_EVENT_CANVAS_ADDED || event == OBS_FRONTEND_EVENT_CANVAS_REMOVED) {
		if (event == OBS_FRONTEND_EVENT_CANVAS_REMOVED) {
			// Drop outputs whose canvas no longer exists.
			std::vector<SpoutCanvasInfo> remaining = spout_get_canvas_list();
			std::map<std::string, SpoutActiveOutput> snapshot;
			{
				std::lock_guard<std::mutex> lock(outputs_mutex);
				snapshot = active_outputs;
			}
			for (const auto &[key, entry] : snapshot) {
				bool found = false;
				for (const auto &info : remaining) {
					if (output_key(info.uuid.c_str(), info.name.c_str()) == key) {
						found = true;
						break;
					}
				}
				if (!found && key != MAIN_CANVAS_KEY) {
					spout_output_stop(entry.uuid.c_str(), entry.name.c_str());
				}
			}
		} else {
			// Aitum / Stream Suite may create canvases after FINISHED_LOADING.
			// Retry Auto-start once the canvas (and its video mix) exists.
			autostart_retry_count = 0;
			spout_schedule_autostart();
		}
		if (spout_output_settings) {
			spout_output_settings->refresh_canvases();
		}
	} else if (event == OBS_FRONTEND_EVENT_EXIT) {
		spout_output_stop_all();
	}
}

bool obs_module_load(void)
{
	spout_source_info = create_spout_source_info();
	obs_register_source(&spout_source_info);

	win_spout_config *config = win_spout_config::get();
	config->load();

	spout_output_info = create_spout_output_info();
	obs_register_output(&spout_output_info);

	// Fork UX: put Spout under the KaleidoVR menu (with App Autostarter), not Tools.
	spout_ensure_kaleidovr_menu_action();

	obs_frontend_add_event_callback(spout_obs_event, nullptr);

	spout_filter_info = create_spout_filter_info();
	obs_register_source(&spout_filter_info);

	blog(LOG_INFO, "win-spout loaded (OBS canvas-aware Spout output, KaleidoVR menu)!");

	return true;
}

void obs_module_unload()
{
	spout_output_stop_all();
	if (spout_output_settings) {
		delete spout_output_settings;
		spout_output_settings = nullptr;
	}
	blog(LOG_INFO, "win-spout unloaded!");
}

const char *obs_module_name()
{
	return "win-spout";
}

const char *obs_module_description()
{
	return "Spout input/output for OBS Studio";
}
