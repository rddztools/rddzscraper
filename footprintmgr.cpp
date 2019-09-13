#include "footprintmgr.h"
#include "ui_footprintmgr.h"

#include <QKeyEvent>
#include <QStandardItem>


FootprintMgr::FootprintMgr(QWidget *parent, Config *config) :
    QDialog(parent),
    ui(new Ui::FootprintMgr)
{
    ui->setupUi(this);
    ui->listView->installEventFilter(this);

    this->config = config;

    model = new QStringListModel(config->getFootprints());
    ui->listView->setModel(model);

    QObject::connect(ui->buttonBox, SIGNAL(accepted()), this, SLOT(accept()));
    QObject::connect(ui->buttonBox, SIGNAL(rejected()), this, SLOT(reject()));
}

FootprintMgr::~FootprintMgr()
{
    delete ui;
}


bool FootprintMgr::eventFilter(QObject *obj, QEvent *event)
{
    QString objName = obj->objectName();
    QKeyEvent *c = static_cast<QKeyEvent *>(event);

    if(objName == "listView")
    {
        if(c->matches(QKeySequence::Delete))
        {
            QItemSelectionModel *selection = ui->listView->selectionModel();
            QModelIndexList selectList = selection->selectedIndexes();
            this->deleteItems(selectList);
            return true;
        }
    }
    return false;
}

void FootprintMgr::accept()
{
   QStringList footprints;

    for(int i=0;ui->listView->model()->rowCount()>i;i++)
        footprints.append(model->index(i).data().toString());

    footprints.removeDuplicates();
    config->insertFootprints(footprints);
    config->loadFootprints();
    this->close();
}

void FootprintMgr::deleteItems(QModelIndexList items)
{
    std::sort(items.begin(), items.end());
    ui->listView->setUpdatesEnabled(false);
    for(int i = items.count() - 1; i > -1; --i)
        model->removeRow(items.at(i).row());
    ui->listView->setUpdatesEnabled(true);
}
