/**
 * Copyright Off World Live Ltd (https://offworld.live), 2019-2021
 *
 * and licenced under the GPL v2 (https://www.gnu.org/licenses/old-licenses/gpl-2.0.en.html)
 *
 * Many thanks to authors of https://github.com/baffler/OBS-OpenVR-Input-Plugin which
 * was used as guidance to working with the OBS Studio APIs
 */

#include "win-spout-output-settings.h"
#include "ui_win-spout-output-settings.h"
#include <obs-frontend-api.h>
#include "../win-spout-config.h"
#include "../win-spout.h"

#include <QCheckBox>
#include <QComboBox>
#include <QGridLayout>
#include <QHeaderView>
#include <QLineEdit>
#include <QShowEvent>
#include <QSizePolicy>
#include <QVBoxLayout>

namespace {

constexpr int kOutputRowHeight = 42;
constexpr int kCellEditorHeight = 32;
constexpr int kCellEditorMargin = 4;

QWidget *wrap_cell_editor(QWidget *editor)
{
	auto *container = new QWidget();
	auto *layout = new QVBoxLayout(container);
	layout->setContentsMargins(kCellEditorMargin, kCellEditorMargin, kCellEditorMargin, kCellEditorMargin);
	layout->setSpacing(0);
	editor->setMinimumHeight(kCellEditorHeight);
	editor->setMaximumHeight(kCellEditorHeight);
	editor->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
	layout->addWidget(editor, 0, Qt::AlignVCenter);
	container->setMinimumHeight(kOutputRowHeight);
	return container;
}

void style_canvas_combo(QComboBox *combo)
{
	if (!combo) {
		return;
	}
	// OBS themes often squash bare QComboBox widgets in table cells; force a
	// readable height that matches the Spout Sender line edit.
	combo->setStyleSheet(QStringLiteral("QComboBox {"
					    "  min-height: %1px;"
					    "  max-height: %1px;"
					    "  padding-top: 4px;"
					    "  padding-bottom: 4px;"
					    "  padding-left: 6px;"
					    "  padding-right: 6px;"
					    "}"
					    "QComboBox::drop-down {"
					    "  width: 20px;"
					    "  subcontrol-origin: padding;"
					    "  subcontrol-position: center right;"
					    "}")
				     .arg(kCellEditorHeight));
	combo->setFixedHeight(kCellEditorHeight);
}

void style_sender_edit(QLineEdit *nameEdit)
{
	if (!nameEdit) {
		return;
	}
	nameEdit->setFixedHeight(kCellEditorHeight);
}

QWidget *make_centered_autostart_cell(QCheckBox *autoBox)
{
	auto *container = new QWidget();
	auto *layout = new QGridLayout(container);
	layout->setContentsMargins(0, 0, 0, 0);
	layout->setSpacing(0);
	autoBox->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
	layout->addWidget(autoBox, 0, 0, Qt::AlignCenter);
	container->setMinimumHeight(kOutputRowHeight);
	return container;
}

template<typename T> T *cell_child(QWidget *cell)
{
	if (!cell) {
		return nullptr;
	}
	if (auto *direct = qobject_cast<T *>(cell)) {
		return direct;
	}
	return cell->findChild<T *>();
}

} // namespace

extern win_spout_output_settings *spout_output_settings;

win_spout_output_settings::win_spout_output_settings(QWidget *parent)
	: QDialog(parent),
	  ui(new Ui::win_spout_output_settings)
{
	this->setAttribute(Qt::WA_DeleteOnClose);
	ui->setupUi(this);

	ui->tableWidget_outputs->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
	ui->tableWidget_outputs->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
	ui->tableWidget_outputs->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
	ui->tableWidget_outputs->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
	ui->tableWidget_outputs->verticalHeader()->setVisible(false);
	ui->tableWidget_outputs->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
	ui->tableWidget_outputs->verticalHeader()->setDefaultSectionSize(kOutputRowHeight);
	ui->tableWidget_outputs->verticalHeader()->setMinimumSectionSize(kOutputRowHeight);
	ui->tableWidget_outputs->setWordWrap(false);
	if (auto *autoHeader = ui->tableWidget_outputs->horizontalHeaderItem(2)) {
		autoHeader->setToolTip(QString::fromUtf8(obs_module_text("autostarttip")));
	}

	connect(ui->pushButton_start, &QPushButton::clicked, this, &win_spout_output_settings::on_start_selected);
	connect(ui->pushButton_stop, &QPushButton::clicked, this, &win_spout_output_settings::on_stop_selected);
	connect(ui->pushButton_start_all, &QPushButton::clicked, this, &win_spout_output_settings::on_start_all);
	connect(ui->pushButton_stop_all, &QPushButton::clicked, this, &win_spout_output_settings::on_stop_all);
	connect(ui->pushButton_add, &QPushButton::clicked, this, &win_spout_output_settings::on_add_output);
	connect(ui->pushButton_remove, &QPushButton::clicked, this, &win_spout_output_settings::on_remove_output);
	connect(ui->checkBox_continuous, &QCheckBox::toggled, this, &win_spout_output_settings::on_table_changed);

	win_spout_config *config = win_spout_config::get();
	ui->checkBox_continuous->setChecked(config->continuous_broadcast);

	load_table();
}

void win_spout_output_settings::showEvent(QShowEvent *event)
{
	QDialog::showEvent(event);
	refresh_canvases();
}

void win_spout_output_settings::populate_canvas_combo(QComboBox *combo, const QString &uuid, const QString &name)
{
	combo->clear();
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
		combo->addItem(label, QString::fromStdString(info.uuid));
		combo->setItemData(index, QString::fromStdString(info.name), Qt::UserRole + 1);
		if ((!uuid.isEmpty() && uuid == QString::fromStdString(info.uuid)) ||
		    (uuid.isEmpty() && !name.isEmpty() && name == QString::fromStdString(info.name)) ||
		    (uuid.isEmpty() && name.isEmpty() && info.is_main)) {
			select = index;
		}
		index++;
	}
	if (combo->count() == 0) {
		combo->addItem(obs_module_text("maincanvas"), QString());
		combo->setItemData(0, QString(), Qt::UserRole + 1);
	}
	combo->setCurrentIndex(select);
}

void win_spout_output_settings::load_table()
{
	win_spout_config *config = win_spout_config::get();
	ui->tableWidget_outputs->setRowCount(0);

	if (config->outputs.isEmpty()) {
		on_add_output();
		return;
	}

	for (const auto &conf : config->outputs) {
		const int row = ui->tableWidget_outputs->rowCount();
		ui->tableWidget_outputs->insertRow(row);

		auto *combo = new QComboBox();
		populate_canvas_combo(combo, conf.canvasUuid, conf.canvasName);
		connect(combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
			&win_spout_output_settings::on_table_changed);
		style_canvas_combo(combo);
		ui->tableWidget_outputs->setCellWidget(row, 0, wrap_cell_editor(combo));

		auto *nameEdit = new QLineEdit(conf.spoutName);
		connect(nameEdit, &QLineEdit::textChanged, this, &win_spout_output_settings::on_table_changed);
		style_sender_edit(nameEdit);
		ui->tableWidget_outputs->setCellWidget(row, 1, wrap_cell_editor(nameEdit));

		auto *autoBox = new QCheckBox();
		autoBox->setToolTip(QString::fromUtf8(obs_module_text("autostarttip")));
		autoBox->setChecked(conf.autoStart);
		connect(autoBox, &QCheckBox::toggled, this, &win_spout_output_settings::on_table_changed);
		ui->tableWidget_outputs->setCellWidget(row, 2, make_centered_autostart_cell(autoBox));
		ui->tableWidget_outputs->setRowHeight(row, kOutputRowHeight);

		ui->tableWidget_outputs->setItem(row, 3, new QTableWidgetItem());
		update_row_running_state(row);
	}
}

void win_spout_output_settings::row_canvas(int row, QString &uuid, QString &name)
{
	auto *combo = cell_child<QComboBox>(ui->tableWidget_outputs->cellWidget(row, 0));
	if (!combo) {
		uuid.clear();
		name.clear();
		return;
	}
	uuid = combo->currentData().toString();
	name = combo->currentData(Qt::UserRole + 1).toString();
}

QString win_spout_output_settings::row_sender(int row)
{
	auto *edit = cell_child<QLineEdit>(ui->tableWidget_outputs->cellWidget(row, 1));
	return edit ? edit->text() : QString();
}

bool win_spout_output_settings::row_autostart(int row)
{
	auto *box = cell_child<QCheckBox>(ui->tableWidget_outputs->cellWidget(row, 2));
	return box && box->isChecked();
}

void win_spout_output_settings::update_row_running_state(int row)
{
	QString uuid;
	QString name;
	row_canvas(row, uuid, name);
	const bool active = spout_output_is_active(uuid.toUtf8().constData(), name.toUtf8().constData());
	auto *item = ui->tableWidget_outputs->item(row, 3);
	if (!item) {
		item = new QTableWidgetItem();
		ui->tableWidget_outputs->setItem(row, 3, item);
	}
	item->setText(active ? obs_module_text("statusrunning") : obs_module_text("statusstopped"));
}

void win_spout_output_settings::update_all_running_states()
{
	for (int row = 0; row < ui->tableWidget_outputs->rowCount(); row++) {
		update_row_running_state(row);
	}
}

void win_spout_output_settings::refresh_canvases()
{
	for (int row = 0; row < ui->tableWidget_outputs->rowCount(); row++) {
		QString uuid;
		QString name;
		row_canvas(row, uuid, name);
		auto *combo = cell_child<QComboBox>(ui->tableWidget_outputs->cellWidget(row, 0));
		if (combo) {
			combo->blockSignals(true);
			populate_canvas_combo(combo, uuid, name);
			combo->blockSignals(false);
		}
		update_row_running_state(row);
	}
}

void win_spout_output_settings::save_settings()
{
	win_spout_config *config = win_spout_config::get();
	config->continuous_broadcast = ui->checkBox_continuous->isChecked();
	config->outputs.clear();
	config->auto_start = false;

	for (int row = 0; row < ui->tableWidget_outputs->rowCount(); row++) {
		SpoutOutputConfig conf;
		row_canvas(row, conf.canvasUuid, conf.canvasName);
		conf.spoutName = row_sender(row);
		conf.autoStart = row_autostart(row);
		if (conf.autoStart) {
			config->auto_start = true;
		}
		if (row == 0) {
			config->spout_output_name = conf.spoutName;
		}
		if (!conf.spoutName.isEmpty()) {
			config->outputs.append(conf);
		}
	}
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

void win_spout_output_settings::on_table_changed()
{
	save_settings();
	update_all_running_states();
}

void win_spout_output_settings::on_add_output()
{
	const int row = ui->tableWidget_outputs->rowCount();
	ui->tableWidget_outputs->insertRow(row);

	auto *combo = new QComboBox();
	populate_canvas_combo(combo, QString(), QString());
	connect(combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
		&win_spout_output_settings::on_table_changed);
	style_canvas_combo(combo);
	ui->tableWidget_outputs->setCellWidget(row, 0, wrap_cell_editor(combo));

	QString sender = "OBS_Spout";
	if (row > 0) {
		sender = QString("OBS_Spout_%1").arg(row + 1);
	}
	auto *nameEdit = new QLineEdit(sender);
	connect(nameEdit, &QLineEdit::textChanged, this, &win_spout_output_settings::on_table_changed);
	style_sender_edit(nameEdit);
	ui->tableWidget_outputs->setCellWidget(row, 1, wrap_cell_editor(nameEdit));

	auto *autoBox = new QCheckBox();
	autoBox->setToolTip(QString::fromUtf8(obs_module_text("autostarttip")));
	autoBox->setChecked(false);
	connect(autoBox, &QCheckBox::toggled, this, &win_spout_output_settings::on_table_changed);
	ui->tableWidget_outputs->setCellWidget(row, 2, make_centered_autostart_cell(autoBox));
	ui->tableWidget_outputs->setRowHeight(row, kOutputRowHeight);

	ui->tableWidget_outputs->setItem(row, 3, new QTableWidgetItem());
	update_row_running_state(row);
	save_settings();
}

void win_spout_output_settings::on_remove_output()
{
	const int row = ui->tableWidget_outputs->currentRow();
	if (row < 0) {
		return;
	}

	QString uuid;
	QString name;
	row_canvas(row, uuid, name);
	if (spout_output_is_active(uuid.toUtf8().constData(), name.toUtf8().constData())) {
		spout_output_stop(uuid.toUtf8().constData(), name.toUtf8().constData());
	}
	ui->tableWidget_outputs->removeRow(row);
	save_settings();
}

void win_spout_output_settings::on_start_selected()
{
	const int row = ui->tableWidget_outputs->currentRow();
	if (row < 0) {
		return;
	}
	save_settings();
	QString uuid;
	QString name;
	row_canvas(row, uuid, name);
	const QByteArray sender = row_sender(row).toUtf8();
	spout_output_start(uuid.toUtf8().constData(), name.toUtf8().constData(), sender.constData());
	update_row_running_state(row);
}

void win_spout_output_settings::on_stop_selected()
{
	const int row = ui->tableWidget_outputs->currentRow();
	if (row < 0) {
		return;
	}
	QString uuid;
	QString name;
	row_canvas(row, uuid, name);
	spout_output_stop(uuid.toUtf8().constData(), name.toUtf8().constData());
	update_row_running_state(row);
}

void win_spout_output_settings::on_start_all()
{
	save_settings();
	for (int row = 0; row < ui->tableWidget_outputs->rowCount(); row++) {
		QString uuid;
		QString name;
		row_canvas(row, uuid, name);
		const QByteArray sender = row_sender(row).toUtf8();
		if (!sender.isEmpty()) {
			spout_output_start(uuid.toUtf8().constData(), name.toUtf8().constData(), sender.constData());
		}
	}
	update_all_running_states();
}

void win_spout_output_settings::on_stop_all()
{
	spout_output_stop_all();
	update_all_running_states();
}
