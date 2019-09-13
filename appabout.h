#ifndef APPABOUT_H
#define APPABOUT_H
#include <QDialog>
#include <QDebug>

namespace Ui {
class AppAbout;
}

class AppAbout : public QDialog
{
    Q_OBJECT
    
public:
    explicit AppAbout(QWidget *parent = 0);
    ~AppAbout();
    void    setVersion(QString version);

    
protected:
    void        changeEvent(QEvent *e);

private:
    Ui::AppAbout *ui;
};

#endif // APPABOUT_H
