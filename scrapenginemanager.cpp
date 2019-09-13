#include "scrapenginemanager.h"
#include "ui_scrapenginemanager.h"

ScrapEngineManager::ScrapEngineManager(QWidget *parent, Config *config) :
    QDialog(parent),
    ui(new Ui::ScrapEngineManager)
{
    this->config = config;
    _defaultParentFont = QFont("", 10);

    ui->setupUi(this);

    ui->seTreeWidget->setContextMenuPolicy(Qt::CustomContextMenu);
    QObject::connect(ui->seTreeWidget,SIGNAL(customContextMenuRequested(QPoint)),this,SLOT(customContextMenuRequestedSlot(QPoint)));
    this->populateTreeView();
}

ScrapEngineManager::~ScrapEngineManager()
{
    delete ui;
}


void ScrapEngineManager::addItemToListView(QString itemLabel)
{
    ui->seToSortView->insertItem(ui->seToSortView->count() + 1,itemLabel);
    ui->seToSortView->sortItems();
}

void ScrapEngineManager::on_okButton_clicked()
{
    int i,j;
    int parentId;
    QTreeWidgetItem *parentItem;
    QTreeWidgetItem *childItem;
    QHash<int,int> idsList;


    // We loop on the toplevelitems
    for(i=0;i<ui->seTreeWidget->topLevelItemCount();i++)
    {
        parentItem  = ui->seTreeWidget->topLevelItem(i);
        parentId    = config->getSEGroupIdByName(parentItem->text(0));

        // And for each tLI, we loop on the childs
        for(j=0;j<parentItem->childCount();j++)
        {
            childItem = parentItem->child(j);
            // We build the QHash in order to generate the sqlite query
            idsList.insert(config->getSEIdByName(childItem->text(0)),parentId);
        }
    }
    this->setCursor(Qt::BusyCursor);
    config->setMultipleParentGroup(idsList);
    this->setCursor(Qt::ArrowCursor);
    this->accept();

}

void ScrapEngineManager::on_abortButton_clicked()
{
    this->reject();
}

void ScrapEngineManager::on_addGroupButton_clicked()
{
    bool ok;

    QString text = QInputDialog::getText(this,tr("New Scrap Engine Group"),tr("Please enter the new scrap engine group name :"), QLineEdit::Normal,                                       "", &ok);

    if (ok && !text.isEmpty())
    {
        // Add the new item on the treewidget
        QTreeWidgetItem* newItem = new QTreeWidgetItem;
        newItem->setText(0,text);
        ui->seTreeWidget->addTopLevelItem(newItem);

        // And we have to add it on the sqlite DB
        config->insertSeGroup(text);
    }
}

void ScrapEngineManager::on_deleteGroupButton_clicked()
{
    QList<QTreeWidgetItem*> selectedItem = ui->seTreeWidget->selectedItems();
    QList<QTreeWidgetItem *> childsItems;
    int keepChilds;

    if(selectedItem.count() == 1)
    {
        if(selectedItem.at(0)->childCount()) // If item has child, ask the user if he want to keep childs or delete them
        {
            keepChilds = QMessageBox::question(this, tr("Keep Scrap Engines"),tr("Do you want to keep the scrap engines files ? "), QMessageBox::Yes | QMessageBox::No | QMessageBox::Abort);
            if(keepChilds == QMessageBox::Yes) // Keep SE Childs
            {
                ui->seTreeWidget->takeTopLevelItem(ui->seTreeWidget->indexOfTopLevelItem(selectedItem.at(0)));
                childsItems = selectedItem.at(0)->takeChildren();
                foreach(QTreeWidgetItem *itemWidget, childsItems)
                {
                    this->addItemToListView(itemWidget->text(0));
                    // Unset parent on the DB and set visible to false
                    config->setParentGroup(itemWidget->text(0), 0);
                }
            }
            else if (keepChilds == QMessageBox::No)
            {
                ui->seTreeWidget->takeTopLevelItem(ui->seTreeWidget->indexOfTopLevelItem(selectedItem.at(0)));
                childsItems = selectedItem.at(0)->takeChildren();
                foreach(QTreeWidgetItem *itemWidget, childsItems)
                {
                    // Delete the Scrap engine
                    config->deleteSe(config->getRealSeId(itemWidget->data(0,Qt::WhatsThisRole).toString()));
                }
            }
        }
        else
        {
            ui->seTreeWidget->takeTopLevelItem(ui->seTreeWidget->indexOfTopLevelItem(selectedItem.at(0)));
            config->deleteSeGroup(selectedItem.at(0)->text(0));
        }
    }
}

void ScrapEngineManager::on_renameButton_clicked()
{
    // We get the selected item
    QList<QTreeWidgetItem*> selectedItem = ui->seTreeWidget->selectedItems();
    QString oldName;
    bool ok;

    if(selectedItem.count() == 1)
    {
        if(selectedItem.at(0)->childCount())
        {
            oldName = selectedItem.at(0)->text(0);
            QString text = QInputDialog::getText(this,tr("New Scrap Engine Group"),tr("Please enter the new scrap engine group name :"), QLineEdit::Normal,                                       oldName, &ok);

            if (ok && !text.isEmpty())
            {
                // we update it on the UI
                selectedItem.at(0)->setText(0,text);

                // we save the new name on DB
                config->updateSeGroupName(oldName, text);
            }
        }
        else
            QMessageBox::critical(this,tr("Rename group"),tr("You can only rename Group, not scrap engines"),QMessageBox::Yes);
    }
    else
        QMessageBox::critical(this,tr("No Group selected"),tr("Please select a group first"),QMessageBox::Yes);
}

void ScrapEngineManager::enableDisableGroup()
{
    // We get the selected item
    QList<QTreeWidgetItem*> selectedItem = ui->seTreeWidget->selectedItems();
    int currentStatus;

    if(selectedItem.count() == 1)
    {
        if(selectedItem.at(0)->childCount())
        {
            currentStatus = config->getSeGroupStatus(selectedItem.at(0)->text(0));
            if(currentStatus)
                _defaultParentFont.setStrikeOut(true);
            else
                _defaultParentFont.setStrikeOut(false);
            // we update it on the UI
            selectedItem.at(0)->setFont(0,_defaultParentFont);

            // we save the new status on DB
            config->setSeGroupStatus(selectedItem.at(0)->text(0),((currentStatus) ? 0 : 1 ));
        }
        else
            QMessageBox::critical(this,tr("Rename group"),tr("You can only rename Group, not scrap engines"),QMessageBox::Yes);
    }
    else
        QMessageBox::critical(this,tr("No Group selected"),tr("Please select a group first"),QMessageBox::Yes);
}

void ScrapEngineManager::customContextMenuRequestedSlot(QPoint pos)
{
    QMenu *menu=new QMenu(this);

    QAction *createNew = new QAction(tr("Create new scrap engines group"), this);
    QAction *deleteOne = new QAction(tr("Delete"), this);
    QAction *renameOne = new QAction(tr("Rename"), this);
    QAction *enableDisable = new QAction(tr("Enable / Disable"), this);

    menu->addAction(createNew);
    menu->addAction(deleteOne);
    menu->addAction(renameOne);
    menu->addAction(enableDisable);

    QObject::connect(createNew,SIGNAL(triggered()),this,SLOT(on_addGroupButton_clicked()));
    QObject::connect(deleteOne,SIGNAL(triggered()),this,SLOT(on_deleteGroupButton_clicked()));
    QObject::connect(renameOne,SIGNAL(triggered()),this,SLOT(on_renameButton_clicked()));
    QObject::connect(enableDisable,SIGNAL(triggered()),this,SLOT(enableDisableGroup()));

    menu->popup(ui->seTreeWidget->viewport()->mapToGlobal(pos));
}


void ScrapEngineManager::populateTreeView()
{    
    QMap<QString, QPair<QString,int> > SEMap = this->config->getAllSe();
    QMapIterator<QString,QPair<QString,int> > mapIterator(SEMap);

    QTreeWidgetItem* top_item = nullptr;
    QTreeWidgetItem* child_item;
    QListWidgetItem* listItem;
    QString currentSeGroup;
    int i,j;

    currentSeGroup = "";
    i=j=0;
    mapIterator.toFront();
    while (mapIterator.hasNext())
    {
        mapIterator.next();

        if((mapIterator.key() != currentSeGroup) || !i)
        {
            currentSeGroup = mapIterator.key();
            if(!currentSeGroup.isEmpty())
            {
                top_item = new QTreeWidgetItem;
                top_item->setText(0,mapIterator.key());
                if(!config->getSeGroupStatus(mapIterator.key()))
                {
                    _defaultParentFont.setStrikeOut(true);
                    top_item->setFont(0,_defaultParentFont);
                    _defaultParentFont.setStrikeOut(false);
                }
                ui->seTreeWidget->addTopLevelItem(top_item);
            }
        }

        if(mapIterator.key().isEmpty())
        {
            listItem = new QListWidgetItem;
            listItem->setText(mapIterator.value().first);
            ui->seToSortView->insertItem(j,listItem);
            j++;
        }
        else
        {
            child_item = new QTreeWidgetItem;
            child_item->setFlags(child_item->flags() & ~(Qt::ItemIsDropEnabled));
            child_item->setText(0,mapIterator.value().first);
            top_item->addChild(child_item);
        }
        i++;
    }
    ui->seToSortView->sortItems();
}
