#include "appabout.h"
#include "ui_appabout.h"

AppAbout::AppAbout(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::AppAbout)
{
    ui->setupUi(this);
}

AppAbout::~AppAbout()
{
    delete ui;
}

void AppAbout::changeEvent(QEvent *e)
{
    QDialog::changeEvent(e);
    switch (e->type()) {
    case QEvent::LanguageChange:
        ui->retranslateUi(this);
        break;
    default:
        break;
    }
}

void AppAbout::setVersion(QString version)
{
    ui->scraperVersion->setText(version);
}
