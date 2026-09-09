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

private Q_SLOTS:
	void handle_start_selected();
	void handle_stop_selected();
	void handle_start_all();
	void handle_stop_all();
	void handle_add_output();
	void handle_remove_output();
	void handle_table_changed();

private:
	Ui::win_spout_output_settings *ui;
	void save_settings();
	void load_table();
	void populate_canvas_combo(class QComboBox *combo, const QString &uuid, const QString &name);
	void row_canvas(int row, QString &uuid, QString &name);
	QString row_sender(int row);
	bool row_autostart(int row);
	void update_row_running_state(int row);
	void update_all_running_states();
	void showEvent(QShowEvent *event) override;
};

#endif // WINSPOUTOUTSETTINGS_H
