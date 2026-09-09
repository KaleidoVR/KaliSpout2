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
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLineEdit>
#include <QShowEvent>
#include <QSizePolicy>

namespace {

constexpr int kOutputRowHeight = 26;
constexpr int kEditorMinHeight = 22;
constexpr int kAutoStartColumnWidth = 96;
constexpr int kStatusColumnWidth = 88;
constexpr int kCanvasComboMinContents = 10;
constexpr int kCanvasComboMaxWidth = 200;

void style_matched_editors(QComboBox *combo, QLineEdit *nameEdit)
{
	// OBS themes often squash bare QComboBox widgets. Keep a compact fixed
	// height that matches the Spout Sender line edit without extra padding.
	if (combo) {
		combo->setStyleSheet(QStringLiteral("QComboBox { min-height: %1px; }").arg(kEditorMinHeight));
		combo->setMinimumHeight(kEditorMinHeight);
		combo->setMaximumHeight(kEditorMinHeight);
		combo->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
		combo->setMinimumContentsLength(kCanvasComboMinContents);
		combo->setMaximumWidth(kCanvasComboMaxWidth);
		combo->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
	}
	if (nameEdit) {
		nameEdit->setMinimumHeight(kEditorMinHeight);
		nameEdit->setMaximumHeight(kEditorMinHeight);
		nameEdit->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
	}
}

QWidget *wrap_canvas_combo(QComboBox *combo)
{
	auto *container = new QWidget();
	container->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
	auto *layout = new QHBoxLayout(container);
	layout->setContentsMargins(1, 1, 1, 1);
	layout->setSpacing(0);
	layout->addWidget(combo, 0, Qt::AlignLeft | Qt::AlignVCenter);
	layout->addStretch(1);
	return container;
}

QWidget *make_centered_autostart_cell(QCheckBox *autoBox)
{
	auto *container = new QWidget();
	container->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
	auto *layout = new QGridLayout(container);
	layout->setContentsMargins(0, 0, 0, 0);
	layout->setSpacing(0);
	autoBox->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
	layout->addWidget(autoBox, 0, 0, Qt::AlignCenter);
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
	this->setMinimumSize(720, 360);
	this->resize(720, 380);

	auto *header = ui->tableWidget_outputs->horizontalHeader();
	header->setMinimumSectionSize(72);
	header->setSectionResizeMode(0, QHeaderView::Fixed);
	header->setSectionResizeMode(1, QHeaderView::Stretch);
	header->setSectionResizeMode(2, QHeaderView::Fixed);
	header->setSectionResizeMode(3, QHeaderView::Fixed);
	header->setStretchLastSection(false);
	ui->tableWidget_outputs->setColumnWidth(0, kCanvasComboMaxWidth + 16);
	ui->tableWidget_outputs->setColumnWidth(2, kAutoStartColumnWidth);
	ui->tableWidget_outputs->setColumnWidth(3, kStatusColumnWidth);
	ui->tableWidget_outputs->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	ui->tableWidget_outputs->verticalHeader()->setVisible(false);
	ui->tableWidget_outputs->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
	ui->tableWidget_outputs->verticalHeader()->setDefaultSectionSize(kOutputRowHeight);
	ui->tableWidget_outputs->verticalHeader()->setMinimumSectionSize(kOutputRowHeight);
	ui->tableWidget_outputs->setWordWrap(false);
	ui->tableWidget_outputs->setMinimumHeight(100);
	if (auto *canvasHeader = ui->tableWidget_outputs->horizontalHeaderItem(0)) {
		canvasHeader->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
	}
	if (auto *senderHeader = ui->tableWidget_outputs->horizontalHeaderItem(1)) {
		senderHeader->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
	}
	if (auto *autoHeader = ui->tableWidget_outputs->horizontalHeaderItem(2)) {
		autoHeader->setToolTip(QString::fromUtf8(obs_module_text("autostarttip")));
		autoHeader->setTextAlignment(Qt::AlignCenter);
	}
	if (auto *statusHeader = ui->tableWidget_outputs->horizontalHeaderItem(3)) {
		statusHeader->setTextAlignment(Qt::AlignCenter);
	}

	connect(ui->pushButton_start, &QPushButton::clicked, this, &win_spout_output_settings::handle_start_selected);
	connect(ui->pushButton_stop, &QPushButton::clicked, this, &win_spout_output_settings::handle_stop_selected);
	connect(ui->pushButton_start_all, &QPushButton::clicked, this, &win_spout_output_settings::handle_start_all);
	connect(ui->pushButton_stop_all, &QPushButton::clicked, this, &win_spout_output_settings::handle_stop_all);
	connect(ui->pushButton_add, &QPushButton::clicked, this, &win_spout_output_settings::handle_add_output);
	connect(ui->pushButton_remove, &QPushButton::clicked, this, &win_spout_output_settings::handle_remove_output);
	connect(ui->checkBox_continuous, &QCheckBox::toggled, this, &win_spout_output_settings::handle_table_changed);

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
		handle_add_output();
		return;
	}

	for (const auto &conf : config->outputs) {
		const int row = ui->tableWidget_outputs->rowCount();
		ui->tableWidget_outputs->insertRow(row);

		auto *combo = new QComboBox();
		populate_canvas_combo(combo, conf.canvasUuid, conf.canvasName);
		connect(combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
			&win_spout_output_settings::handle_table_changed);

		auto *nameEdit = new QLineEdit(conf.spoutName);
		connect(nameEdit, &QLineEdit::textChanged, this, &win_spout_output_settings::handle_table_changed);
		style_matched_editors(combo, nameEdit);
		ui->tableWidget_outputs->setCellWidget(row, 0, wrap_canvas_combo(combo));
		ui->tableWidget_outputs->setCellWidget(row, 1, nameEdit);

		auto *autoBox = new QCheckBox();
		autoBox->setToolTip(QString::fromUtf8(obs_module_text("autostarttip")));
		autoBox->setChecked(conf.autoStart);
		connect(autoBox, &QCheckBox::toggled, this, &win_spout_output_settings::handle_table_changed);
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
	item->setTextAlignment(Qt::AlignCenter);
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

void win_spout_output_settings::handle_table_changed()
{
	save_settings();
	update_all_running_states();
}

void win_spout_output_settings::handle_add_output()
{
	const int row = ui->tableWidget_outputs->rowCount();
	ui->tableWidget_outputs->insertRow(row);

	auto *combo = new QComboBox();
	populate_canvas_combo(combo, QString(), QString());
	connect(combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
		&win_spout_output_settings::handle_table_changed);

	QString sender = "OBS_Spout";
	if (row > 0) {
		sender = QString("OBS_Spout_%1").arg(row + 1);
	}
	auto *nameEdit = new QLineEdit(sender);
	connect(nameEdit, &QLineEdit::textChanged, this, &win_spout_output_settings::handle_table_changed);
	style_matched_editors(combo, nameEdit);
	ui->tableWidget_outputs->setCellWidget(row, 0, wrap_canvas_combo(combo));
	ui->tableWidget_outputs->setCellWidget(row, 1, nameEdit);

	auto *autoBox = new QCheckBox();
	autoBox->setToolTip(QString::fromUtf8(obs_module_text("autostarttip")));
	autoBox->setChecked(false);
	connect(autoBox, &QCheckBox::toggled, this, &win_spout_output_settings::handle_table_changed);
	ui->tableWidget_outputs->setCellWidget(row, 2, make_centered_autostart_cell(autoBox));
	ui->tableWidget_outputs->setRowHeight(row, kOutputRowHeight);

	ui->tableWidget_outputs->setItem(row, 3, new QTableWidgetItem());
	update_row_running_state(row);
	save_settings();
}

void win_spout_output_settings::handle_remove_output()
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

void win_spout_output_settings::handle_start_selected()
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

void win_spout_output_settings::handle_stop_selected()
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

void win_spout_output_settings::handle_start_all()
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

void win_spout_output_settings::handle_stop_all()
{
	spout_output_stop_all();
	update_all_running_states();
}
