/*
 * Copyright (C) by Olivier Goffart <ogoffart@woboq.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
 */
#include "selectivesyncdialog.h"
#include "folder.h"
#include "account.h"
#include "networkjobs.h"
#include "theme.h"
#include "folderman.h"
#include <QDialogButtonBox>
#include <QVBoxLayout>
#include <QTreeWidget>
#include <qpushbutton.h>
#include <QFileIconProvider>
#include <QDebug>
#include <QSettings>
#include <QScopedValueRollback>

namespace Mirall {

SelectiveSyncTreeView::SelectiveSyncTreeView(const QString& folderPath, const QString &rootName,
                                             const QStringList &oldBlackList, QWidget* parent)
    : QTreeWidget(parent), _folderPath(folderPath), _rootName(rootName), _oldBlackList(oldBlackList)
{
    connect(this, SIGNAL(itemExpanded(QTreeWidgetItem*)), this, SLOT(slotItemExpanded(QTreeWidgetItem*)));
    connect(this, SIGNAL(itemChanged(QTreeWidgetItem*,int)), this, SLOT(slotItemChanged(QTreeWidgetItem*,int)));
}

void SelectiveSyncTreeView::refreshFolders()
{
    LsColJob *job = new LsColJob(AccountManager::instance()->account(), _folderPath, this);
    connect(job, SIGNAL(directoryListing(QStringList)),
            this, SLOT(slotUpdateDirectories(QStringList)));
    job->start();
    clear();
}

static QTreeWidgetItem* findFirstChild(QTreeWidgetItem *parent, const QString& text)
{
    for (int i = 0; i < parent->childCount(); ++i) {
        QTreeWidgetItem *child = parent->child(i);
        if (child->text(0) == text) {
            return child;
        }
    }
    return 0;
}

void SelectiveSyncTreeView::recursiveInsert(QTreeWidgetItem* parent, QStringList pathTrail, QString path)
{
    QFileIconProvider prov;
    QIcon folderIcon = prov.icon(QFileIconProvider::Folder);
    if (pathTrail.size() == 0) {
        if (path.endsWith('/')) {
            path.chop(1);
        }
        parent->setToolTip(0, path);
        parent->setData(0, Qt::UserRole, path);
    } else {
        QTreeWidgetItem *item = findFirstChild(parent, pathTrail.first());
        if (!item) {
            item = new QTreeWidgetItem(parent);
            if (parent->checkState(0) == Qt::Checked) {
                item->setCheckState(0, Qt::Checked);
            } else if (parent->checkState(0) == Qt::Unchecked) {
                item->setCheckState(0, Qt::Unchecked);
            } else {
                item->setCheckState(0, Qt::Checked);
                foreach(const QString &str , _oldBlackList) {
                    if (str + "/" == path) {
                        item->setCheckState(0, Qt::Unchecked);
                        break;
                    } else if (str.startsWith(path)) {
                        item->setCheckState(0, Qt::PartiallyChecked);
                    }
                }
            }
            item->setIcon(0, folderIcon);
            item->setText(0, pathTrail.first());
//            item->setData(0, Qt::UserRole, pathTrail.first());
            item->setChildIndicatorPolicy(QTreeWidgetItem::ShowIndicator);
        }

        pathTrail.removeFirst();
        recursiveInsert(item, pathTrail, path);
    }
}

void SelectiveSyncTreeView::slotUpdateDirectories(const QStringList&list)
{
    QScopedValueRollback<bool> isInserting(_inserting);
    _inserting = true;

    QTreeWidgetItem *root = topLevelItem(0);
    if (!root) {
        root = new QTreeWidgetItem(this);
        root->setText(0, _rootName);
        root->setIcon(0, Theme::instance()->applicationIcon());
        root->setData(0, Qt::UserRole, _folderPath);
        if (_oldBlackList.isEmpty()) {
            root->setCheckState(0, Qt::Checked);
        } else {
            root->setCheckState(0, Qt::PartiallyChecked);
        }
    }

    Account *account = AccountManager::instance()->account();
    QUrl url = account->davUrl();
    QString pathToRemove = url.path();
    if (!pathToRemove.endsWith('/')) {
        pathToRemove.append('/');
    }
    pathToRemove.append(_folderPath);
    pathToRemove.append('/');

    foreach (QString path, list) {
        path.remove(pathToRemove);
        QStringList paths = path.split('/');
        if (paths.last().isEmpty()) paths.removeLast();
        if (paths.isEmpty())
            continue;
        recursiveInsert(root, paths, path);
    }
    root->setExpanded(true);
}

void SelectiveSyncTreeView::slotItemExpanded(QTreeWidgetItem *item)
{
    QString dir = item->data(0, Qt::UserRole).toString();
    LsColJob *job = new LsColJob(AccountManager::instance()->account(), dir, this);
    connect(job, SIGNAL(directoryListing(QStringList)),
            SLOT(slotUpdateDirectories(QStringList)));
    job->start();
}

void SelectiveSyncTreeView::slotItemChanged(QTreeWidgetItem *item, int col)
{
    if (col != 0 || _inserting)
        return;

    if (item->checkState(0) == Qt::Checked) {
        // If we are checked, check that we may need to check the parent as well if
        // all the sibilings are also checked
        QTreeWidgetItem *parent = item->parent();
        if (parent && parent->checkState(0) != Qt::Checked) {
            bool hasUnchecked = false;
            for (int i = 0; i < parent->childCount(); ++i) {
                if (parent->child(i)->checkState(0) != Qt::Checked) {
                    hasUnchecked = true;
                    break;
                }
            }
            if (!hasUnchecked) {
                parent->setCheckState(0, Qt::Checked);
            } else if (parent->checkState(0) == Qt::Unchecked) {
                parent->setCheckState(0, Qt::PartiallyChecked);
            }
        }
        // also check all the childs
        for (int i = 0; i < item->childCount(); ++i) {
            if (item->child(i)->checkState(0) != Qt::Checked) {
                item->child(i)->setCheckState(0, Qt::Checked);
            }
        }
    }

    if (item->checkState(0) == Qt::Unchecked) {
        QTreeWidgetItem *parent = item->parent();
        if (parent && parent->checkState(0) != Qt::Unchecked) {
            bool hasChecked = false;
            for (int i = 0; i < parent->childCount(); ++i) {
                if (parent->child(i)->checkState(0) != Qt::Unchecked) {
                    hasChecked = true;
                    break;
                }
            }
            if (!hasChecked) {
                parent->setCheckState(0, Qt::Unchecked);
            } else if (parent->checkState(0) == Qt::Checked) {
                parent->setCheckState(0, Qt::PartiallyChecked);
            }
        }

        // Uncheck all the childs
        for (int i = 0; i < item->childCount(); ++i) {
            if (item->child(i)->checkState(0) != Qt::Unchecked) {
                item->child(i)->setCheckState(0, Qt::Unchecked);
            }
        }
    }

    if (item->checkState(0) == Qt::PartiallyChecked) {
        QTreeWidgetItem *parent = item->parent();
        if (parent && parent->checkState(0) != Qt::PartiallyChecked) {
            parent->setCheckState(0, Qt::PartiallyChecked);
        }
    }
}

QStringList SelectiveSyncTreeView::createBlackList(QTreeWidgetItem* root) const
{
    if (!root) {
        root = topLevelItem(0);
    }
    if (!root) return {};

    switch(root->checkState(0)) {
    case Qt::Unchecked:
        return { root->data(0, Qt::UserRole).toString() };
    case  Qt::Checked:
        return {};
    case Qt::PartiallyChecked:
        break;
    }

    QStringList result;
    if (root->childCount()) {
        for (int i = 0; i < root->childCount(); ++i) {
            result += createBlackList(root->child(i));
        }
    } else {
        // We did not load from the server so we re-use the one from the old black list
        QString path = root->data(0, Qt::UserRole).toString();
        foreach (const QString & it, _oldBlackList) {
            if (it.startsWith(path))
                result += it;
        }
    }
    return result;
}



SelectiveSyncDialog::SelectiveSyncDialog(Folder* folder, QWidget* parent, Qt::WindowFlags f)
    :   QDialog(parent, f), _folder(folder)
{
    QVBoxLayout *layout = new QVBoxLayout(this);
    _treeView = new SelectiveSyncTreeView(_folder->remotePath(), _folder->alias(), _folder->selectiveSyncBlackList(), parent);
    layout->addWidget(_treeView);
    QDialogButtonBox *buttonBox = new QDialogButtonBox(Qt::Horizontal);
    QPushButton *button;
    button = buttonBox->addButton(QDialogButtonBox::Ok);
    connect(button, SIGNAL(clicked()), this, SLOT(accept()));
    button = buttonBox->addButton(QDialogButtonBox::Cancel);
    connect(button, SIGNAL(clicked()), this, SLOT(reject()));
    layout->addWidget(buttonBox);

    // Make sure we don't get crashes if the folder is destroyed while we are still open
    connect(_folder, SIGNAL(destroyed(QObject*)), this, SLOT(deleteLater()));

    _treeView->refreshFolders();
}

void SelectiveSyncDialog::accept()
{
    QStringList blackList = _treeView->createBlackList();
    _folder->setSelectiveSyncBlackList(blackList);

    // FIXME: Use MirallConfigFile
    QSettings settings(_folder->configFile(), QSettings::IniFormat);
    settings.beginGroup(FolderMan::escapeAlias(_folder->alias()));
    settings.setValue("blackList", blackList);

    QDialog::accept();
}



}



