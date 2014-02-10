#include "ChooseInstallModDialog.h"
#include "ui_ChooseInstallModDialog.h"

#include <QSortFilterProxyModel>
#include <QListView>
#include <QMessageBox>

#include "logic/quickmod/QuickModsList.h"
#include "gui/dialogs/quickmod/QuickModInstallDialog.h"
#include "ChooseQuickModVersionDialog.h"
#include "AddQuickModFileDialog.h"
#include "logic/quickmod/QuickMod.h"
#include "logic/OneSixInstance.h"

#include "MultiMC.h"

template <typename T> bool intersectLists(const QList<T> &l1, const QList<T> &l2)
{
	foreach(const T & item, l1)
	{
		if (!l2.contains(item))
		{
			return false;
		}
	}

	return true;
}
bool listContainsSubstring(const QStringList &list, const QString &str)
{
	foreach(const QString & item, list)
	{
		if (item.contains(str))
		{
			return true;
		}
	}
	return false;
}

class ModFilterProxyModel : public QSortFilterProxyModel
{
	Q_OBJECT
public:
	ModFilterProxyModel(QObject *parent = 0) : QSortFilterProxyModel(parent)
	{
		setSortRole(Qt::DisplayRole);
		setSortCaseSensitivity(Qt::CaseInsensitive);
	}

	void setSourceModel(QAbstractItemModel *model)
	{
		QSortFilterProxyModel::setSourceModel(model);
		connect(model, &QAbstractItemModel::modelReset, this, &ModFilterProxyModel::resort);
		connect(model, SIGNAL(rowsInserted(QModelIndex, int, int)), this, SLOT(resort()));
		resort();
	}

	void setTags(const QStringList &tags)
	{
		m_tags = tags;
		invalidateFilter();
	}
	void setCategory(const QString &category)
	{
		m_category = category;
		invalidateFilter();
	}
	void setFulltext(const QString &query)
	{
		m_fulltext = query;
		invalidateFilter();
	}

private
slots:
	void resort()
	{
		sort(0);
	}

protected:
	bool filterAcceptsRow(int source_row, const QModelIndex &source_parent) const
	{
		const QModelIndex index = sourceModel()->index(source_row, 0, source_parent);

		if (index.data(QuickModsList::IsStubRole).toBool())
		{
			return false;
		}

		if (!m_tags.isEmpty())
		{
			if (!intersectLists(m_tags, index.data(QuickModsList::TagsRole).toStringList()))
			{
				return false;
			}
		}
		if (!m_category.isEmpty())
		{
			if (!listContainsSubstring(index.data(QuickModsList::CategoriesRole).toStringList(),
									   m_category))
			{
				return false;
			}
		}
		if (!m_fulltext.isEmpty())
		{
			bool inName = index.data(QuickModsList::NameRole).toString().contains(
				m_fulltext, Qt::CaseInsensitive);
			bool inDesc = index.data(QuickModsList::DescriptionRole).toString().contains(
				m_fulltext, Qt::CaseInsensitive);
			if (!inName && !inDesc)
			{
				return false;
			}
		}

		return true;
	}

private:
	QStringList m_tags;
	QString m_category;
	QString m_fulltext;
};

class TagsValidator : public QValidator
{
	Q_OBJECT
public:
	TagsValidator(QObject *parent = 0) : QValidator(parent)
	{
	}

protected:
	State validate(QString &input, int &pos) const
	{
		// TODO write a good validator
		return Acceptable;
	}
};

ChooseInstallModDialog::ChooseInstallModDialog(BaseInstance *instance, QWidget *parent)
	: QDialog(parent), ui(new Ui::ChooseInstallModDialog), m_currentMod(0),
	  m_instance(instance), m_view(new QListView(this)), m_model(new ModFilterProxyModel(this))
{
	ui->setupUi(this);

	ui->viewLayout->addWidget(m_view);

	ui->tagsEdit->setValidator(new TagsValidator(this));

	m_view->setSelectionBehavior(QListView::SelectRows);
	m_view->setSelectionMode(QListView::SingleSelection);
	m_model->setSourceModel(MMC->quickmodslist().get());
	m_view->setModel(m_model);

	connect(m_view->selectionModel(), &QItemSelectionModel::selectionChanged, this,
			&ChooseInstallModDialog::modSelectionChanged);
	connect(MMC->quickmodslist().get(), &QuickModsList::modsListChanged, this,
			&ChooseInstallModDialog::setupCategoryBox);

	setupCategoryBox();
}

ChooseInstallModDialog::~ChooseInstallModDialog()
{
	delete ui;
}

void ChooseInstallModDialog::on_installButton_clicked()
{
	if (m_view->selectionModel()->selection().isEmpty())
	{
		return;
	}

	auto mod = m_view->selectionModel()->selectedIndexes().first().data(QuickModsList::QuickModRole).value<QuickMod *>();
	if (static_cast<OneSixInstance *>(m_instance)->getFullVersion()->quickmods.contains(mod->uid()))
	{
		return;
	}

	QFile userFile(m_instance->instanceRoot() + "/user.json");
	if (!userFile.open(QFile::ReadWrite))
	{
		QMessageBox::critical(this, tr("Error"), tr("Couldn't open %1 for reading and writing: %2").arg(userFile.fileName(), userFile.errorString()));
		return;
	}
	// TODO more error reporting
	QJsonObject obj = QJsonDocument::fromJson(userFile.readAll()).object();
	QJsonArray plusmods = obj.value("+mods").toArray();
	QJsonObject modObject;
	modObject.insert(mod->uid(), QString());
	plusmods.append(modObject);
	obj.insert("+mods", plusmods);
	userFile.seek(0);
	userFile.write(QJsonDocument(obj).toJson(QJsonDocument::Indented));
	userFile.close();
	static_cast<OneSixInstance *>(m_instance)->reloadVersion(this);
	// TODO notify the user of the success
}
void ChooseInstallModDialog::on_closeButton_clicked()
{
	reject();
}

void ChooseInstallModDialog::on_categoriesLabel_linkActivated(const QString &link)
{
	ui->categoryBox->setCurrentText(link);
	ui->tagsEdit->setText(QString());
}
void ChooseInstallModDialog::on_tagsLabel_linkActivated(const QString &link)
{
	ui->tagsEdit->setText(ui->tagsEdit->text() + ", " + link);
	ui->categoryBox->setCurrentText(QString());
	on_tagsEdit_textChanged();
}
void ChooseInstallModDialog::on_fulltextEdit_textChanged()
{
	m_model->setFulltext(ui->fulltextEdit->text());
}
void ChooseInstallModDialog::on_tagsEdit_textChanged()
{
	m_model->setTags(
		ui->tagsEdit->text().split(QRegularExpression(", {0,1}"), QString::SkipEmptyParts));
}
void ChooseInstallModDialog::on_categoryBox_currentTextChanged()
{
	m_model->setCategory(ui->categoryBox->currentText());
}

void ChooseInstallModDialog::modSelectionChanged(const QItemSelection &selected,
												 const QItemSelection &deselected)
{
	if (m_currentMod)
	{
		disconnect(m_currentMod, &QuickMod::logoUpdated, this,
				   &ChooseInstallModDialog::modLogoUpdated);
	}

	if (selected.isEmpty())
	{
		m_currentMod = 0;
		ui->nameLabel->setText("");
		ui->descriptionLabel->setText("");
		ui->websiteLabel->setText("");
		ui->categoriesLabel->setText("");
		ui->tagsLabel->setText("");
		ui->logoLabel->setPixmap(QPixmap());
	}
	else
	{
		m_currentMod = m_model->index(selected.first().top(), 0)
						   .data(QuickModsList::QuickModRole)
						   .value<QuickMod *>();
		ui->nameLabel->setText(m_currentMod->name());
		ui->descriptionLabel->setText(m_currentMod->description());
		ui->websiteLabel->setText(QString("<a href=\"%1\">%2</a>").arg(
			m_currentMod->websiteUrl().toString(QUrl::FullyEncoded),
			m_currentMod->websiteUrl().toString(QUrl::PrettyDecoded)));
		QStringList categories;
		foreach(const QString & category, m_currentMod->categories())
		{
			categories.append(QString("<a href=\"%1\">%1</a>").arg(category));
		}
		ui->categoriesLabel->setText(categories.join(", "));
		QStringList tags;
		foreach(const QString & tag, m_currentMod->tags())
		{
			tags.append(QString("<a href=\"%1\">%1</a>").arg(tag));
		}
		ui->tagsLabel->setText(tags.join(", "));
		ui->logoLabel->setPixmap(m_currentMod->logo());

		connect(m_currentMod, &QuickMod::logoUpdated, this,
				&ChooseInstallModDialog::modLogoUpdated);
	}
}

void ChooseInstallModDialog::modLogoUpdated()
{
	ui->logoLabel->setPixmap(m_currentMod->logo());
}

void ChooseInstallModDialog::setupCategoryBox()
{
	QStringList categories;
	categories.append("");

	for (int i = 0; i < MMC->quickmodslist()->numMods(); ++i)
	{
		categories.append(MMC->quickmodslist()->modAt(i)->categories());
	}

	categories.removeDuplicates();

	ui->categoryBox->clear();
	ui->categoryBox->addItems(categories);
}

void ChooseInstallModDialog::on_addButton_clicked()
{
	AddQuickModFileDialog dialog(this);
	if (dialog.exec() == QDialog::Accepted)
	{
		switch (dialog.type())
		{
		case AddQuickModFileDialog::FileName:
			MMC->quickmodslist()->registerMod(dialog.fileName());
			break;
		case AddQuickModFileDialog::Url:
			MMC->quickmodslist()->registerMod(dialog.url());
			break;
		}
	}
}
void ChooseInstallModDialog::on_updateButton_clicked()
{
	MMC->quickmodslist()->updateFiles();
}

#include "ChooseInstallModDialog.moc"
