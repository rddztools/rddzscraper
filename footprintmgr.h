#ifndef FOOTPRINTMGR_H
#define FOOTPRINTMGR_H
#include <QStringListModel>
#ifndef CONFIG_H
#include "config.h"
#endif

#include <QDialog>


//class MainWindow;

namespace Ui {
class FootprintMgr;
}

class FootprintMgr : public QDialog
{
    Q_OBJECT
    
public:
    FootprintMgr(QWidget *parent, Config *config);
    ~FootprintMgr();
     bool eventFilter(QObject *obj, QEvent *event);
    
private slots:
     void       accept();

private:
    Ui::FootprintMgr *ui;
    Config *config;
    QStringListModel *model;
    void    deleteItems(QModelIndexList items);
};

#endif // FOOTPRINTMGR_H
