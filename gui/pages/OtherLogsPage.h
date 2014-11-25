/* Copyright 2013-2014 MultiMC Contributors
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

#pragma once

#include <QWidget>

#include "BasePage.h"

namespace Ui
{
class OtherLogsPage;
}

class RecursiveFileSystemWatcher;

class BaseInstance;

class OtherLogsPage : public QWidget, public BasePage
{
	Q_OBJECT

public:
	explicit OtherLogsPage(BaseInstance *instance, QWidget *parent = 0);
	~OtherLogsPage();

	QString id() const override
	{
		return "logs";
	}
	QString displayName() const override
	{
		return tr("Other logs");
	}
	QIcon icon() const override
	{
		return QIcon::fromTheme("log");
	}
	QString helpPage() const override
	{
		return "Minecraft-Logs";
	}
	void opened() override;
	void closed() override;

private slots:
	void populateSelectLogBox();
	void on_selectLogBox_currentIndexChanged(const int index);
	void on_btnReload_clicked();
	void on_btnPaste_clicked();
	void on_btnCopy_clicked();
	void on_btnDelete_clicked();

private:
	Ui::OtherLogsPage *ui;
	BaseInstance *m_instance;
	RecursiveFileSystemWatcher *m_watcher;
	QString m_currentFile;

	void setControlsEnabled(const bool enabled);
};
