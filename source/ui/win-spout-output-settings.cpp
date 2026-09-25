/**
 * KaliSpout2 — Copyright (C) 2024-2026 KaleidoVR
 * Based on the OBS Spout2 plugin
 * Copyright Off World Live Ltd (https://offworld.live), 2019-2021
 *
 * Licensed under the GNU General Public License v2
 * https://www.gnu.org/licenses/old-licenses/gpl-2.0.en.html
 */

#include "win-spout-output-settings.h"
#include "ui_win-spout-output-settings.h"
#include <obs-frontend-api.h>
#include "../win-spout-config.h"
#include "../win-spout.h"

#include <QByteArray>
#include <QCheckBox>
#include <QComboBox>
#include <QLineEdit>
#include <QPushButton>
#include <QShowEvent>

extern win_spout_output_settings *spout_output_settings;

win_spout_output_settings::win_spout_output_settings(QWidget *parent)
	: QDialog(parent),
	  ui(new Ui::win_spout_output_settings)
{
	this->setAttribute(Qt::WA_DeleteOnClose);
	ui->setupUi(this);

	ui->checkBox_auto->setText(obs_module_text("autostart"));
	ui->checkBox_auto->setToolTip(obs_module_text("autostarttip"));
	ui->label_canvas->setText(obs_module_text("canvaslabel"));
	ui->comboBox_canvas->setToolTip(obs_module_text("canvastip"));
	ui->label_spoutname->setText(obs_module_text("spoutoutputname"));
	ui->pushButton_start->setText(obs_module_text("start"));
	ui->pushButton_stop->setText(obs_module_text("stop"));

	win_spout_config *config = win_spout_config::get();
	ui->checkBox_auto->setChecked(config->auto_start);
	ui->checkBox_continuous->setChecked(config->continuous_broadcast);
	ui->lineEdit_spoutname->setText(config->spout_output_name);
	populate_canvas_combo(config->canvas_uuid, config->canvas_name);

	connect(ui->pushButton_start, &QPushButton::clicked, this, &win_spout_output_settings::on_start);
	connect(ui->pushButton_stop, &QPushButton::clicked, this, &win_spout_output_settings::on_stop);
	connect(ui->checkBox_auto, &QCheckBox::toggled, this,
		&win_spout_output_settings::on_settings_changed);
	connect(ui->checkBox_continuous, &QCheckBox::toggled, this,
		&win_spout_output_settings::on_settings_changed);
	connect(ui->lineEdit_spoutname, &QLineEdit::textChanged, this,
		&win_spout_output_settings::on_settings_changed);
	connect(ui->comboBox_canvas, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
		&win_spout_output_settings::on_canvas_changed);

	set_started_button_state(!spout_output_is_any_active());
}

void win_spout_output_settings::showEvent(QShowEvent *event)
{
	QDialog::showEvent(event);
	refresh_canvases();
}

void win_spout_output_settings::populate_canvas_combo(const QString &uuid, const QString &name)
{
	ui->comboBox_canvas->blockSignals(true);
	ui->comboBox_canvas->clear();

	const auto canvases = spout_get_canvas_list();
	int select = 0;
	int index = 0;
	for (const auto &info : canvases) {
		QString label = QString::fromStdString(info.name);
		if (label.isEmpty()) {
			label = obs_module_text("maincanvas");
		}
		if (info.is_main && label.compare("Main", Qt::CaseInsensitive) != 0) {
			label = QString("%1 (%2)").arg(label, obs_module_text("maincanvas"));
		}
		ui->comboBox_canvas->addItem(label, QString::fromStdString(info.uuid));
		ui->comboBox_canvas->setItemData(index, QString::fromStdString(info.name), Qt::UserRole + 1);
		if ((!uuid.isEmpty() && uuid == QString::fromStdString(info.uuid)) ||
		    (uuid.isEmpty() && !name.isEmpty() && name == QString::fromStdString(info.name)) ||
		    (uuid.isEmpty() && name.isEmpty() && info.is_main)) {
			select = index;
		}
		index++;
	}
	if (ui->comboBox_canvas->count() == 0) {
		ui->comboBox_canvas->addItem(obs_module_text("maincanvas"), QString());
		ui->comboBox_canvas->setItemData(0, QString(), Qt::UserRole + 1);
	}
	ui->comboBox_canvas->setCurrentIndex(select);
	ui->comboBox_canvas->blockSignals(false);
}

void win_spout_output_settings::current_canvas(QString &uuid, QString &name) const
{
	const int index = ui->comboBox_canvas->currentIndex();
	if (index < 0) {
		uuid.clear();
		name.clear();
		return;
	}
	uuid = ui->comboBox_canvas->itemData(index).toString();
	name = ui->comboBox_canvas->itemData(index, Qt::UserRole + 1).toString();
}

void win_spout_output_settings::refresh_canvases()
{
	QString uuid;
	QString name;
	current_canvas(uuid, name);
	if (uuid.isEmpty() && name.isEmpty()) {
		win_spout_config *config = win_spout_config::get();
		uuid = config->canvas_uuid;
		name = config->canvas_name;
	}
	populate_canvas_combo(uuid, name);
	set_started_button_state(!spout_output_is_any_active());
}

void win_spout_output_settings::save_settings()
{
	win_spout_config *config = win_spout_config::get();
	config->auto_start = ui->checkBox_auto->isChecked();
	config->continuous_broadcast = ui->checkBox_continuous->isChecked();
	config->spout_output_name = ui->lineEdit_spoutname->text();
	current_canvas(config->canvas_uuid, config->canvas_name);
	config->save();
}

win_spout_output_settings::~win_spout_output_settings()
{
	save_settings();
	if (spout_output_settings == this) {
		spout_output_settings = nullptr;
	}
	delete ui;
}

void win_spout_output_settings::on_settings_changed()
{
	save_settings();
}

void win_spout_output_settings::on_canvas_changed()
{
	const bool running = spout_output_is_any_active();
	save_settings();
	if (running) {
		spout_output_stop_all();
		start_current();
		return;
	}
	set_started_button_state(true);
}

bool win_spout_output_settings::start_current()
{
	QString uuid;
	QString name;
	current_canvas(uuid, name);
	const QByteArray uuid_bytes = uuid.toUtf8();
	const QByteArray name_bytes = name.toUtf8();
	const QByteArray sender = ui->lineEdit_spoutname->text().toUtf8();
	const bool started =
		spout_output_start(uuid_bytes.constData(), name_bytes.constData(), sender.constData());
	set_started_button_state(!started);
	return started;
}

void win_spout_output_settings::on_start()
{
	save_settings();
	start_current();
}

void win_spout_output_settings::on_stop()
{
	spout_output_stop_all();
	set_started_button_state(true);
}

void win_spout_output_settings::set_started_button_state(bool started)
{
	ui->pushButton_start->setEnabled(started);
	ui->pushButton_stop->setEnabled(!started);
}
