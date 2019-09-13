#ifndef SEARCHREPLACEBOX_H
#define SEARCHREPLACEBOX_H

#include <QDialog>

namespace Ui {
class searchReplaceBox;
}

class searchReplaceBox : public QDialog
{
    Q_OBJECT

public:
    explicit searchReplaceBox(QWidget *parent = 0);
    ~searchReplaceBox();

    QString getSearchValue();
    QString getReplaceValue();
    bool    isRegex();
    bool    ignoreScheme();
    void    changeEvent(QEvent *e);


public slots:
    void    on_isRegex_toggled(bool status);

private:
    Ui::searchReplaceBox *ui;
};

#endif // SEARCHREPLACEBOX_H
