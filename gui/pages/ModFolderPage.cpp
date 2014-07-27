/* Copyright 2014 MultiMC Contributors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "ModFolderPage.h"
#include "ui_ModFolderPage.h"

#include <QFileDialog>
#include <QMessageBox>
#include <QEvent>
#include <QKeyEvent>
#include <QDesktopServices>
#include <QAbstractItemModel>

#include <pathutils.h>

#include "MultiMC.h"
#include "gui/dialogs/CustomMessageBox.h"
#include "gui/dialogs/ModEditDialogCommon.h"
#include "logic/ModList.h"
#include "logic/Mod.h"
#include "logic/VersionFilterData.h"
#include "logic/quickmod/QuickModInstanceModList.h"
#include "logic/tasks/SequentialTask.h"

ModFolderPage::ModFolderPage(BaseInstance *inst, std::shared_ptr<ModList> mods, QString id,
							 QString iconName, QString displayName, QString helpPage,
							 QWidget *parent)
	: QWidget(parent), ui(new Ui::ModFolderPage)
{
	ui->setupUi(this);
	m_inst = inst;
	m_mods = mods;
	m_id = id;
	m_displayName = displayName;
	m_iconName = iconName;
	m_helpName = helpPage;
	ui->modTreeView->setModel(m_mods.get());
	ui->modTreeView->installEventFilter(this);
	m_mods->startWatching();
	auto smodel = ui->modTreeView->selectionModel();
	connect(smodel, SIGNAL(currentChanged(QModelIndex, QModelIndex)),
			SLOT(modCurrent(QModelIndex, QModelIndex)));
}

ModFolderPage::~ModFolderPage()
{
	m_mods->stopWatching();
	delete ui;
}

bool ModFolderPage::shouldDisplay() const
{
	if (m_inst)
		return !m_inst->isRunning();
	return true;
}

bool ModFolderPage::modListFilter(QKeyEvent *keyEvent)
{
	switch (keyEvent->key())
	{
	case Qt::Key_Delete:
		on_rmModBtn_clicked();
		return true;
	case Qt::Key_Plus:
		on_addModBtn_clicked();
		return true;
	default:
		break;
	}
	return QWidget::eventFilter(ui->modTreeView, keyEvent);
}

bool ModFolderPage::eventFilter(QObject *obj, QEvent *ev)
{
	if (ev->type() != QEvent::KeyPress)
	{
		return QWidget::eventFilter(obj, ev);
	}
	QKeyEvent *keyEvent = static_cast<QKeyEvent *>(ev);
	if (obj == ui->modTreeView)
		return modListFilter(keyEvent);
	return QWidget::eventFilter(obj, ev);
}

void ModFolderPage::on_addModBtn_clicked()
{
	QStringList fileNames = QFileDialog::getOpenFileNames(
		this, QApplication::translate("ModFolderPage", "Select Loader Mods"));
	for (auto filename : fileNames)
	{
		m_mods->stopWatching();
		m_mods->installMod(QFileInfo(filename));
		m_mods->startWatching();
	}
}
void ModFolderPage::on_rmModBtn_clicked()
{
	int first, last;
	auto list = ui->modTreeView->selectionModel()->selectedRows();

	if (!lastfirst(list, first, last))
		return;
	m_mods->stopWatching();
	m_mods->deleteMods(first, last);
	m_mods->startWatching();
}

void ModFolderPage::on_viewModBtn_clicked()
{
	openDirInDefaultProgram(m_mods->dir().absolutePath(), true);
}

void ModFolderPage::modCurrent(const QModelIndex &current, const QModelIndex &previous)
{
	if (!current.isValid())
	{
		ui->frame->clear();
		return;
	}
	int row = current.row();
	Mod &m = m_mods->operator[](row);
	ui->frame->updateWithMod(m);
}

CoreModFolderPage::CoreModFolderPage(BaseInstance *inst, std::shared_ptr<ModList> mods,
									 QString id, QString iconName, QString displayName,
									 QString helpPage, QWidget *parent)
	: ModFolderPage(inst, mods, id, iconName, displayName, helpPage, parent)
{
}
bool CoreModFolderPage::shouldDisplay() const
{
	if (ModFolderPage::shouldDisplay())
	{
		auto inst = dynamic_cast<OneSixInstance *>(m_inst);
		if (!inst)
			return true;
		auto version = inst->getFullVersion();
		if (!version)
			return true;
		if (version->m_releaseTime < g_VersionFilterData.legacyCutoffDate)
		{
			return true;
		}
	}
	return false;
}

LoaderModFolderPage::LoaderModFolderPage(BaseInstance *inst, std::shared_ptr<ModList> mods, QString id, QString iconName, QString displayName, QString helpPage, QWidget *parent)
	: ModFolderPage(inst, mods, id, iconName, displayName, helpPage, parent)
{
	m_modsModel = new QuickModInstanceModList(m_inst, m_mods, this);
	ui->modTreeView->setModel(m_proxy = new QuickModInstanceModListProxy(m_modsModel, this));
}

QModelIndex LoaderModFolderPage::mapToModsList(const QModelIndex &view) const
{
	return m_modsModel->mapToModList(m_proxy->mapToSource(view));
}
void LoaderModFolderPage::sortMods(const QModelIndexList &view, QModelIndexList *quickmods, QModelIndexList *mods)
{
	for (auto index : view)
	{
		if (!index.isValid())
		{
			continue;
		}
		const QModelIndex mappedOne = m_proxy->mapToSource(index);
		if (m_modsModel->isModListArea(mappedOne))
		{
			mods->append(m_modsModel->mapToModList(mappedOne));
		}
		else
		{
			quickmods->append(mappedOne);
		}
	}
}

void LoaderModFolderPage::modCurrent(const QModelIndex &current, const QModelIndex &previous)
{
	ModFolderPage::modCurrent(mapToModsList(current), mapToModsList(previous));
}

void LoaderModFolderPage::on_rmModBtn_clicked()
 {
	int first, last;
	auto tmp = ui->modTreeView->selectionModel()->selectedRows();
	QModelIndexList quickmods, mods;
	sortMods(tmp, &quickmods, &mods);

	// mods
	{
		if (lastfirst(mods, first, last))
		{
			m_mods->stopWatching();
			m_mods->deleteMods(first, last);
			m_mods->startWatching();
		}
	}
	// quickmods
	{
		try
		{
			for (auto quickmod : quickmods)
			{
				m_modsModel->removeMod(quickmod);
			}
		}
		catch (MMCError &error)
		{
			QMessageBox::critical(this, tr("Error"), tr("Unable to remove mod:\n%1").arg(error.cause()));
		}
	}
}
void LoaderModFolderPage::on_updateModBtn_clicked()
{
	auto tmp = ui->modTreeView->selectionModel()->selectedRows();
	QModelIndexList quickmods, mods;
	sortMods(tmp, &quickmods, &mods);
	try
	{
		for (auto quickmod : quickmods)
		{
			m_modsModel->updateMod(quickmod);
		}
	}
	catch (MMCError &error)
	{
		QMessageBox::critical(this, tr("Error"), tr("Unable to update mod:\n%1").arg(error.cause()));
	}
}
