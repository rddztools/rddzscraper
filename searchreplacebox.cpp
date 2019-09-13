#include "searchreplacebox.h"
#include "ui_searchreplacebox.h"

#include "qdebug.h"

searchReplaceBox::searchReplaceBox(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::searchReplaceBox)
{
    ui->setupUi(this);
}

searchReplaceBox::~searchReplaceBox()
{
    delete ui;
}

void searchReplaceBox::changeEvent(QEvent *e)
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

QString searchReplaceBox::getSearchValue()
{
    return ui->searchValue->text();
}

QString searchReplaceBox::getReplaceValue()
{
    return ui->replaceValue->text();

}

bool searchReplaceBox::isRegex()
{
    return ui->isRegex->isChecked();
}

bool searchReplaceBox::ignoreScheme()
{
    return ui->ignoreScheme->isChecked();
}

void searchReplaceBox::on_isRegex_toggled(bool status)
{
    if(status == true)
    {
        ui->ignoreScheme->setChecked(false);
        ui->ignoreScheme->setEnabled(false);
    }
    else
        ui->ignoreScheme->setEnabled(true);
}



