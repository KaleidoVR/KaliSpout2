/**
 * KaliSpout2 — Copyright (C) 2024-2026 KaleidoVR
 * Based on the OBS Spout2 plugin
 * Copyright Off World Live Ltd (https://offworld.live), 2019-2021
 *
 * Licensed under the GNU General Public License v2
 * https://www.gnu.org/licenses/old-licenses/gpl-2.0.en.html
 */

#ifndef WINSPOUTOUTSETTINGS_H
#define WINSPOUTOUTSETTINGS_H

#include <QDialog>
#include <QShowEvent>
#include "ui_win-spout-output-settings.h"

class win_spout_output_settings : public QDialog {
	Q_OBJECT

public:
	explicit win_spout_output_settings(QWidget *parent = 0);
	~win_spout_output_settings();
	void refresh_canvases();
	void set_started_button_state(bool started);

private Q_SLOTS:
	void on_start();
	void on_stop();
	void on_settings_changed();
	void on_canvas_changed();

private:
	Ui::win_spout_output_settings *ui;
	void save_settings();
	void populate_canvas_combo(const QString &uuid, const QString &name);
	void current_canvas(QString &uuid, QString &name) const;
	bool start_current();
	void showEvent(QShowEvent *event) override;
};

#endif // WINSPOUTOUTSETTINGS_H
