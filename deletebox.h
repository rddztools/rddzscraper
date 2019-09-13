#ifndef DELETEBOX_H
#define DELETEBOX_H

#include <QDialog>

namespace Ui {
class deleteBox;
}

class deleteBox : public QDialog
{
    Q_OBJECT

public:
    explicit deleteBox(QWidget *parent = 0);
    ~deleteBox();


    void setColumnsDropDown(QStringList columnsName, bool forceReload=false);
    int getColumnIndex();
    QString     getColumnName();
    QString     getRemoveType();
    QVariant    getRemoveValue();
    bool        isRegex();

    void changeEvent(QEvent *e);
private:
    Ui::deleteBox *ui;
};

#endif // DELETEBOX_H
