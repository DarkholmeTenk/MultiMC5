#include "ScreenshotsPage.h"
#include "ui_ScreenshotsPage.h"

#include <QModelIndex>
#include <QMutableListIterator>
#include <QMap>
#include <QSet>
#include <QFileIconProvider>
#include <QFileSystemModel>
#include <QStyledItemDelegate>
#include <QLineEdit>
#include <QEvent>
#include <QPainter>
#include <QClipboard>
#include <QDesktopServices>
#include <QKeyEvent>

#include <pathutils.h>

#include "gui/dialogs/ProgressDialog.h"
#include "gui/dialogs/CustomMessageBox.h"
#include "logic/net/NetJob.h"
#include "logic/screenshots/ImgurUpload.h"
#include "logic/screenshots/ImgurAlbumCreation.h"
#include "logic/tasks/SequentialTask.h"

#include "logic/RWStorage.h"

typedef RWStorage<QString, QIcon> SharedIconCache;
typedef std::shared_ptr<SharedIconCache> SharedIconCachePtr;

class ThumbnailingResult : public QObject
{
	Q_OBJECT
public slots:
	inline void emitResultsReady(const QString &path) { emit resultsReady(path); }
	inline void emitResultsFailed(const QString &path) { emit resultsFailed(path); }
signals:
	void resultsReady(const QString &path);
	void resultsFailed(const QString &path);
};

class ThumbnailRunnable : public QRunnable
{
public:
	ThumbnailRunnable(QString path, SharedIconCachePtr cache)
	{
		m_path = path;
		m_cache = cache;
	}
	void run()
	{
		QFileInfo info(m_path);
		if (info.isDir())
			return;
		if ((info.suffix().compare("png", Qt::CaseInsensitive) != 0))
			return;
		int tries = 5;
		while (tries)
		{
			if (!m_cache->stale(m_path))
				return;
			QImage image(m_path);
			if (image.isNull())
			{
				QThread::msleep(500);
				tries--;
				continue;
			}
			QImage small;
			if (image.width() > image.height())
				small = image.scaledToWidth(512).scaledToWidth(256, Qt::SmoothTransformation);
			else
				small = image.scaledToHeight(512).scaledToHeight(256, Qt::SmoothTransformation);
			auto smallSize = small.size();
			QPoint offset((256 - small.width()) / 2, (256 - small.height()) / 2);
			QImage square(QSize(256, 256), QImage::Format_ARGB32);
			square.fill(Qt::transparent);

			QPainter painter(&square);
			painter.drawImage(offset, small);
			painter.end();

			QIcon icon(QPixmap::fromImage(square));
			m_cache->add(m_path, icon);
			m_resultEmitter.emitResultsReady(m_path);
			return;
		}
		m_resultEmitter.emitResultsFailed(m_path);
	}
	QString m_path;
	SharedIconCachePtr m_cache;
	ThumbnailingResult m_resultEmitter;
};

// this is about as elegant and well written as a bag of bricks with scribbles done by insane
// asylum patients.
class FilterModel : public QIdentityProxyModel
{
	Q_OBJECT
public:
	explicit FilterModel(QObject *parent = 0) : QIdentityProxyModel(parent)
	{
		m_thumbnailingPool.setMaxThreadCount(4);
		m_thumbnailCache = std::make_shared<SharedIconCache>();
		m_thumbnailCache->add("placeholder", QIcon::fromTheme("screenshot-placeholder"));
		connect(&watcher, SIGNAL(fileChanged(QString)), SLOT(fileChanged(QString)));
		// FIXME: the watched file set is not updated when files are removed
	}
	virtual ~FilterModel() { m_thumbnailingPool.waitForDone(500); }
	virtual QVariant data(const QModelIndex &proxyIndex, int role = Qt::DisplayRole) const
	{
		auto model = sourceModel();
		if (!model)
			return QVariant();
		if (role == Qt::DisplayRole || role == Qt::EditRole)
		{
			QVariant result = sourceModel()->data(mapToSource(proxyIndex), role);
			return result.toString().remove(QRegExp("\\.png$"));
		}
		if (role == Qt::DecorationRole)
		{
			QVariant result =
				sourceModel()->data(mapToSource(proxyIndex), QFileSystemModel::FilePathRole);
			QString filePath = result.toString();
			QIcon temp;
			if (!watched.contains(filePath))
			{
				((QFileSystemWatcher &)watcher).addPath(filePath);
				((QSet<QString> &)watched).insert(filePath);
			}
			if (m_thumbnailCache->get(filePath, temp))
			{
				return temp;
			}
			if (!m_failed.contains(filePath))
			{
				((FilterModel *)this)->thumbnailImage(filePath);
			}
			return (m_thumbnailCache->get("placeholder"));
		}
		return sourceModel()->data(mapToSource(proxyIndex), role);
	}
	virtual bool setData(const QModelIndex &index, const QVariant &value,
						 int role = Qt::EditRole)
	{
		auto model = sourceModel();
		if (!model)
			return false;
		if (role != Qt::EditRole)
			return false;
		// FIXME: this is a workaround for a bug in QFileSystemModel, where it doesn't
		// sort after renames
		{
			((QFileSystemModel *)model)->setNameFilterDisables(true);
			((QFileSystemModel *)model)->setNameFilterDisables(false);
		}
		return model->setData(mapToSource(index), value.toString() + ".png", role);
	}

private:
	void thumbnailImage(QString path)
	{
		auto runnable = new ThumbnailRunnable(path, m_thumbnailCache);
		connect(&(runnable->m_resultEmitter), SIGNAL(resultsReady(QString)),
				SLOT(thumbnailReady(QString)));
		connect(&(runnable->m_resultEmitter), SIGNAL(resultsFailed(QString)),
				SLOT(thumbnailFailed(QString)));
		((QThreadPool &)m_thumbnailingPool).start(runnable);
	}
private slots:
	void thumbnailReady(QString path) { emit layoutChanged(); }
	void thumbnailFailed(QString path) { m_failed.insert(path); }
	void fileChanged(QString filepath)
	{
		m_thumbnailCache->setStale(filepath);
		thumbnailImage(filepath);
		// reinsert the path...
		watcher.removePath(filepath);
		watcher.addPath(filepath);
	}

private:
	SharedIconCachePtr m_thumbnailCache;
	QThreadPool m_thumbnailingPool;
	QSet<QString> m_failed;
	QSet<QString> watched;
	QFileSystemWatcher watcher;
};

class CenteredEditingDelegate : public QStyledItemDelegate
{
public:
	explicit CenteredEditingDelegate(QObject *parent = 0) : QStyledItemDelegate(parent) {}
	virtual ~CenteredEditingDelegate() {}
	virtual QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem &option,
								  const QModelIndex &index) const
	{
		auto widget = QStyledItemDelegate::createEditor(parent, option, index);
		auto foo = dynamic_cast<QLineEdit *>(widget);
		if (foo)
		{
			foo->setAlignment(Qt::AlignHCenter);
			foo->setFrame(true);
			foo->setMaximumWidth(192);
		}
		return widget;
	}
};

ScreenshotsPage::ScreenshotsPage(BaseInstance *instance, QWidget *parent)
	: QWidget(parent), ui(new Ui::ScreenshotsPage)
{
	m_model.reset(new QFileSystemModel());
	m_filterModel.reset(new FilterModel());
	m_filterModel->setSourceModel(m_model.get());
	m_model->setFilter(QDir::Files | QDir::Writable | QDir::Readable);
	m_model->setReadOnly(false);
	m_model->setNameFilters({"*.png"});
	m_model->setNameFilterDisables(false);
	m_folder = PathCombine(instance->minecraftRoot(), "screenshots");
	m_valid = ensureFolderPathExists(m_folder);

	ui->setupUi(this);
	ui->tabWidget->tabBar()->hide();
	ui->listView->setModel(m_filterModel.get());
	ui->listView->setIconSize(QSize(128, 128));
	ui->listView->setGridSize(QSize(192, 160));
	ui->listView->setSpacing(9);
	// ui->listView->setUniformItemSizes(true);
	ui->listView->setLayoutMode(QListView::Batched);
	ui->listView->setViewMode(QListView::IconMode);
	ui->listView->setResizeMode(QListView::Adjust);
	ui->listView->installEventFilter(this);
	ui->listView->setEditTriggers(0);
	ui->listView->setItemDelegate(new CenteredEditingDelegate(this));
	connect(ui->listView, SIGNAL(activated(QModelIndex)), SLOT(onItemActivated(QModelIndex)));
}

bool ScreenshotsPage::eventFilter(QObject *obj, QEvent *evt)
{
	if (obj != ui->listView)
		return QWidget::eventFilter(obj, evt);
	if (evt->type() != QEvent::KeyPress)
	{
		return QWidget::eventFilter(obj, evt);
	}
	QKeyEvent *keyEvent = static_cast<QKeyEvent *>(evt);
	switch (keyEvent->key())
	{
	case Qt::Key_Delete:
		on_deleteBtn_clicked();
		return true;
	case Qt::Key_F2:
		on_renameBtn_clicked();
		return true;
	default:
		break;
	}
	return QWidget::eventFilter(obj, evt);
}

ScreenshotsPage::~ScreenshotsPage()
{
	delete ui;
}

void ScreenshotsPage::onItemActivated(QModelIndex index)
{
	if (!index.isValid())
		return;
	auto info = m_model->fileInfo(index);
	QString fileName = info.absoluteFilePath();
	openFileInDefaultProgram(info.absoluteFilePath());
}

void ScreenshotsPage::on_viewFolderBtn_clicked()
{
	openDirInDefaultProgram(m_folder, true);
}

void ScreenshotsPage::on_uploadBtn_clicked()
{
	auto selection = ui->listView->selectionModel()->selectedIndexes();
	if (selection.isEmpty())
		return;

	QList<ScreenshotPtr> uploaded;
	auto job = std::make_shared<NetJob>("Screenshot Upload");
	for (auto item : selection)
	{
		auto info = m_model->fileInfo(item);
		auto screenshot = std::make_shared<ScreenShot>(info);
		uploaded.push_back(screenshot);
		job->addNetAction(ImgurUpload::make(screenshot));
	}
	SequentialTask task;
	auto albumTask = std::make_shared<NetJob>("Imgur Album Creation");
	auto imgurAlbum = ImgurAlbumCreation::make(uploaded);
	albumTask->addNetAction(imgurAlbum);
	task.addTask(job);
	task.addTask(albumTask);
	ProgressDialog prog(this);
	if (prog.exec(&task) != QDialog::Accepted)
	{
		CustomMessageBox::selectable(this, tr("Failed to upload screenshots!"),
									 tr("Unknown error"), QMessageBox::Warning)->exec();
	}
	else
	{
		auto link = QString("https://imgur.com/a/%1").arg(imgurAlbum->id());
		QClipboard *clipboard = QApplication::clipboard();
		clipboard->setText(link);
		QDesktopServices::openUrl(link);
		CustomMessageBox::selectable(
			this, tr("Upload finished"),
			tr("The <a href=\"%1\">link  to the uploaded album</a> has been opened in the "
			   "default browser and placed in your clipboard.<br/>Delete hash: %2 (save "
			   "this if you want to be able to edit/delete the album)")
				.arg(link, imgurAlbum->deleteHash()),
			QMessageBox::Information)->exec();
	}
}

void ScreenshotsPage::on_deleteBtn_clicked()
{
	auto mbox = CustomMessageBox::selectable(
		this, tr("Are you sure?"), tr("This will delete all selected screenshots."),
		QMessageBox::Warning, QMessageBox::Yes | QMessageBox::No);
	std::unique_ptr<QMessageBox> box(mbox);

	if (box->exec() != QMessageBox::Yes)
		return;

	auto selected = ui->listView->selectionModel()->selectedIndexes();
	for (auto item : selected)
	{
		m_model->remove(item);
	}
}

void ScreenshotsPage::on_renameBtn_clicked()
{
	auto selection = ui->listView->selectionModel()->selectedIndexes();
	if (selection.isEmpty())
		return;
	ui->listView->edit(selection[0]);
	// TODO: mass renaming
}

void ScreenshotsPage::opened()
{
	if (m_valid)
	{
		QString path = QDir(m_folder).absolutePath();
		m_model->setRootPath(path);
		ui->listView->setRootIndex(m_filterModel->mapFromSource(m_model->index(path)));
	}
}

#include "ScreenshotsPage.moc"
