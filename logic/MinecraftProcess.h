/* Copyright 2013 MultiMC Contributors
 *
 * Authors: Orochimarufan <orochimarufan.x3@gmail.com>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *	 http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include <QProcess>
#include <QString>
#include "BaseInstance.h"

/**
 * @brief the MessageLevel Enum
 * defines what level a message is
 */
namespace MessageLevel
{
enum Enum
{
	MultiMC, /**< MultiMC Messages */
	Debug,   /**< Debug Messages */
	Info,	/**< Info Messages */
	Message, /**< Standard Messages */
	Warning, /**< Warnings */
	Error,   /**< Errors */
	Fatal	/**< Fatal Errors */
};
}

/**
 * @file logic/MinecraftProcess.h
 * @brief The MinecraftProcess class
 */
class MinecraftProcess : public QProcess
{
	Q_OBJECT
public:
	/**
	 * @brief MinecraftProcess constructor
	 * @param inst the Instance pointer to launch
	 */
	MinecraftProcess(BaseInstance *inst);

	/**
	 * @brief launch minecraft
	 */
	void launch();

	BaseInstance *instance()
	{
		return m_instance;
	}

	void setWorkdir(QString path);

	void setLaunchScript(QString script)
	{
		launchScript = script;
	}

	void setNativeFolder(QString natives)
	{
		m_nativeFolder = natives;
	}

	void killMinecraft();

	inline void setLogin(MojangAccountPtr account)
	{
		m_account = account;
	}

signals:
	/**
	 * @brief emitted when Minecraft immediately fails to run
	 */
	void launch_failed(BaseInstance *);

	/**
	 * @brief emitted when the PreLaunchCommand fails
	 */
	void prelaunch_failed(BaseInstance *, int code, QProcess::ExitStatus status);

	/**
	 * @brief emitted when the PostLaunchCommand fails
	 */
	void postlaunch_failed(BaseInstance *, int code, QProcess::ExitStatus status);

	/**
	 * @brief emitted when mc has finished and the PostLaunchCommand was run
	 */
	void ended(BaseInstance *, int code, QProcess::ExitStatus status);

	/**
	 * @brief emitted when we want to log something
	 * @param text the text to log
	 * @param level the level to log at
	 */
	void log(QString text, MessageLevel::Enum level = MessageLevel::MultiMC);

protected:
	BaseInstance *m_instance = nullptr;
	QString m_err_leftover;
	QString m_out_leftover;
	QProcess m_prepostlaunchprocess;
	bool killed = false;
	MojangAccountPtr m_account;
	QString launchScript;
	QString m_nativeFolder;

protected
slots:
	void finish(int, QProcess::ExitStatus status);
	void on_stdErr();
	void on_stdOut();

private:
	QString censorPrivateInfo(QString in);
	MessageLevel::Enum getLevel(const QString &message, MessageLevel::Enum defaultLevel);
	
};
