#include "deletebox.h"
#include "ui_deletebox.h"

#include "qdebug.h"

deleteBox::deleteBox(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::deleteBox)
{
    ui->setupUi(this);
}


deleteBox::~deleteBox()
{
    delete ui;
}

void deleteBox::changeEvent(QEvent *e)
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

void deleteBox::setColumnsDropDown(QStringList columnsName, bool forceReload)
{
    if(forceReload == true)
        ui->columnComboBox->clear();

    if(!ui->columnComboBox->count())
    ui->columnComboBox->addItems(columnsName);
}

int deleteBox::getColumnIndex()
{
    return ui->columnComboBox->currentIndex();
}

QString deleteBox::getColumnName()
{
    return ui->columnComboBox->currentText();
}

QString deleteBox::getRemoveType()
{
    return ui->filterButtonGroup->checkedButton()->objectName();
}

QVariant deleteBox::getRemoveValue()
{
    if(ui->filterButtonGroup->checkedButton()->objectName().startsWith("number"))
        return ui->inputValue->text().toDouble();
    else
        return ui->inputValue->text();
}

bool deleteBox::isRegex()
{
    return ui->useRegexCheckBox->isChecked();
}
