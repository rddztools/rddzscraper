#ifndef SCRAPENGINEMANAGER_H
#define SCRAPENGINEMANAGER_H
#ifndef CONFIG_H
#include "config.h"
#endif

#include <QDialog>
#include <QListWidgetItem>
#include <QInputDialog>
#include <QMessageBox>
#include <QMenu>
#include <QAction>

namespace Ui {
class ScrapEngineManager;
}

class ScrapEngineManager : public QDialog
{
    Q_OBJECT

public:
    ScrapEngineManager(QWidget *parent, Config *config);
    ~ScrapEngineManager();

private slots:
    void    on_okButton_clicked();
    void    on_abortButton_clicked();
    void    on_addGroupButton_clicked();
    void    on_deleteGroupButton_clicked();
    void    on_renameButton_clicked();

    void    enableDisableGroup();
    void    customContextMenuRequestedSlot(QPoint pos);

private:
    Ui::ScrapEngineManager *ui;
    Config *config;
    QFont _defaultParentFont;

     void populateTreeView();
     void addItemToListView(QString itemLabel);
};

#endif // SCRAPENGINEMANAGER_H
