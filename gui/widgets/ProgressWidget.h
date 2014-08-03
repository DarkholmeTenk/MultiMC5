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

#pragma once

#include <QWidget>

class QGridLayout;
class QLabel;
class QProgressBar;
class QPushButton;

class ProgressProvider;

class ProgressWidget : public QWidget
{
	Q_OBJECT

public:
	explicit ProgressWidget(QWidget *parent = 0);
	~ProgressWidget();

	void exec(ProgressProvider *task);
	ProgressProvider *getTask();

	void setSkipButton(bool present, const QString &label = QString());

protected:
	QSize sizeHint() const override;

private
slots:
	void changeStatus(const QString &status);
	void changeProgress(qint64 current, qint64 total);

protected:
	void keyPressEvent(QKeyEvent *e) override;

private:
	QGridLayout *m_gridLayout;
	QLabel *m_statusLabel;
	QProgressBar *m_taskProgressBar;
	QPushButton *m_skipButton;

	void updateSize();

	ProgressProvider *task;
};
