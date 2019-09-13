#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QClipboard>
#include <QToolButton>
#include <QInputDialog>
#include <QMessageBox>
#include <QFileInfo>
#include <QDebug>
#include <QShortcut>

#ifdef Q_OS_WIN
#include <windows.h>
#endif
#ifdef Q_OS_LINUX
#include <unistd.h>
#endif

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    currentVersionTxt = APPVERSION;
    getBacklinksMethod = 0;
    errorDuringUpdate = false;
    _getDomainOnly = true;
    _configLoaded = false;
    _customNoDedup = false;

   this->updateDelimiters();

    // We load language file before UI
    QString locale = QLocale::system().name().section('_', 0, 0);
    QString lang;
    QSettings settings(QSettings::IniFormat, QSettings::UserScope,
                       "RDDZ_Scraper", "RDDZ_Scraper");
    bool writeConfig, windowInfo;

    writeConfig = (settings.childGroups().contains("Global")) ? false : true;
    windowInfo  = (settings.childGroups().contains("MainWindow")) ? true : false;
    settings.beginGroup("Global");
    if(!writeConfig)
        lang = settings.value("lang",locale).toString();
    else
    {
        settings.setValue("lang",locale);
        lang = locale;
    }
    settings.endGroup();
    settings.sync();

    QString currentVersion = currentVersionTxt;

    if(QSystemTrayIcon::isSystemTrayAvailable())
    {
        sysTrayIcon.setIcon(QIcon(":/icons/icons/rddzscraper-24.png"));
        sysTrayIcon.setToolTip("RDDZ Scraper");
        sysTrayIcon.setVisible(true);
    }


    // Loading StyleSheet
#ifdef Q_OS_MAC
    QFile styleSheet(":/styles/styles/default-macosx.css");
#else
    QFile styleSheet(":/styles/styles/default.css");
#endif

    if ( styleSheet.open(QIODevice::ReadOnly) )
    {
        QTextStream in(&styleSheet);
        QString style = in.readAll();

#ifdef Q_OS_MAC
        style.append("\n\n#proxiesButtonGroupBox > QPushButton {\nmargin-bottom:15px;\n}\n");
#endif
        qApp->setStyleSheet(style);
    }

    // We define color here
    this->okBrush.setColor(QColor(129,217,211));
    this->okBrush.setStyle(Qt::SolidPattern);
    this->errorBrush.setColor(QColor(255,114,95));
    this->errorBrush.setStyle(Qt::SolidPattern);
    this->redirBrush.setColor(QColor(255,234,115));
    this->redirBrush.setStyle(Qt::SolidPattern);


    ui->setupUi(this);
    appTranslator.load(":/lang/lang/rddzscraper_" + lang, QLibraryInfo::location(QLibraryInfo::TranslationsPath));
    qApp->installTranslator(&appTranslator);

    if(windowInfo)
    {
        restoreGeometry(settings.value("MainWindow/geometry").toByteArray());
        restoreState(settings.value("MainWindow/state").toByteArray());
    }

    QString forceDefaultSe;
    if(qApp->arguments().size() > 1)
    {
        QCommandLineParser cmdLineParser;
        QCommandLineOption setDefaultSe("se", QCoreApplication::translate("main", "Define the default scrap engine at startup"),QCoreApplication::translate("main", "Scrap Engine Name"));
        cmdLineParser.addOption(setDefaultSe);
        cmdLineParser.parse(qApp->arguments());
        forceDefaultSe = cmdLineParser.value("se");
    }
    else
        forceDefaultSe = "";
    config  = new Config(this,forceDefaultSe);


    // Add QLabel for Elapsed Time
    QLabel *elapsedTimeStatusLabel = new QLabel();
    elapsedTimeLabel = new QLabel();
    elapsedTimeLabel->setText("00:00:00");
    elapsedTimeStatusLabel->setText(tr("Last operation : "));
    ui->statusBar->addPermanentWidget(elapsedTimeStatusLabel);
    ui->statusBar->addPermanentWidget(elapsedTimeLabel);
    lastOperationTimer = new QTimer(this);
    QObject::connect(this->lastOperationTimer, SIGNAL(timeout()),this,SLOT(updateLastOperationTime()));


    ui->menuBar->clear();
    ui->listResult->installEventFilter(this);
    ui->SESelect->installEventFilter(this);
    ui->proxiesList->installEventFilter(this);


    scraptable = new scrapTable(this);
    scraptable->visibleRows = 100;

    // We load the scrape engines
    config->loadSE();

    scraper = new Scraper(this,config,scraptable);
    footprintMgr = new FootprintMgr(nullptr,config);
    seMgr = new ScrapEngineManager(nullptr,config);
    network = nullptr;
    proxyTimer = nullptr;
    this->updateViewTimer = new QTimer(this);

    QStringList ahrefsBacklinksMode, mozBacklinksMode;
    ahrefsBacklinksMode << "exact" << "domain" << "subdomains" << "prefix";
    mozBacklinksMode << "page_to_page" << "page_to_subdomain" << "page_to_domain" << "domain_to_page" << "domain_to_subdomain" << "domain_to_domain";

    ui->backlinksCheckMode->insertItems(0, ahrefsBacklinksMode);
    ui->backlinksMozCheckMode->insertItems(0,mozBacklinksMode);

    this->loadConfiguration();
    this->loadNewsTabContent();
    this->initProxiesList();
    QPair<int,QString> currentSE;
    currentSE.first = ui->SESelect->currentData(Qt::AccessibleTextRole).toInt();
    currentSE.second = ui->SESelect->currentData(Qt::WhatsThisRole).toString();
    config->setCurrentSe(currentSE);
    this->updateMenuOptions();

    this->buildScrapTabResults("scrap");

    this->scrapExtensions.append(".txt");
    this->scrapExtensions.append(".csv");
    ui->footprintComboBox->setCurrentIndex(ui->footprintComboBox->findText(config->getLastFootprint()));
    ui->footprintComboBox->setToolTip(config->getLastFootprint());
    ui->proxiesGroupBox->setEnabled(false);
    if(config->getDisableWarnProxies() == true)
        ui->scrapWithoutProxyWarnCheckBox->setChecked(true);
    if(config->useProxyRotating() == true)
        ui->useProxyRotatingCheckBox->setChecked(true);
    if(config->proxy_enable() == true)
    {
        ui->proxiesGroupBox->setEnabled(true);
        ui->useProxiesCheckBox->setChecked(true);
    }
    ui->customSeColumnHeaderTable->setMinimumHeight(70);

    qRegisterMetaType<QMap<int,QList<QVariant> > >("QMap<int,QList<QVariant>");

    ui->listResult->setContextMenuPolicy(Qt::CustomContextMenu);


    QObject::connect(ui->listResult,SIGNAL(doubleClicked(QModelIndex)),this,SLOT(openUrlWidget(QModelIndex)));
    QObject::connect(this,SIGNAL(restoreCursor()),this,SLOT(restoreCursorSlot()));
    QObject::connect(this,SIGNAL(setBusyCursor()),this,SLOT(setBusyCursorSlot()));
    QObject::connect(this->scraptable,SIGNAL(enableCursor()),this,SLOT(restoreCursorSlot()));
    QObject::connect(this->scraptable,SIGNAL(disableCursor()),this,SLOT(setBusyCursorSlot()));
    QObject::connect(this,SIGNAL(noDuplicateUrl(bool,QString,QString)),this,SLOT(noDuplicateUrlSlot(bool,QString,QString)));
    QObject::connect(this,SIGNAL(deleteItemsFromTable(int,bool,bool)),this,SLOT(deleteItemsFromTableSlot(int,bool,bool)));
    QObject::connect(this,SIGNAL(enableButtonsSignal()), this, SLOT(enableButtons()));
    QObject::connect(this,SIGNAL(disableButtonsSignal()), this, SLOT(disableButtons()));
    QObject::connect(this,SIGNAL(updateNbUrlSignal()),this,SLOT(updateNbUrl()));
    QObject::connect(this,SIGNAL(displayMsg(QString)),this,SLOT(displayMsgSlot(QString)));
    QObject::connect(ui->listResult,SIGNAL(customContextMenuRequested(QPoint)),this,SLOT(customContextMenuRequestedSlot(QPoint)));
    QObject::connect(this->scraper, SIGNAL(updateScrapTableLayout()), this->scraptable, SLOT(updateLayout()));
    QObject::connect(this, SIGNAL(restoreSorting()), this, SLOT(restoreSortingSlot()));
    // For MacOSX only, set the scrollperitem mode
    ui->listResult->setVerticalScrollMode(QAbstractItemView::ScrollPerItem);
    QObject::connect(ui->listResult->verticalScrollBar(), SIGNAL(valueChanged(int)), scraptable, SLOT(scrollBarValueChanged(int)));

    this->on_SESelect_activated(ui->SESelect->currentText());
    this->on_customSeComboBox_currentIndexChanged(ui->customSeComboBox->currentIndex());

    this->buildToolbar();
    this->buildMenuLanguage();
    new QShortcut(QKeySequence::Quit, this, SLOT(close()));

    // We set QMenu for Get Backlinks button
    QMenu *backlinksMenu = new QMenu();
    QList<QAction *> backlinksActions;
    QAction *backlinksDomainAction, *backlinksSubdomainAction, *backlinksUrlAction;

    backlinksDomainAction = new QAction(tr("Domain"), this);
    backlinksDomainAction->setData("Domain");
    backlinksActions.append(backlinksDomainAction);

    backlinksSubdomainAction = new QAction(tr("Subdomain"), this);
    backlinksSubdomainAction->setData("Subdomain");
    backlinksActions.append(backlinksSubdomainAction);

    backlinksUrlAction = new QAction(tr("URL"), this);
    backlinksUrlAction->setData("URL");
    backlinksActions.append(backlinksUrlAction);

    QObject::connect(backlinksDomainAction,SIGNAL(triggered()),this,SLOT(getToolBacklinksDomain()));
    QObject::connect(backlinksSubdomainAction,SIGNAL(triggered()),this,SLOT(getToolBacklinksSubomain()));
    QObject::connect(backlinksUrlAction,SIGNAL(triggered()),this,SLOT(getToolBacklinksUrl()));


    backlinksMenu->addActions(backlinksActions);
    backlinksMenu->setMinimumWidth(215);
    ui->toolBacklinksButton->setMenu(backlinksMenu);


    // We set QMenu for Transfer to custom1 button
    QMenu *transferToCustom1Menu = new QMenu();
    QList<QAction *> transferToCustom1Actions;
    QAction *transferToCustom1Action, *transferNonExistingToCustom1Action;

    transferToCustom1Action = new QAction(tr("Append"), this);
    transferToCustom1Action->setData("Append");
    transferToCustom1Action->setToolTip("Append results to custom1 field. Remove duplicate entries.");
    transferToCustom1Actions.append(transferToCustom1Action);

    transferNonExistingToCustom1Action = new QAction(tr("Diff"), this);
    transferNonExistingToCustom1Action->setData("Diff");
    transferNonExistingToCustom1Action->setToolTip(tr("Transfer the data that don't exist in custom1 field"));
    transferToCustom1Actions.append(transferNonExistingToCustom1Action);

    QObject::connect(transferToCustom1Action,SIGNAL(triggered()),this,SLOT(transferToCustom1()));
    QObject::connect(transferNonExistingToCustom1Action,SIGNAL(triggered()),this,SLOT(transferNonExistingToCustom1()));

    transferToCustom1Menu->addActions(transferToCustom1Actions);
    transferToCustom1Menu->setMinimumWidth(215);
    ui->transfertToCustom1Button->setMenu(transferToCustom1Menu);

    // We set QMenu for Remove Duplicate button
    QMenu *removeDuplicateMenu = new QMenu();
    QList<QAction *> removeDuplicateActions;
    QAction *removeDuplicateDomainAction, *removeDuplicateURLAction;

    removeDuplicateDomainAction = new QAction(tr("Domain"), this);
    removeDuplicateDomainAction->setData("Domain");
    removeDuplicateActions.append(removeDuplicateDomainAction);

    removeDuplicateURLAction = new QAction(tr("URL"), this);
    removeDuplicateURLAction->setData("URL");
    removeDuplicateActions.append(removeDuplicateURLAction);

    QObject::connect(removeDuplicateDomainAction,SIGNAL(triggered()),this,SLOT(removeDuplicateDomainsAction()));
    QObject::connect(removeDuplicateURLAction,SIGNAL(triggered()),this,SLOT(removeDuplicateUrlAction()));


    removeDuplicateMenu->addActions(removeDuplicateActions);
    removeDuplicateMenu->setMinimumWidth(100);
    ui->removeDuplicateButton->setMenu(removeDuplicateMenu);

    // We connect actions for import and export
    QObject::connect(ui->actionImportScrapFile, SIGNAL(triggered()), this, SLOT(importScrapButtonEmulate()));
    QObject::connect(ui->actionSaveUrlOnly, SIGNAL(triggered()), this, SLOT(stdScrapExportButtonEmulate()));
    QObject::connect(ui->actionFull_Export, SIGNAL(triggered()), this, SLOT(fullScrapExportButtonEmulate()));
}


MainWindow::~MainWindow()
{
    QSettings settings(QSettings::IniFormat, QSettings::UserScope,
                       "RDDZ_Scraper", "RDDZ_Scraper");
    settings.setValue("MainWindow/geometry", saveGeometry());
    settings.setValue("MainWindow/state", saveState());

    // We set the last used engine
    config->setLastUsedEngine(ui->SESelect->currentData(Qt::WhatsThisRole).toString());

    delete config;
    delete scraper;
    delete network;
    delete footprintMgr;
    delete ui;
}

void MainWindow::changeEvent(QEvent *e)
{

    QMainWindow::changeEvent(e);
    switch (e->type()) {
    case QEvent::LanguageChange:
        ui->retranslateUi(this);
        //this->buildToolbar();
        break;
    default:
        break;
    }
}

/////////////////////////////
/* All publics functions */
///////////////////////////

bool MainWindow::eventFilter(QObject *obj, QEvent *event)
{
    QString objName = obj->objectName();
    QKeyEvent *c = static_cast<QKeyEvent *>(event);


    // We catch events on listResult
    if(objName == "listResult")
    {
        if(event->type() == QEvent::KeyPress)
        {
            if (c->matches(QKeySequence::Copy))
            {
                this->doFullCopy();
                return true;
            }
            else if(c->matches(QKeySequence::Paste))
            {
                this->doPaste();
                return true;
            }
            else if(c->matches(QKeySequence::Delete))
            {
                this->on_removeSelectedUrlButton_clicked();
                return true;
            }
        }
    }
    else if (obj->objectName() == "SESelect" && ui->autoSaveCustomsOptionCheckbox->isChecked())
    {
        if(event->type() == QEvent::MouseButtonPress)
        {
            this->on_saveConfig_clicked();
            return true;
        }
    }
    else if(obj->objectName() == "proxiesList")
    {
        if(event->type() == QEvent::KeyPress)
        {
            if(c->matches(QKeySequence::Delete))
            {
                this->on_proxiesDeleteSelectedButton_clicked();
                return true;
            }
        }
        else if (c->matches(QKeySequence::Copy))
        {
            int currentCol = 0;
            int totalColumn = ui->proxiesList->model()->columnCount() - 1;

            QList<QModelIndex> selectionRange = ui->proxiesList->selectionModel()->selectedIndexes();
            QMutableListIterator<QModelIndex> listIterator(selectionRange);
            QString line, currentItemStr, selection;
            QStringList lines;


            listIterator.toFront();
            line.clear();
            while(listIterator.hasNext())
            {
                listIterator.next();

                currentItemStr = listIterator.value().data().toString();
                currentCol = listIterator.value().column();

                if(currentCol > 0 && currentCol < 4)
                    line.append(":");
                line.append(currentItemStr);
                if(currentCol == totalColumn)
                {
                    lines.append(line);
                    line.clear();
                }
            }
            selection = lines.join("\n");

            QApplication::clipboard()->clear();
            QApplication::clipboard()->setText(selection);
            return true;
        }
    }
    return false;
}


void MainWindow::doPaste()
{
    QString     clipB;
    QByteArray content;

    clipB   = QApplication::clipboard()->text();
    content.append(clipB);
    this->updateScrapList(content);
}

void MainWindow::doCopy(QList<int> columns, bool selectedOnly)
{
    int previousRow = 0;
    int currentRow,currentCol = 0;
    int lastIndexList = 0;
    bool enterLoop = true;
    QString selection,line;
    QStringList lines;

    if(selectedOnly == true)
    {
        QModelIndexList indexList = ui->listResult->selectionModel()->selectedIndexes();

        QString currentItemStr;


        lastIndexList = indexList.lastIndexOf(indexList.last());

        line.clear();
        foreach (QModelIndex index, indexList)
        {

            currentItemStr = this->scraptable->getItemByIndex(index).toString();
            currentRow = index.row();
            currentCol = index.column();

            if (columns.contains(-1) || columns.contains(currentCol))
            {
                if(!previousRow && enterLoop == true)
                    previousRow = currentRow;

                if(previousRow == currentRow && enterLoop == false)
                    line.append("|");
                else if(previousRow == currentRow && enterLoop == true)
                    enterLoop = false;
                else
                {
                    lines.append(line);
                    line.clear();
                }
                line.append(currentItemStr.replace("|","%7C"));

                previousRow = currentRow;
            }

            if(indexList.indexOf(index) == lastIndexList)
            {
                lines.append(line);
            }
        }
        selection = lines.join("\n");
    }
    else
    {
        QVariantList itemsList = this->scraptable->getItemsByColumn(columns.at(0));
        foreach (QVariant item, itemsList) {
            lines.append(item.toString());
        }
        selection = lines.join("\n");
    }

    QApplication::clipboard()->clear();
    QApplication::clipboard()->setText(selection);
}

void MainWindow::doFullCopy()
{
    QList<int> columns;
    columns.append(-1);

    this->doCopy(columns);
}

// Change since 1.7.6, we copy only selected items in columns
void MainWindow::doUrlCopy()
{
    QList<int> columns;
    columns.append(this->scraptable->columnClicked);

    this->doCopy(columns);
}

void MainWindow::doUrlPrCopy()
{
    QList<int> columns;
    columns.append(this->getColumnIndexByName("urlCol"));
    columns.append(this->getColumnIndexByName("prCol"));

    this->doCopy(columns);
}

void MainWindow::doColumnCopy()
{
    QList<int> columns;
    columns.append(this->scraptable->columnClicked);

    this->doCopy(columns, false);
}

void MainWindow::emulateSelectAll()
{
    ui->listResult->selectAll();
}

Network *MainWindow::getNetwork()
{
    return this->network;
}

int MainWindow::getNbUrl(int col)
{
    QStringList urls;
    urls.clear();

    QMapIterator<int, QVariantList> scrapResIterator(scraptable->scrapResults);

    if(col >= 0)
    {
        scrapResIterator.toFront();
        while(scrapResIterator.hasNext())
        {
            scrapResIterator.next();
            if(scrapResIterator.value().at(col).toString().isEmpty())
            {
                QUrl testUrl(scrapResIterator.value().at(scraptable->columnIndexes.value("urlCol")).toString());
                if(testUrl.isValid() && !testUrl.isRelative())
                    urls.append(scrapResIterator.value().at(scraptable->columnIndexes.value("urlCol")).toString());
            }
        }
    }
    return urls.size();
}

QStringList MainWindow::getRedirUrls()
{
    QStringList urls;

    QMapIterator<int, QVariantList> scrapResIterator(scraptable->scrapResults);

    scrapResIterator.toFront();
    while(scrapResIterator.hasNext())
    {
        scrapResIterator.next();

        if(scrapResIterator.value().at(scraptable->columnIndexes.value("httpCol")).toInt() > 300 && scrapResIterator.value().at(scraptable->columnIndexes.value("httpCol")).toInt() < 400)
        {
            QUrl testUrl(scrapResIterator.value().at(scraptable->columnIndexes.value("urlCol")).toString());
            if(testUrl.isValid() && !testUrl.isRelative())
                urls.append(scrapResIterator.value().at(scraptable->columnIndexes.value("urlCol")).toString());
        }
    }
    return urls;
}

QStringList MainWindow::getUrls()
{
    QStringList urls;

    return urls;
}

int MainWindow::getColumnIndexByName(QString columnName)
{
    return this->scraptable->columnIndexes.value(columnName,-1);
}

QString MainWindow::getCurrentAction()
{
    return this->action;
}

QChar MainWindow::getCsvSeparator()
{
    return _csvSeparator;
}

QChar MainWindow::getCsvEnclosedBy()
{
    return _csvDelimiter;
}


int MainWindow::getColumnCount()
{
    int colCount;
    QAbstractItemModel* tableModel = ui->listResult->model();
    colCount = tableModel->columnCount();
    return colCount;
}

void MainWindow::insertColumn(QString columnName, QString columnTooltip, QString stdName, int width, int startAt)
{
    int localStart = (startAt == -1) ? scraptable->colCount : startAt;
    this->scraptable->columnIndexes.insert(stdName, scraptable->colCount);
    this->scraptable->headerColSize.insert(localStart,width);
    scraptable->setHeaderData(localStart,Qt::Horizontal,columnName, Qt::DisplayRole);
    scraptable->setHeaderData(localStart,Qt::Horizontal,columnTooltip, Qt::ToolTipRole);
    scraptable->colCount = scraptable->colCount+1;

    // We enable network buttons here
    if(stdName == "httpCol")
    {
        ui->httpButton->setEnabled(true);
        ui->resolveRedirectButton->setEnabled((true));
    }
    else if(stdName == "dfCol")
        ui->checkDofollowNofollow->setEnabled(true);
    else if(stdName == "blCol")
        ui->toolBacklinksButton->setEnabled(true);
    else if(stdName == "oblCol")
        ui->getOblButton->setEnabled(true);
    else if(stdName == "platformCol")
        ui->getPlatformButton->setEnabled(true);
    else if(stdName == "ipCol")
        ui->getIpAddrButton->setEnabled(true);
    else if(stdName == "linkAliveCol")
        ui->linkAliveButton->setEnabled(true);
    else if(stdName == "domainAvailableCol")
        ui->whoisButton->setEnabled(true);


}

void MainWindow::buildScrapTabResults(QString action, QString backlinkAPI, QStringList scrapFileHeader)
{
    int columnStartIndex;
    bool scrapFileHeaderContainsUrl;

    // We have to check for previous action. If it's the same, return
    if(action  == "scrap" && action == this->action && scrapFileHeader.isEmpty())
        return;

    scraptable->clearScrapResultsHash();
    scraptable->redirRetry = 0;
    scraptable->previousRedirLeft = 0;
    scrapFileHeaderContainsUrl = false;

    // Cause Crash on windaube (Qt 5.2.0, not tested on older version)
    //    scraptable->clearHeaderData();

    // We set all col value to -1
    this->scraptable->resetColumnIndexes();

    this->maxCol = columnStartIndex = 0;
    this->action = action;

    // Before building scraptableresults, we disable all netowrk button
    this->disableAllNetworkButtons();

    if(action == "scrap" || action == "customScrap")
    {
        scraptable->currentTableModel = "scrap";
        scraptable->clearHeaderData();

        if(action == "customScrap" && !scrapFileHeader.isEmpty())
        {
            int i;

            for(i=0;i<scrapFileHeader.size();i++)
            {
                if(scrapFileHeader.at(i).toLower() == "url")
                {
                    this->insertColumn("URL", "URL", "urlCol",320);
                    scrapFileHeaderContainsUrl = true;
                }
                else
                    this->insertColumn(scrapFileHeader.at(i),scrapFileHeader.at(i),ui->customSeColumnHeaderTable->horizontalHeaderItem(i)->text());
            }
        }
        if(action == "scrap" || (action == "customScrap" && scrapFileHeaderContainsUrl))
        {
            if(scrapFileHeaderContainsUrl == false)
                this->insertColumn("URL", "URL", "urlCol",320);
            if(this->config->readGeneralConfig("StdColumn","HTTP",true).toBool() || scrapFileHeader.contains("HTTP Status"))
                this->insertColumn("HTTP Status", tr("Get HTTP status code for URL"), "httpCol",80);
            if(this->config->readGeneralConfig("StdColumn","DF",true).toBool() || scrapFileHeader.contains("Dofollow (%)"))
                this->insertColumn("Dofollow (%)", tr("Percent of dofollow links in the page"), "dfCol",100);

            if(this->config->getConfigurationValue("blApiForScrap","Free") != "Free")
            {
                if(this->config->readGeneralConfig("StdColumn","BL",true).toBool() || scrapFileHeader.contains("Backlinks"))
                {
                    this->insertColumn("Backlinks", tr("Get number of backlinks for URL/Domain/Subdomain"), "blCol");


                    if (this->config->getConfigurationValue("blApiForScrap","Free") == "Ahrefs")
                    {
                        this->insertColumn("URL Rank", tr("Return Ahrefs URL Rank"), "ahrefsUrlRankCol");
                        this->insertColumn("Domain Rank", tr("Return Ahrefs Domain Rank"), "ahrefsDomainRankCol");
                    }
                    else if (this->config->getConfigurationValue("blApiForScrap","Free") == "Majestic" || scrapFileHeader.contains("") || this->config->getConfigurationValue("blApiForScrap","Free") == "SEObserver")
                    {
                        this->insertColumn("RD", tr("Get number of referring domains"), "rdCol");
                        this->insertColumn("CF", tr("Get Citation Flow for URL"), "cfCol");
                        this->insertColumn("TF", tr("Get Trust Flow for URL"), "tfCol");
                        this->insertColumn("TTFT", tr("Topical Trust Flow Topic for URL"), "ttftCol",150);
                        this->insertColumn("TTFV", tr("Topical Trust Flow Value for URL"), "ttfvCol");
                        this->insertColumn("Status", tr("Status of the item into the Majestic index"), "majesticStatusCol");
                        this->insertColumn("LCD", tr("Last Crawl Date for URL"), "lcdCol");

                    }
                    else if (this->config->getConfigurationValue("blApiForScrap","Free") == "Moz")
                    {
                        this->insertColumn("MozRank", tr("Get the MozRank"), "mozRankCol",100);
                        this->insertColumn("PA", tr("Get Page Authority"), "paCol");
                        this->insertColumn("DA", tr("Get Domain Authority"), "daCol");
                    }
                }
            }

            if(this->config->readGeneralConfig("StdColumn","OBL",true).toBool() || scrapFileHeader.contains("OBL"))
                this->insertColumn("OBL", tr("Get total of outbound links for URL"), "oblCol");

            if(this->config->readGeneralConfig("StdColumn","Platform",true).toBool() || scrapFileHeader.contains("Platform"))
            {
                this->insertColumn("Platform Category", tr("Get platform category type for URL"), "platformCatCol",200);
                this->insertColumn("Platform", tr("Get platform type for URL"), "platformCol",100);
            }
            if(this->config->readGeneralConfig("StdColumn","IP",true).toBool() || scrapFileHeader.contains("IP Address"))
                this->insertColumn("IP Address", tr("Get IP address for each URL"), "ipCol",80);
            if(this->config->readGeneralConfig("StdColumn","LinkAlive",true).toBool() || scrapFileHeader.contains("Link Alive"))
                this->insertColumn("Link Alive", tr("Check if the link is still alive."), "linkAliveCol");
            if(this->config->readGeneralConfig("StdColumn","DomainAvailable",true).toBool() || scrapFileHeader.contains("Domain Available"))
                this->insertColumn("Domain Available", tr("Check if the domain is available."), "domainAvailableCol",150);
        }
    }
    else if (action == "backlinks")
    {
        if(backlinkAPI == "Moz")
        {

            scraptable->currentTableModel = "Moz";
            scraptable->clearHeaderData();

            //qDebug() << scrapFileHeader;

            if(scrapFileHeader.isEmpty() || scrapFileHeader.contains(tr("Backlinks for")) || scrapFileHeader.contains("Backlinks for"))
                this->insertColumn(tr("Backlinks for"), tr("Display the url/domain/subdomain for which we are retrieving backlinks"), "blForCol",200);
            this->insertColumn("URL", "URL", "urlCol",320);
            if(this->config->readGeneralConfig("mozApiColumn","Anchor",true).toBool() || scrapFileHeader.contains(tr("Anchor")) || scrapFileHeader.contains("Anchor"))
                this->insertColumn(tr("Anchor"), tr("Anchor"), "anchorCol",200);
            if(this->config->readGeneralConfig("StdColumn","HTTP",true).toBool() || scrapFileHeader.contains("HTTP Status"))
                this->insertColumn("HTTP Status", tr("Get HTTP status code for URL"), "httpCol",80);
            if(this->config->readGeneralConfig("StdColumn","DF",true).toBool() || scrapFileHeader.contains("Dofollow (%)"))
                this->insertColumn("Dofollow (%)", tr("Percent of dofollow links in the page"), "dfCol",100);
            if(this->config->readGeneralConfig("mozApiColumn","Backlinks",true).toBool() || scrapFileHeader.contains("Backlinks"))
                this->insertColumn("Backlinks", tr("Get number of backlinks for URL/Domain/Subdomain"), "blCol");
            // Specific Backlink section
            if(this->config->readGeneralConfig("mozApiColumn","Mozrank",true).toBool() || scrapFileHeader.contains("MozRank"))
                this->insertColumn("MozRank", tr("Get the MozRank"), "mozRankCol",100);
            if(this->config->readGeneralConfig("mozApiColumn","PA",true).toBool() || scrapFileHeader.contains("PA"))
                this->insertColumn("PA", tr("Get Page Authority"), "paCol");
            if(this->config->readGeneralConfig("mozApiColumn","DA",true).toBool() || scrapFileHeader.contains("DA"))
                this->insertColumn("DA", tr("Get Domain Authority"), "daCol");
            // End of specific backink section
            if(this->config->readGeneralConfig("StdColumn","OBL",true).toBool() || scrapFileHeader.contains("OBL"))
                this->insertColumn("OBL", tr("Get total of outbound links for URL"), "oblCol");
            if(this->config->readGeneralConfig("StdColumn","Platform",true).toBool() || scrapFileHeader.contains("Platform"))
            {
                this->insertColumn("Platform Category", tr("Get platform category type for URL"), "platformCatCol",200);
                this->insertColumn("Platform", tr("Get platform type for URL"), "platformCol",200);
            }
            if(this->config->readGeneralConfig("StdColumn","IP",true).toBool() || scrapFileHeader.contains("IP Address"))
                this->insertColumn("IP Address", tr("Get IP address for each URL"), "ipCol",80);
            if(this->config->readGeneralConfig("StdColumn","LinkAlive",true).toBool() || scrapFileHeader.contains("Link Alive"))
                this->insertColumn("Link Alive", tr("Check if the link is still alive."), "linkAliveCol");
            if(this->config->readGeneralConfig("StdColumn","DomainAvailable",true).toBool() || scrapFileHeader.contains("Domain Available"))
                this->insertColumn("Domain Available", tr("Check if the domain is available."), "domainAvailableCol",150);
        }
        else if(backlinkAPI == "Ahrefs")
        {

            scraptable->currentTableModel = "Ahrefs";
            scraptable->clearHeaderData();

            if(scrapFileHeader.isEmpty() || scrapFileHeader.contains(tr("Backlinks for")) || scrapFileHeader.contains("Backlinks for"))
                this->insertColumn(tr("Backlinks for"), tr("Display the url/domain/subdomain for which we are retrieving backlinks"), "blForCol",200);
            this->insertColumn("URL", "URL", "urlCol",320);
            if(this->config->readGeneralConfig("ahrefsApiColumn","Anchor",true).toBool() || scrapFileHeader.contains(tr("Anchor")) || scrapFileHeader.contains("Anchor"))
                this->insertColumn(tr("Anchor"), tr("Anchor"), "anchorCol",200);
            if(this->config->readGeneralConfig("ahrefsApiColumn","TargetUrl",true).toBool() || scrapFileHeader.contains(tr("Target URL")) || scrapFileHeader.contains("Target URL"))
                this->insertColumn(tr("Target URL"), tr("URL of the page the backlink is pointing to"), "targetUrlCol",200);
            if(this->config->readGeneralConfig("ahrefsApiColumn","LinkType",true).toBool() || scrapFileHeader.contains(tr("Link type")) || scrapFileHeader.contains("Link type"))
                this->insertColumn(tr("Link type"), tr("Link type"), "linkTypeCol",150);
            if(this->config->readGeneralConfig("StdColumn","HTTP",true).toBool() || scrapFileHeader.contains("HTTP Status"))
                this->insertColumn("HTTP Status", tr("Get HTTP status code for URL"), "httpCol",80);
            if(this->config->readGeneralConfig("StdColumn","DF",true).toBool() || scrapFileHeader.contains("Dofollow (%)"))
                this->insertColumn("Dofollow (%)", tr("Percent of dofollow links in the page"), "dfCol",100);
            if(this->config->readGeneralConfig("ahrefsApiColumn","Nofollow",true).toBool() || scrapFileHeader.contains("Nofollow"))
                this->insertColumn("Nofollow", tr("Indicates if the backlink is nofollow or not"), "nofollowCol");
            if(this->config->readGeneralConfig("ahrefsApiColumn","UrlRank",true).toBool() || scrapFileHeader.contains("URL Rank"))
                this->insertColumn("URL Rank", tr("Return Ahrefs URL Rank"), "ahrefsUrlRankCol");
            if(this->config->readGeneralConfig("ahrefsApiColumn","DomainRank",true).toBool() || scrapFileHeader.contains("Domain Rank"))
                this->insertColumn("Domain Rank", tr("Return Ahrefs Domain Rank"), "ahrefsDomainRankCol");
            if(this->config->readGeneralConfig("StdColumn","OBL",true).toBool() || scrapFileHeader.contains("OBL"))
                this->insertColumn("OBL", tr("Get total of outbound links for URL"), "oblCol");
            if(this->config->readGeneralConfig("StdColumn","Platform",true).toBool() || scrapFileHeader.contains("Platform"))
            {
                this->insertColumn("Platform Category", tr("Get platform category type for URL"), "platformCatCol",200);
                this->insertColumn("Platform", tr("Get platform type for URL"), "platformCol",200);
            }
            if(this->config->readGeneralConfig("StdColumn","IP",true).toBool() || scrapFileHeader.contains("IP Address"))
                this->insertColumn("IP Address", tr("Get IP address for each URL"), "ipCol",80);
            if(this->config->readGeneralConfig("StdColumn","LinkAlive",true).toBool() || scrapFileHeader.contains("Link Alive"))
                this->insertColumn("Link Alive", tr("Check if the link is still alive."), "linkAliveCol");
            if(this->config->readGeneralConfig("StdColumn","DomainAvailable",true).toBool() || scrapFileHeader.contains("Domain Available"))
                this->insertColumn("Domain Available", tr("Check if the domain is available."), "domainAvailableCol",150);
        }
        else
        {
            scraptable->currentTableModel = "Majestic";
            scraptable->clearHeaderData();

            if(scrapFileHeader.isEmpty() || scrapFileHeader.contains(tr("Backlinks for")) || scrapFileHeader.contains("Backlinks for"))
                this->insertColumn(tr("Backlinks for"), tr("Display the url/domain/subdomain for which we are retrieving backlinks"), "blForCol",200);
            this->insertColumn("URL", tr("URL of the page where the backlink is found"), "urlCol",320);
            if(this->config->readGeneralConfig("majesticApiColumn","Anchor",true).toBool() || scrapFileHeader.contains(tr("Anchor")) || scrapFileHeader.contains("Anchor"))
                this->insertColumn(tr("Anchor"), tr("Anchor"), "anchorCol",200);
            if(this->config->readGeneralConfig("majesticApiColumn","LinkType",true).toBool() || scrapFileHeader.contains(tr("Link type")) || scrapFileHeader.contains("Link type"))
                this->insertColumn(tr("Link type"), tr("Link type"), "linkTypeCol",150);
            if(this->config->readGeneralConfig("majesticApiColumn","TargetUrl",true).toBool() || scrapFileHeader.contains(tr("Target URL")) || scrapFileHeader.contains("Target URL"))
                this->insertColumn(tr("Target URL"), tr("URL of the page the backlink is pointing to"), "targetUrlCol",200);
            if(this->config->readGeneralConfig("majesticApiColumn","Deleted",true).toBool() || scrapFileHeader.contains(tr("Deleted")) || scrapFileHeader.contains("Deleted"))
                this->insertColumn(tr("Deleted"), tr("Indicates if the link was deleted or not. 0 => link alive, 1 => link deleted"), "linkDeletedCol");
            if(this->config->readGeneralConfig("StdColumn","HTTP",true).toBool() || scrapFileHeader.contains("HTTP Status"))
                this->insertColumn("HTTP Status", tr("Get HTTP status code for URL"), "httpCol",80);
            if(this->config->readGeneralConfig("StdColumn","DF",true).toBool() || scrapFileHeader.contains("Dofollow (%)"))
                this->insertColumn("Dofollow (%)", tr("Percent of dofollow links in the page"), "dfCol",100);
            if(this->config->readGeneralConfig("majesticApiColumn","Nofollow",true).toBool() || scrapFileHeader.contains("Nofollow"))
                this->insertColumn("Nofollow", tr("Indicates if the backlink is Nofollow. 1 is nofollow, 0 is dofollow"), "nofollowCol");
            // From 1.6.0, we remove the backlinks col. Too much API cost and time retrieval
            /*             if(this->config->readGeneralConfig("majesticApiColumn","Backlinks",true).toBool() || scrapFileHeader.contains("Backlinks"))
          this->insertColumn("Backlinks", tr("Get number of backlinks for URL/Domain/Subdomain"), "blCol"); */
            if(this->config->readGeneralConfig("majesticApiColumn","CF",true).toBool() || scrapFileHeader.contains("CF"))
                this->insertColumn("CF", tr("Get Citation Flow for URL"), "cfCol");
            if(this->config->readGeneralConfig("majesticApiColumn","TF",true).toBool() || scrapFileHeader.contains("TF"))
                this->insertColumn("TF", tr("Get Trust Flow for URL"), "tfCol");
            if(this->config->readGeneralConfig("majesticApiColumn","TTFT",true).toBool() || scrapFileHeader.contains("TTFT"))
                this->insertColumn("TTFT", tr("Topical Trust Flow Topic for URL"), "ttftCol",150);
            if(this->config->readGeneralConfig("majesticApiColumn","TTFV",true).toBool() || scrapFileHeader.contains("TTFV"))
                this->insertColumn("TTFV", tr("Topical Trust Flow Value for URL"), "ttfvCol");

            if(this->config->readGeneralConfig("majesticApiColumn","FID",true).toBool() || scrapFileHeader.contains("FID"))
                this->insertColumn("FID", tr("First Indexed Date for URL"), "fidCol");
            if(this->config->readGeneralConfig("majesticApiColumn","LSD",true).toBool() || scrapFileHeader.contains("LSD"))
                this->insertColumn("LSD", tr("Last Seen Date for URL"), "lsdCol");

            if(this->config->readGeneralConfig("StdColumn","OBL",true).toBool() || scrapFileHeader.contains("OBL"))
                this->insertColumn("OBL", tr("Get total of outbound links for URL"), "oblCol");
            if(this->config->readGeneralConfig("StdColumn","Platform",true).toBool() || scrapFileHeader.contains("Platform"))
            {
                this->insertColumn("Platform Category", tr("Get platform category type for URL"), "platformCatCol",200);
                this->insertColumn("Platform", tr("Get platform type for URL"), "platformCol",200);
            }
            if(this->config->readGeneralConfig("StdColumn","IP",true).toBool() || scrapFileHeader.contains("IP Address"))
                this->insertColumn("IP Address", tr("Get IP address for each URL"), "ipCol",80);
            if(this->config->readGeneralConfig("StdColumn","LinkAlive",true).toBool() || scrapFileHeader.contains("Link Alive"))
                this->insertColumn("Link Alive", tr("Check if the link is still alive."), "linkAliveCol");
            if(this->config->readGeneralConfig("StdColumn","DomainAvailable",true).toBool() || scrapFileHeader.contains("Domain Available"))
                this->insertColumn("Domain Available", tr("Check if the domain is available."), "domainAvailableCol",150);
        }

    }
    ui->listResult->setModel(scraptable);
    ui->listResult->setSortingEnabled(true);
    ui->listResult->verticalHeader()->setVisible(true);
    ui->listResult->verticalHeader()->setDefaultSectionSize(20);
    ui->listResult->horizontalHeader()->setStretchLastSection(true);
    ui->listResult->resizeColumnsToContents();
}

void MainWindow::loadNewsTabContent()
{
    QString lang;
    QVariant confLang;

    confLang = config->readGeneralConfig("Global","lang");
    lang = confLang.toString();


    QNetworkAccessManager *qnam = new QNetworkAccessManager();
    QNetworkReply *reply = qnam->get(QNetworkRequest(QUrl("https://raw.githubusercontent.com/rddztools/rddzscraper/master/CHANGELOG.md")));

    QEventLoop loop;
    QObject::connect(reply, SIGNAL(finished()), &loop, SLOT(quit()));
    loop.exec();
    const QByteArray data=reply->readAll();
    int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    reply->deleteLater();

    if(statusCode != 200)
    {
        ui->newsTextEdit->setText(tr("News server unreachable"));
        return;
    }
    ui->newsTextEdit->setText(data);
}

void MainWindow::loadConfiguration()
{
    QHash<QString,QString> hashDefault;
    int i;

    config->loadConfiguration();
    hashDefault.clear();

    // Create tmp dir if not present
    QDir tmpStackDir(QApplication::applicationDirPath());
    tmpStackDir.mkdir("tmp");

    // And init tab Header for expert mode
    QStringList headerLabels;
    for(i=0;i<35;i++)
    {
        headerLabels.append("Col"+QString::number(i+1));
    }
    ui->customSeColumnHeaderTable->setColumnCount(headerLabels.size());
    ui->customSeColumnHeaderTable->setRowCount(1);
    ui->customSeColumnHeaderTable->setHorizontalHeaderLabels(headerLabels);

    // global config
    if(config->getConfigurationValue("silentMode").isEmpty())
        hashDefault.insert("silentMode","false");
    if(config->getConfigurationValue("minimalLog").isEmpty())
        hashDefault.insert("minimalLog","false");
    if(config->getConfigurationValue("logFile").isEmpty())
        hashDefault.insert("logFile","false");
    if(config->getConfigurationValue("htmlStack").isEmpty())
        hashDefault.insert("htmlStack","false");
    if(config->getConfigurationValue("autoSaveScrap").isEmpty())
        hashDefault.insert("autoSaveScrap","true");
    if(config->getConfigurationValue("lineBreak").isEmpty())
    {
#ifdef Q_OS_WIN
        lineBreak = "\r\n";
        hashDefault.insert("lineBreak","0");
#else
        lineBreak = "\n";
        hashDefault.insert("lineBreak","1");
#endif
    }
    if(config->getConfigurationValue("txtSeparator").isEmpty())
        hashDefault.insert("txtSeparator","|");
    if(config->getConfigurationValue("csvSeparator").isEmpty())
        hashDefault.insert("csvSeparator",",");
    if(config->getConfigurationValue("csvDelimiter").isEmpty())
        hashDefault.insert("csvDelimiter","\"");


    if(config->getConfigurationValue("disableAutoSort").isEmpty())
        hashDefault.insert("disableAutoSort","0");
    if(config->getConfigurationValue("disableDedupe").isEmpty())
        hashDefault.insert("disableDedupe","0");
    if(config->getConfigurationValue("autosaveSeConfig").isEmpty())
        hashDefault.insert("autosaveSeConfig","false");
    if(config->getConfigurationValue("keepCustom1OnChange").isEmpty())
        hashDefault.insert("keepCustom1OnChange","false");
    if(config->getConfigurationValue("keepCustom2OnChange").isEmpty())
        hashDefault.insert("keepCustom2OnChange","false");
    if(config->getConfigurationValue("keepScrapResults").isEmpty())
        hashDefault.insert("keepScrapResults","false");
    if(config->getConfigurationValue("backlinksActiveService").isEmpty())
        hashDefault.insert("backlinksActiveService","Moz");
    if(config->getConfigurationValue("ahrefsApiKey").isEmpty())
        hashDefault.insert("ahrefsApiKey","None");
    if(config->getConfigurationValue("backlinksCheckMode").isEmpty())
        hashDefault.insert("backlinksCheckMode","Domain");
    if(config->getConfigurationValue("majesticIndex").isEmpty())
        hashDefault.insert("majesticIndex","fresh");
    if(config->getConfigurationValue("mozBacklinksMode").isEmpty())
        hashDefault.insert("mozBacklinksMode","page_to_domain");


    // On set les values pour le scrap
    if (config->getConfigurationValue("scraperTimeout").isEmpty())
        hashDefault.insert("scraperTimeout","30");
    if (config->getConfigurationValue("pageloadingTimeout").isEmpty())
        hashDefault.insert("pageloadingTimeout","90");
    if (config->getConfigurationValue("statuscodeTimeout").isEmpty())
        hashDefault.insert("statuscodeTimeout","30");
    if (config->getConfigurationValue("dofollowTimeout").isEmpty())
        hashDefault.insert("dofollowTimeout","30");
    if (config->getConfigurationValue("backlinksTimeout").isEmpty())
        hashDefault.insert("backlinksTimeout","20");
    if (config->getConfigurationValue("oblTimeout").isEmpty())
        hashDefault.insert("oblTimeout","20");
    if (config->getConfigurationValue("proxiesTestTimeout").isEmpty())
        hashDefault.insert("proxiesTestTimeout","30");
    if(config->getConfigurationValue("retryOnTimeout").isEmpty())
        hashDefault.insert("retryOnTimeout","false");


    if(!hashDefault.isEmpty())
        config->updateConfiguration(hashDefault);

    lineBreak = (config->getConfigurationValue("lineBreak") == "1") ? "\n" : "\r\n";

    // From 1.7.4, let the user choose this settings into configuration tab
    txtDelimiter = config->getConfigurationValue("txtSeparator").at(0);
    _csvSeparator = config->getConfigurationValue("csvSeparator").at(0);
    _csvDelimiter = config->getConfigurationValue("csvDelimiter").at(0);


    // We load the whois API
    ui->whoisApiComboBox->setCurrentText(config->getConfigurationValue("whoisApiService"));
    ui->whoisAPIKey->setText(config->getConfigurationValue("whoisApiKey"+QString::number(ui->whoisApiComboBox->currentIndex())));

    // We load backlinks Services
    ui->backlinksCheckMode->setCurrentIndex(ui->backlinksCheckMode->findText(config->getConfigurationValue("backlinksCheckMode")));
    ui->backlinksMozCheckMode->setCurrentIndex(ui->backlinksMozCheckMode->findText(config->getConfigurationValue("mozBacklinksMode")));
    ui->majesticIndexComboBox->setCurrentIndex(ui->majesticIndexComboBox->findText(config->getConfigurationValue("majesticIndex")));
    if(config->getConfigurationValue("ahrefsApiKey") != "None")
        ui->ahrefsApiKeyEdit->setText(config->getConfigurationValue("ahrefsApiKey"));
    else
        ui->ahrefsApiKeyEdit->setText("");
    if(config->getConfigurationValue("majesticAccessToken") != "None")
        ui->majesticAccessTokenEdit->setText(config->getConfigurationValue("majesticAccessToken"));
    else
        ui->mozAccessIdEdit->setText("");
    if(config->getConfigurationValue("mozAccessId") != "None")
        ui->mozAccessIdEdit->setText(config->getConfigurationValue("mozAccessId"));
    else
        ui->mozSecretKeyEdit->setText("");
    if(config->getConfigurationValue("mozSecretKey") != "None")
        ui->mozSecretKeyEdit->setText(config->getConfigurationValue("mozSecretKey"));
    else
        ui->mozSecretKeyEdit->setText("");
    if(config->getConfigurationValue("seobserverApiKey") != "None")
        ui->seobserverApiEdit->setText(config->getConfigurationValue("seobserverApiKey"));
    else
        ui->seobserverApiEdit->setText("");


    int majesticReferringIndex = (config->getConfigurationValue("majesticReferring","-1").toInt() == -1) ? 0 : 1;
    ui->majesticReferringComboBox->setCurrentIndex(majesticReferringIndex);
    ui->majesticRetrievingComboBox->setCurrentIndex(config->getConfigurationValue("majesticRetrievingMethod","0").toInt())   ;

    if(config->getConfigurationValue("backlinksActiveService") == "Ahrefs")
        ui->backlinksAhrefsOptionRadio->setChecked(true);
    else if(config->getConfigurationValue("backlinksActiveService") == "Majestic")
        ui->backlinksMajesticOptionRadio->setChecked(true);
    else if(config->getConfigurationValue("backlinksActiveService") == "Moz")
        ui->backlinksMozOptionRadio->setChecked(true);
    else if(config->getConfigurationValue("backlinksActiveService") == "SEObserver")
        ui->backlinksSeobserverOptionRadio->setChecked(true);

    ui->lineBreakComboBox->insertItem(0,"Windows (\\r\\n)");
    ui->lineBreakComboBox->insertItem(1,"Mac/Linux (\\n)");


    ui->dedupDomainBasedOnComboBox->setCurrentIndex(ui->dedupDomainBasedOnComboBox->findText(config->getConfigurationValue("freeDedupOn","PR")));
    ui->ahrefsDedupDomainComboBox->setCurrentIndex(ui->ahrefsDedupDomainComboBox->findText(config->getConfigurationValue("ahrefsDedupOn","PR")));
    ui->majesticDedupDomainComboBox->setCurrentIndex(ui->majesticDedupDomainComboBox->findText(config->getConfigurationValue("majesticDedupOn","PR")));
    ui->mozDedupDomainComboBox->setCurrentIndex(ui->mozDedupDomainComboBox->findText(config->getConfigurationValue("mozDedupOn","PR")));


    // And we load global configuration
    ui->silentModeOptionCheckbox->setChecked(QVariant(config->getConfigurationValue("silentMode")).toBool());
    ui->minimalLogCheckBox->setChecked(QVariant(config->getConfigurationValue("minimalLog")).toBool());
    ui->logFileCheckBox->setChecked(QVariant(config->getConfigurationValue("logFile")).toBool());
    ui->htmlStackCheckBox->setChecked(QVariant(config->getConfigurationValue("htmlStack")).toBool());
    ui->scrapResultsAutoSaveCheckBox->setChecked(QVariant(config->getConfigurationValue("autoSaveScrap")).toBool());
    ui->lineBreakComboBox->setCurrentIndex(QVariant(config->getConfigurationValue("lineBreak")).toInt());
    ui->textSeparatorLineEdit->setText(config->getConfigurationValue("txtSeparator"));
    ui->csvSeparatorLineEdit->setText(config->getConfigurationValue("csvSeparator"));
    ui->csvDelimiterLineEdit->setText(config->getConfigurationValue("csvDelimiter"));



    // Scrap configuration
    ui->disableAutoSort->setChecked(QVariant(config->getConfigurationValue("disableAutoSort")).toBool());
    ui->disableDedupe->setChecked(QVariant(config->getConfigurationValue("disableDedupe")).toBool());
    ui->autoSaveCustomsOptionCheckbox->setChecked(QVariant(config->getConfigurationValue("autosaveSeConfig")).toBool());
    ui->keepCustom1OptionCheckbox->setChecked(QVariant(config->getConfigurationValue("keepCustom1OnChange")).toBool());
    ui->keepCustom2OptionCheckbox->setChecked(QVariant(config->getConfigurationValue("keepCustom2OnChange")).toBool());
    ui->keepScrapResultCheckbox->setChecked(QVariant(config->getConfigurationValue("keepScrapResults")).toBool());
    ui->scrapBacklinkServiceComboBox->setCurrentText(this->blApiForScrap = config->getConfigurationValue("blApiForScrap","Free"));

    // Set retryonTimeout
    ui->retryOnTimeout->setChecked(QVariant(config->getConfigurationValue("retryOnTimeout","false")).toBool());


    // Timeout
    ui->scrapTimeoutEdit->setText(config->getConfigurationValue("scraperTimeout"));
    ui->pageLoadingTimeoutEdit->setText(config->getConfigurationValue("pageloadingTimeout"));
    ui->statusCodeTimeoutEdit->setText(config->getConfigurationValue("statuscodeTimeout"));
    ui->dofollowTimeoutEdit->setText(config->getConfigurationValue("dofollowTimeout"));
    ui->backlinksTimeoutEdit->setText(config->getConfigurationValue("backlinksTimeout"));
    ui->oblTimeoutEdit->setText(config->getConfigurationValue("oblTimeout"));
    ui->testProxiesTimeoutEdit->setText(config->getConfigurationValue("proxiesTestTimeout"));

    // Cols
    ui->displayColumnHttp->setChecked(this->config->readGeneralConfig("StdColumn","HTTP",true).toBool());
    ui->displayColumnDofollow->setChecked(this->config->readGeneralConfig("StdColumn","DF",true).toBool());
    ui->displayColumnBacklinks->setChecked(this->config->readGeneralConfig("StdColumn","BL",true).toBool());
    ui->displayColumnObl->setChecked(this->config->readGeneralConfig("StdColumn","OBL",true).toBool());
    ui->displayColumnPlatform->setChecked(this->config->readGeneralConfig("StdColumn","Platform",true).toBool());
    ui->displayColumnIP->setChecked(this->config->readGeneralConfig("StdColumn","IP",true).toBool());
    ui->displayColumnLinkAlive->setChecked(this->config->readGeneralConfig("StdColumn","LinkAlive",true).toBool());
    ui->displayColumnDomainAvailable->setChecked(this->config->readGeneralConfig("StdColumn","DomainAvailable",true).toBool());

    ui->displayMozColumnAnchor->setChecked(this->config->readGeneralConfig("mozApiColumn","mozApiColumn",true).toBool());
    ui->displayMozColumnBacklinks->setChecked(this->config->readGeneralConfig("mozApiColumn","mozBacklinksColumn",true).toBool());
    ui->displayMozColumnMozrank->setChecked(this->config->readGeneralConfig("mozApiColumn","Mozrank",true).toBool());
    ui->displayMozColumnPa->setChecked(this->config->readGeneralConfig("mozApiColumn","PA",true).toBool());
    ui->displayMozColumnDa->setChecked(this->config->readGeneralConfig("mozApiColumn","DA",true).toBool());

    ui->displayAhrefsColumnAnchor->setChecked(this->config->readGeneralConfig("ahrefsApiColumn","Anchor",true).toBool());
    ui->displayAhrefsColumnTargetUrl->setChecked(this->config->readGeneralConfig("ahrefsApiColumn","TargetUrl",true).toBool());
    ui->displayAhrefsColumnLinkType->setChecked(this->config->readGeneralConfig("ahrefsApiColumn","LinkType",true).toBool());
    ui->displayAhrefsColumnUrlRank->setChecked(this->config->readGeneralConfig("ahrefsApiColumn","UrlRank",true).toBool());
    ui->displayAhrefsColumnDomainRank->setChecked(this->config->readGeneralConfig("ahrefsApiColumn","DomainRank",true).toBool());
    ui->displayAhrefsColumnNofollow->setChecked(this->config->readGeneralConfig("ahrefsApiColumn","Nofollow",true).toBool());

    ui->displayMajesticColumnAnchor->setChecked(this->config->readGeneralConfig("majesticApiColumn","Anchor",true).toBool());
    ui->displayMajesticColumnLinkType->setChecked(this->config->readGeneralConfig("majesticApiColumn","LinkType",true).toBool());
    ui->displayMajesticColumnTargetUrl->setChecked(this->config->readGeneralConfig("majesticApiColumn","TargetUrl",true).toBool());
    ui->displayMajesticColumnDeleted->setChecked(this->config->readGeneralConfig("majesticApiColumn","Deleted",true).toBool());
    ui->displayMajesticColumnNofollow->setChecked(this->config->readGeneralConfig("majesticApiColumn","Nofollow",true).toBool());
    ui->displayMajesticColumnBacklinks->setChecked(this->config->readGeneralConfig("majesticApiColumn","Backlinks",true).toBool());
    ui->displayMajesticColumnCf->setChecked(this->config->readGeneralConfig("majesticApiColumn","CF",true).toBool());
    ui->displayMajesticColumnTf->setChecked(this->config->readGeneralConfig("majesticApiColumn","TF",true).toBool());
    ui->displayMajesticColumnTtft->setChecked(this->config->readGeneralConfig("majesticApiColumn","TTFT",true).toBool());
    ui->displayMajesticColumnTtfv->setChecked(this->config->readGeneralConfig("majesticApiColumn","TTFV",true).toBool());


    ui->httpButton->setEnabled(this->config->readGeneralConfig("StdColumn","HTTP",true).toBool());
    ui->resolveRedirectButton->setEnabled(this->config->readGeneralConfig("StdColumn","HTTP",true).toBool());
    ui->checkDofollowNofollow->setEnabled(this->config->readGeneralConfig("StdColumn","DF",true).toBool());
    ui->toolBacklinksButton->setEnabled(this->config->readGeneralConfig("StdColumn","BL",true).toBool());
    ui->getOblButton->setEnabled(this->config->readGeneralConfig("StdColumn","OBL",true).toBool());
    ui->getPlatformButton->setEnabled(this->config->readGeneralConfig("StdColumn","Platform",true).toBool());
    ui->getIpAddrButton->setEnabled(this->config->readGeneralConfig("StdColumn","IP",true).toBool());
    ui->linkAliveButton->setEnabled(this->config->readGeneralConfig("StdColumn","LinkAlive",true).toBool());
    ui->whoisButton->setEnabled(this->config->readGeneralConfig("StdColumn","DomainAvailable",true).toBool());

    _configLoaded = true;
}



/////////////////////////////
/* All top menu functions */
///////////////////////////

void MainWindow::updateMenuOptions()
{
    QString autoBacklinkMethod = config->getConfigurationValue("autogetBacklinksMethod");

    if(config->getDisableWarnProxies() == true)
        ui->actionDisable_Warning_for_scraping_without_proxies->setChecked(true);
    if(config->getAutoRemoveBadUrl() == true)
        ui->actionAuto_remove_bad_urls_after_http_status_check->setChecked(true);
    if(config->getAutoResolveRedir() == true)
        ui->actionAuto_resolve_redirections->setChecked(true);
    if(config->getAutoGetStatus() == true)
        ui->actionAutorun_HTTP_Status_check_after_scrap->setChecked(true);
    if(config->getAutoDF() == true)
        ui->actionAutorun_check_dofollow->setChecked(true);
    if(config->getAutoOBL() == true)
        ui->actionAutorun_outbound_links->setChecked(true);
    if(config->getAutoIP() == true)
        ui->actionAutorun_retrieve_IP->setChecked(true);
    if(config->proxy_enable() == true)
        ui->actionUse_proxies->setChecked(true);
    if(config->getTestProxiesOnGoogle() == true)
        ui->testProxiesOnGoogleCheckBox->setChecked(true);
    if(config->getAutoBL() == true)
    {
        if(!autoBacklinkMethod.isEmpty())
        {
            int backlinkMethod = autoBacklinkMethod.toInt();
            if (backlinkMethod == 0)
                ui->actionAutorunBacklinksURL->setChecked(true);
            else if (backlinkMethod == 1)
                ui->actionAutorunBacklinksSubdomain->setChecked(true);
            else if (backlinkMethod == 2)
                ui->actionAutorunBacklinksDomain->setChecked(true);
        }
    }
}

void MainWindow::on_aboutQT_triggered()
{
    QMessageBox::aboutQt(this,tr("About QT"));
}

void MainWindow::on_about_triggered()
{
    aboutWin.setVersion("<html><head/><body style=\"font-family:'Sans'; font-size:12px; font-weight:400; font-style:normal;\"><p align=\"left\" style=\"margin-top:0px; margin-bottom:0px; margin-left:0px; margin-right:0px; -qt-block-indent:0; text-indent:0px;\"><span style=\" font-weight:600;\">"+currentVersionTxt+"</span></p></body></html>");
    aboutWin.exec();
}


void MainWindow::on_actionFootprint_Manager_triggered()
{
    footprintMgr->exec();
}

void MainWindow::on_actionXPath_Expression_tester_triggered()
{
    QString xpiName;

#ifdef Q_OS_WIN
    xpiName = "RDDZ_XPI.exe";
#else
    xpiName = "RDDZ_XPI";
#endif

    // We check if binary is present
    QFile xpiFile(QDir::toNativeSeparators(QApplication::applicationDirPath()+"/"+xpiName));
    if(!xpiFile.exists())
    {
        emit displayMsg(tr("The binary %1 is not present. Please compile or download it and put the binary into %2").arg(xpiName,xpiFile.fileName()));
    }
    process.startDetached("\""+QDir::toNativeSeparators(QApplication::applicationDirPath())+"/"+xpiName+"\"");
}

void MainWindow::launchSEManager()
{
    this->seMgr->exec();
    config->loadSE();
}

void MainWindow::on_actionAuto_remove_bad_urls_after_http_status_check_triggered(bool checked)
{
    config->updateAutomation("autoRemoveBadUrl",checked);
}

void MainWindow::on_actionAuto_resolve_redirections_triggered(bool checked)
{
    config->updateAutomation("autoResolveRedir",checked);
}


void MainWindow::on_actionAutorun_HTTP_Status_check_after_scrap_triggered(bool checked)
{
    config->updateAutomation("autoGetStatus",checked);
}

void MainWindow::on_actionAutorun_check_dofollow_triggered(bool checked)
{
    config->updateAutomation("autoDFStatus",checked);
}


void MainWindow::on_actionAutorunBacklinksDomain_triggered(bool checked)
{
    QHash<QString,QString> configValues;

    this->clearAutoCheckedBacklinksAction();
    ui->actionAutorunBacklinksDomain->setChecked(checked);
    getBacklinksMethod = 2;
    configValues.insert("autogetBacklinksMethod","2");
    config->updateConfiguration(configValues);
    config->updateAutomation("autoBacklinks",checked);
}

void MainWindow::on_actionAutorunBacklinksSubdomain_triggered(bool checked)
{
    QHash<QString,QString> configValues;

    this->clearAutoCheckedBacklinksAction();
    ui->actionAutorunBacklinksSubdomain->setChecked(checked);
    getBacklinksMethod = 1;
    configValues.insert("autogetBacklinksMethod","1");
    config->updateConfiguration(configValues);
    config->updateAutomation("autoBacklinks",checked);
}

void MainWindow::on_actionAutorunBacklinksURL_triggered(bool checked)
{
    QHash<QString,QString> configValues;

    this->clearAutoCheckedBacklinksAction();
    ui->actionAutorunBacklinksURL->setChecked(checked);
    getBacklinksMethod = 0;
    configValues.insert("autogetBacklinksMethod","0");
    config->updateConfiguration(configValues);
    config->updateAutomation("autoBacklinks",checked);
}


//void MainWindow::on_actionAutorun_backlinks_triggered(bool checked)
//{
//    config->updateAutomation("autoBacklinks",checked);
//}

void MainWindow::on_actionAutorun_outbound_links_triggered(bool checked)
{
    config->updateAutomation("autoObl",checked);
}

void MainWindow::on_actionAutorun_retrieve_IP_triggered(bool checked)
{
    config->updateAutomation("autoIpAddr",checked);
}

void MainWindow::buildMenuLanguage()
{
    QMenu *languageMenu;
    QActionGroup *languageActionGroup;
    QAction *actionFr, *actionEn;
    QString lang;
    QVariant confLang;

    confLang = config->readGeneralConfig("Global","lang");
    lang = confLang.toString();

    ui->menuLanguage->clear();
    languageMenu = ui->menuLanguage;
    languageActionGroup = new QActionGroup(this);

    connect(languageActionGroup, SIGNAL(triggered(QAction *)),
            this, SLOT(switchLanguage(QAction *)));

    actionEn = new QAction(tr("English"), this);
    actionEn->setData("en");
    actionEn->setIcon(QIcon(":/lang/icons/United-States.png"));
    actionEn->setIconVisibleInMenu(true);
    languageMenu->addAction(actionEn);
    languageActionGroup->addAction(actionEn);
    actionEn->setCheckable(true);
    if(lang == "en")
        actionEn->setChecked(true);


    actionFr = new QAction(tr("French"), this);
    actionFr->setData("fr");
    actionFr->setIcon(QIcon(":/lang/icons/France.png"));
    actionFr->setIconVisibleInMenu(true);
    languageMenu->addAction(actionFr);
    languageActionGroup->addAction(actionFr);
    actionFr->setCheckable(true);
    if(lang == "fr")
        actionFr->setChecked(true);

}

void MainWindow::switchLanguage(QAction *action)
{
    QString locale = action->data().toString();
    config->writeGeneralConfig("Global","lang",locale);

    appTranslator.load(":/lang/lang/rddzscraper_" + locale);
    ui->retranslateUi(this);


    this->buildToolbar();
    this->buildMenuLanguage();
}

/////////////////////////////
/* All toolbar functions */
///////////////////////////



void MainWindow::buildToolButton(QString iconPath, QString title, QString tooltip, QMenu *menu, QString objectName, bool isEnabled, bool showMenu)
{
    QToolButton *optionsButton;
    QString backgroundUrl;
    QStringList dontDisplayArrowDown;
    //    QAction *action;


    dontDisplayArrowDown << "footprintToolButton" << "xpiToolButton" << "seManagerToolButton" ;
    optionsButton = new QToolButton(this);
    optionsButton->setIcon(QIcon(iconPath));
    optionsButton->setIconSize(QSize(24,24));
    optionsButton->setText(title);
    optionsButton->setToolTip(tooltip);
    optionsButton->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
    backgroundUrl = ":/menu/icons/menu/empty.png";
    if(showMenu == true && (!dontDisplayArrowDown.contains(objectName)))
    {
        optionsButton->setMenu(menu);
        optionsButton->setPopupMode(QToolButton::InstantPopup);
        backgroundUrl = ":/menu/icons/menu/down-arrow.png";
    }
    else if (showMenu == true && objectName == "footprintToolButton")
    {
        QObject::connect(optionsButton,SIGNAL(clicked()),ui->actionFootprint_Manager, SIGNAL(triggered()));
    }
    else if (showMenu == true && objectName == "xpiToolButton")
    {
        QObject::connect(optionsButton,SIGNAL(clicked()),ui->actionXPath_Expression_tester, SIGNAL(triggered()));
    }
    else if (showMenu == true && objectName == "seManagerToolButton")
    {
        QObject::connect(optionsButton,SIGNAL(clicked()),this, SLOT(launchSEManager()));
    }
    else if(showMenu == false && objectName == "quitToolButton")
        QObject::connect(optionsButton,SIGNAL(clicked()),ui->quit, SIGNAL(triggered()));
    //    else if(showMenu == false && objectName == "footprintToolButton")
    //        QObject::connect(optionsButton,SIGNAL(clicked()),ui->actionFootprint_Manager, SIGNAL(triggered()));
    optionsButton->setStyleSheet("background-color:qlineargradient(spread:pad, x1:0.5, y1:0, x2:0.5, y2:1, stop:0 rgba(176, 176, 176, 255), stop:1 rgba(135, 135, 135, 255));padding-top:1px;background:url("+backgroundUrl+") no-repeat right middle;");
    optionsButton->setEnabled(isEnabled);
    optionsButton->setAutoRaise(false);
    optionsButton->setMinimumWidth(80);
    optionsButton->setObjectName(QString::fromUtf8(objectName.toStdString().c_str()));
    ui->mainToolBar->addWidget(optionsButton);

    //    if(objectName=="quitToolButton")
    //        ui->mainToolBar->insertSeparator(action);
}

void MainWindow::buildToolbar()
{
    ui->mainToolBar->clear();
    QMenu *emptymenu = new QMenu();

    this->buildToolButton(":/menu/icons/menu/import.png",tr("Import"),tr("Import scrap result, proxies or Search engine configuration"),ui->menuImport, "importToolButton");
    this->buildToolButton(":/menu/icons/menu/export.png",tr("Export"),tr("Export your scrap results in different format"),ui->menuSave_scrap_results, "exportToolButton");
    this->buildToolButton(":/menu/icons/menu/options.png",tr("Options"),tr("Define your scrap options here"),ui->menuOptions, "optionsToolButton");
    this->buildToolButton(":/menu/icons/menu/footprint.png",tr("Footprints"),tr("Footprints manager"),emptymenu, "footprintToolButton");
    this->buildToolButton(":/menu/icons/menu/semanager.png",tr("Scrap engines"),tr("Scrap Engines Manager"),emptymenu, "seManagerToolButton");
    this->buildToolButton(":/menu/icons/menu/rddzxpi.png",tr("XPath tester"),tr("XPath Expression tester"),emptymenu, "xpiToolButton");
    this->buildToolButton(":/menu/icons/menu/lang.png",tr("Language"),tr("Select application language"),ui->menuLanguage, "langToolButton");

    this->buildToolButton(":/icons/icons/rddzscraper-24.png",tr("About"),tr("About RDDZ Scraper and QT"),ui->menuAbout, "aboutToolButton");
    this->buildToolButton(":/menu/icons/menu/quit.png",tr("Quit"),tr("Leave the App :("),ui->menuAbout, "quitToolButton", true,false);

    ui->mainToolBar->setVisible(true);
    ui->mainToolBar->setContextMenuPolicy(Qt::PreventContextMenu);

}


/////////////////////////////////////////////////////////
/* All Over the tabs function (scrap, backlinks,Abort)*/
///////////////////////////////////////////////////////

void MainWindow::on_footprintComboBox_activated(QString fp)
{
    ui->footprintComboBox->setToolTip(fp);
}

void MainWindow::on_SendFootprint_clicked()
{
    int response,custom1Start,custom1End,custom1Step,custom2Start,custom2End,custom2Step;
    QString custom1Str, custom2Str;
    QStringList custom1, custom2;
    QString scrapurl;
    QHash<QString,QString> hashFp;
    QHash<QString, QVariant> scrapEngineConfig;

    QRegExp rxIntLoop("\\{%intloop:(\\d+):(\\d+):(\\d+)%\\}");
    QRegExp rxFP("\\{%footprint%\\}"),rxC1("\\{%custom1(::encoded)?%\\}"), rxC2("\\{%custom2(::encoded)?%\\}");

    this->initScrapClass();
    ui->taskProgressBar->reset();

    scraper->clearCustomList();
    scraper->threadTimeout = config->getConfigurationValue("scraperTimeout").toInt();

    scrapEngineConfig = config->getScrapEngineConfigurationHash(ui->SESelect->currentData(Qt::AccessibleTextRole).toInt());

    custom1Str      = ui->custom1List->toPlainText().trimmed();
    custom1         = custom1Str.split("\n");
    custom2Str      = ui->custom2List->toPlainText().trimmed();
    custom2         = custom2Str.split("\n");

    if(config->proxy_enable() == false)
    {
        if(config->getDisableWarnProxies() == false)
        {
            response = QMessageBox::warning(this,tr("Proxies information"),tr("Be careful, you're about to scrap without proxies !!\nWould you like to continue ?"),QMessageBox::Yes | QMessageBox::No);
            if(response == QMessageBox::Yes);
            else if(response == QMessageBox::No)
                return;
        }
    }

    // We check footprint and customs, if they are used in Expert mode
    scrapurl = scrapEngineConfig.value("url").toString();
    if(rxFP.indexIn(scrapurl) != -1 && ui->footprintComboBox->currentText().isEmpty())
    {
        QMessageBox::information(this,tr("Empty footprint"),tr("Your footprint is empty, but you use it on your scrap configuration.Please enter a footprint"));
        return;
    }
    if(rxC1.indexIn(scrapurl) != -1 && ui->custom1List->toPlainText().isEmpty())
    {
        QMessageBox::information(this,tr("Empty custom"),tr("Your custom1 is empty, but you use it on your scrap configuration.Please enter a custom1"));
        return;
    }
    if(rxC2.indexIn(scrapurl) != -1 && ui->custom2List->toPlainText().isEmpty())
    {
        QMessageBox::information(this,tr("Empty custom"),tr("Your custom2 is empty, but you use it on your scrap configuration.Please enter a custom2"));
        return;
    }


    if(!ui->footprintComboBox->currentText().isEmpty() && ui->footprintComboBox->findText(ui->footprintComboBox->currentText()) == -1)
    {
        ui->footprintComboBox->addItem(ui->footprintComboBox->currentText());
        config->insertFootprint(ui->footprintComboBox->currentText());
    }
    if (config->getConfigurationValue("keepScrapResults") == "false")
    {
        urlHash.clear();
        ui->nbUrls->clear();
        scraptable->clearScrapResultsHash();

    }

    scraper->setFootprint(QUrl::toPercentEncoding(ui->footprintComboBox->currentText()));
    // We set the last used Footprint
    hashFp.insert("lastFootprint", ui->footprintComboBox->currentText());
    config->updateConfiguration(hashFp);

    scraper->setSeSelected(scrapEngineConfig);
    config->updateSeList(scrapEngineConfig,scrapEngineConfig.value("display_name").toString(),"newdefault");

    this->disableButtons();

    // Case we custom1 only
    if(!custom1Str.isEmpty() && custom2Str.isEmpty())
    {
        // We have an intloop
        if(rxIntLoop.indexIn(custom1Str) != -1)
        {
            custom1Start    = rxIntLoop.cap(1).toInt();
            custom1End      = rxIntLoop.cap(2).toInt();
            custom1Step     = rxIntLoop.cap(3).toInt();
            custom1.clear();
            for(;custom1Start<=custom1End;custom1Start+=custom1Step)
                custom1 << QString::number(custom1Start);
        }
        if(rxC1.indexIn(scrapurl) == -1 && rxC1.indexIn(scrapEngineConfig.value("xpath_content").toString()) == -1 && rxC1.indexIn(scrapEngineConfig.value("prefix").toString()) == -1 && rxC1.indexIn(scrapEngineConfig.value("suffix").toString()) == -1 && rxC1.indexIn(scrapEngineConfig.value("postArray").toString()) == -1)
            custom1.clear();
        scraper->setCustomLoopList(custom1,QStringList());

    }
    // custom2 only
    else if(custom1Str.isEmpty() && !custom2Str.isEmpty())
    {
        // We have an intloop
        if(rxIntLoop.indexIn(custom2Str) != -1)
        {
            custom2Start    = rxIntLoop.cap(1).toInt();
            custom2End      = rxIntLoop.cap(2).toInt();
            custom2Step     = rxIntLoop.cap(3).toInt();
            custom2.clear();
            for(;custom2Start<=custom2End;custom2Start+=custom2Step)
                custom2 << QString::number(custom2Start);
        }
        if(rxC2.indexIn(scrapurl) == -1 && rxC2.indexIn(scrapEngineConfig.value("xpath_content").toString()) == -1 && rxC2.indexIn(scrapEngineConfig.value("prefix").toString()) == -1 && rxC2.indexIn(scrapEngineConfig.value("suffix").toString()) == -1 && rxC2.indexIn(scrapEngineConfig.value("postArray").toString()) == -1)
            custom2.clear();
        scraper->setCustomLoopList(custom2, QStringList());
    }
    // Case with custom1 and custom2
    else if(!custom1Str.isEmpty() && !custom2Str.isEmpty())
    {
        // We have an intloop
        if(rxIntLoop.indexIn(custom1Str) != -1)
        {
            custom1Start    = rxIntLoop.cap(1).toInt();
            custom1End      = rxIntLoop.cap(2).toInt();
            custom1Step     = rxIntLoop.cap(3).toInt();
            custom1.clear();
            for(;custom1Start<=custom1End;custom1Start+=custom1Step)
                custom1 << QString::number(custom1Start);
        }
        if(rxIntLoop.indexIn(custom2Str) != -1)
        {
            custom2Start    = rxIntLoop.cap(1).toInt();
            custom2End      = rxIntLoop.cap(2).toInt();
            custom2Step     = rxIntLoop.cap(3).toInt();
            custom2.clear();

            for(;custom2Start<custom2End;custom2Start+=custom2Step)
                custom2 << QString::number(custom2Start);
        }

        if(rxC1.indexIn(scrapurl) == -1 && rxC1.indexIn(scrapEngineConfig.value("xpath_content").toString()) == -1 && rxC1.indexIn(scrapEngineConfig.value("prefix").toString()) == -1 && rxC1.indexIn(scrapEngineConfig.value("suffix").toString()) == -1 && rxC1.indexIn(scrapEngineConfig.value("postArray").toString()) == -1)
            custom1.clear();
        if(rxC2.indexIn(scrapurl) == -1 && rxC2.indexIn(scrapEngineConfig.value("xpath_content").toString()) == -1 && rxC2.indexIn(scrapEngineConfig.value("prefix").toString()) == -1 && rxC2.indexIn(scrapEngineConfig.value("suffix").toString()) == -1 && rxC2.indexIn(custom1Str) == -1 && rxC2.indexIn(scrapEngineConfig.value("postArray").toString()) == -1)
            custom2.clear();
        scraper->setCustomLoopList(custom1, custom2);
    }


    tools.clearUrlProxies();
    ui->customRatioLabel->clear();

    // We create the file for storing the results
    if(config->getConfigurationValue("autoSaveScrap") == "true")
    {
        QDateTime autoSaveResultsFileDate(QDateTime::currentDateTime());
        scraper->autoSaveScrapResultsFile.setFileName(QApplication::applicationDirPath()+"/results/scrapSession-"+autoSaveResultsFileDate.toString("yyyyMMddHHmmss")+".txt");
        if (!scraper->autoSaveScrapResultsFile.open(QFile::WriteOnly | QFile::Truncate))
        {
            appendLog(tr("Auto Save scrap results to file check, but unable to create save file."),"error");
            return;
        }
        else
        {
            scraper->scrapDataStream.setDevice(&scraper->autoSaveScrapResultsFile);
            scraper->scrapDataStream << "##########################################################################################" + lineBreak +
                                        tr("Scrap engine : ") + ui->customSeComboBox->currentText() + lineBreak +
                                        "Footprint : " + ui->footprintComboBox->currentText() + lineBreak +
                                        "Custom 1 : " + lineBreak + custom1Str + lineBreak +
                                        "Custom 2 : " + lineBreak + custom2Str + lineBreak +
                                        "##########################################################################################" + lineBreak;

        }
    }

    scraper->jsEnable = scrapEngineConfig.value("jsEnable").toBool();
    this->scraper->htmlEncoded = scrapEngineConfig.value("htmlEncoded").toBool();


    if(this->getSeCustomColumns().size())
    {
        scraper->customScrapColumn = true;
        this->buildScrapTabResults("customScrap","",this->getSeCustomColumns());
    }
    else
    {
        scraper->customScrapColumn = false;
        this->buildScrapTabResults("scrap");
    }


    // We set the keep html stack feature
    scraper->keepStackFiles = QVariant(this->config->getConfigurationValue("htmlStack","false")).toBool();

    QtConcurrent::run(this->scraper, &Scraper::launchScrap);
}

void MainWindow::on_backlinksButton_clicked()
{
    QHash<QString, QString> hashFp;
    QStringList domainList;
    int i;

    this->initScrapClass();

    // Here we open a multiline dialog
    // And we have to run backlinks check via qtconcurrent on backlink class
    bool ok;
    QString text = QInputDialog::getMultiLineText(this, tr("Enter your urls"),
                                                  tr("Url (one per line)"), ui->footprintComboBox->currentText(), &ok);
    if (ok && !text.isEmpty())
    {
        // Remove empty string from list
        domainList = text.split("\n");
        for (i = 0;i<domainList.size();i++)
        {
            if (!domainList.at(i).isEmpty())
                this->scraper->backlinks->urlToCheck.append(domainList.at(i));
        }
        //        this->scraper->backlinks->urlToCheck = text.split("\n");
        this->disableButtons();
        this->setBusyCursorSlot();
        this->buildScrapTabResults("backlinks", config->getConfigurationValue("backlinksActiveService"));

        tools.clearUrlProxies();
        if(ui->backlinksAhrefsOptionRadio->isChecked())
            this->scraper->backlinks->processBacklinksUrl("Ahrefs");
        else if(ui->backlinksMajesticOptionRadio->isChecked())
        {
            this->scraper->backlinks->processBacklinksUrl("Majestic");
        }
        else if(ui->backlinksMozOptionRadio->isChecked())
            this->scraper->backlinks->processBacklinksUrl("Moz");
        else if(ui->backlinksSeobserverOptionRadio->isChecked())
            this->scraper->backlinks->processBacklinksUrl("SEObserver");
        else
            this->scraper->backlinks->processBacklinksUrl("default");
    }

    return;
}

void MainWindow::on_abortButton_clicked()
{
    if(this->proxyTimer != nullptr && this->proxyTimer->isActive())
    {
        this->proxyTimer->stop();
        QObject::disconnect(this->proxyTimer, SIGNAL(timeout()), this, SLOT(timeoutProxies()));
        this->proxyTimer = nullptr;
    }
    scraper->sendAbortNetwork();

    appendLog(tr("Aborting network operations"));
}


/////////////////////////////////////////
/* First tab functions : scrap result */
///////////////////////////////////////

int MainWindow::idealThreadMultiplicator()
{
    QHash<QString, int> domainsHash;
    QString currentDomain;
    int i,nbDomains, threadMultiplicator;

    threadMultiplicator = 5;
    for(i=0;i<scraptable->scrapResults.size();i++)
    {
        QUrl testUrl(scraptable->scrapResults.value(i).at(scraptable->columnIndexes.value("urlCol")).toString());
        if(testUrl.isValid() && !testUrl.isRelative())
        {
            currentDomain   = Tools::getDomain(scraptable->scrapResults.value(i).at(scraptable->columnIndexes.value("urlCol")).toString(),_getDomainOnly,false);
            nbDomains       = domainsHash.value(currentDomain,0);
            domainsHash.insert(currentDomain,nbDomains+1);
        }
    }

    // We loop on the QHash and if a domain get more than 20 urls, we set the multiplicator to 1
    QHashIterator<QString,int> hashIterator(domainsHash);
    hashIterator.toFront();
    while(hashIterator.hasNext())
    {
        hashIterator.next();

        if(hashIterator.value() > 30)
        {
            threadMultiplicator = 1;
            break;
        }
        if(hashIterator.value() > 10)
        {
            threadMultiplicator = 2;
        }

    }

    if(domainsHash.size() == 1)
        threadMultiplicator = 1;

    return(threadMultiplicator);
}

void MainWindow::requestLauncher(int requestIndex, int timerType)
{
    Q_UNUSED(timerType);
    QStringList items, allDomainItems;
    QString currentDomain;
    int i,searchColumn, urlMapIndex, threadMultiplicator;
    bool buildDomainsList; // Flag for building a domains/subdomains list only


    this->initScrapClass();
    this->scraper->urlMapping.clear();
    this->scraper->initScrapVars();

    // Here we set all private variable for scraper class
    scraper->setGetBacklinksMethod(getBacklinksMethod);
    scraper->setWhoisApiService(config->getConfigurationValue("whoisApiService"));
    scraper->setWhoisApiKey(config->getConfigurationValue("whoisApiKey"+QString::number(ui->whoisApiComboBox->findText(config->getConfigurationValue("whoisApiService")))));


    threadMultiplicator = 1;
    buildDomainsList    = false;
    _getDomainOnly       = true;

    if(!scraptable->scrapResults.size())
        return;

    switch(requestIndex) {
    case REQUEST_STATUS_CODE :
        searchColumn = scraptable->columnIndexes.value("httpCol");
        scraper->threadTimeout = config->getConfigurationValue("statuscodeTimeout").toInt();
        break;
    case REQUEST_DOFOLLOW :
        searchColumn = scraptable->columnIndexes.value("dfCol");
        scraper->threadTimeout = config->getConfigurationValue("dofollowTimeout").toInt();
        break;
    case REQUEST_OBL :
        searchColumn = scraptable->columnIndexes.value("oblCol");
        break;
    case REQUEST_PLATFORM :
        searchColumn = scraptable->columnIndexes.value("platformCol");
        scraper->threadTimeout = config->getConfigurationValue("pageloadingTimeout").toInt();
        break;
    case REQUEST_BACKLINKS :
        if(getBacklinksMethod)
            buildDomainsList = true;
        if(getBacklinksMethod == 1)
            _getDomainOnly = false;

        searchColumn = scraptable->columnIndexes.value("blCol");
        break;
    case REQUEST_LINK_ALIVE :
        scraper->forceDeleteStack = 1;
        scraper->setFootprint(ui->footprintComboBox->currentText());
        searchColumn = scraptable->columnIndexes.value("linkAliveCol");
        break;
    case REQUEST_WHOIS :
        searchColumn = scraptable->columnIndexes.value("domainAvailableCol");
        threadMultiplicator = 1;
        buildDomainsList = true;
        if(config->getConfigurationValue("whoisApiKey"+QString::number(ui->whoisApiComboBox->currentIndex()),"").isEmpty())
        {
            emit appendLog(tr("Please enter your API Key !!"));
            return;
        }
        break;
    case REQUEST_IP_ADDR :
        searchColumn = scraptable->columnIndexes.value("ipCol");
        scraper->startIpTimer();
        break;
    default :
        searchColumn = 0;
    }


    if(requestIndex != REQUEST_REDIRECTION)
    {
        for(i=urlMapIndex=0;i<scraptable->scrapResults.size();i++)
        {
            if(buildDomainsList)
            {
                currentDomain = Tools::getDomain(scraptable->scrapResults.value(i).at(scraptable->columnIndexes.value("urlCol")).toString(),_getDomainOnly,false);
                allDomainItems.append(currentDomain);
            }

            if(scraptable->scrapResults.value(i).at(searchColumn).toString().isEmpty())
            {
                QUrl testUrl(scraptable->scrapResults.value(i).at(scraptable->columnIndexes.value("urlCol")).toString());
                if(testUrl.isValid() && !testUrl.isRelative())
                {
                    if(buildDomainsList)
                        items.append(currentDomain);
                    else
                    {
                        if(requestIndex == REQUEST_LINK_ALIVE && this->action != "scrap")
                        {
                            this->scraper->urlMapping.insert(urlMapIndex, QStringList() << scraptable->scrapResults.value(i).at(scraptable->columnIndexes.value("urlCol")).toString() << scraptable->scrapResults.value(i).at(scraptable->columnIndexes.value("blForCol")).toString());
                            urlMapIndex++;
                        }
                        items.append(scraptable->scrapResults.value(i).at(scraptable->columnIndexes.value("urlCol")).toString());
                    }
                }
            }
        }
    }
    else
    {
        items = this->getRedirUrls();
    }


    if(!items.size())
    {
        QString errorMsg = tr("Your scrap results doesn't contains any URL or ");
        switch(requestIndex)
        {
        case REQUEST_STATUS_CODE:
            errorMsg.append("HTTP Response code");
            break;
        case REQUEST_DOFOLLOW:
            errorMsg.append("dofollow/Nofollow check");
            break;
        case REQUEST_REDIRECTION:
            errorMsg.append("resolve redirections");
            break;
        case REQUEST_BACKLINKS:
            errorMsg.append("backlinks");
            break;
        case REQUEST_OBL:
            errorMsg.append("outbound links");
            break;
        case REQUEST_IP_ADDR:
            errorMsg.append("IP addresses");
            break;
        case REQUEST_LINK_ALIVE:
            errorMsg.append("Link alive");
            break;
        case REQUEST_WHOIS:
            errorMsg.append("Domain available");
            break;
        case REQUEST_PLATFORM:
            errorMsg.append("Platform check");
            break;

        }
        errorMsg.append(tr(" has already been recovered."));
        appendLog(errorMsg);
        this->enableButtons();
        this->restoreCursor();
    }


    // For domains list, we dedup domains in order to have a clean list
    if(buildDomainsList)
        items = Tools::dedupStringList(items);


    // Import case : we have to keep the total for fields already retrieved
    if (requestIndex == REQUEST_STATUS_CODE || requestIndex == REQUEST_REDIRECTION) // resolveredirect
        scraper->countHeader = scraptable->scrapResults.size() - items.size();
    else if (requestIndex == REQUEST_DOFOLLOW)
        scraper->countRel = scraptable->scrapResults.size()  - items.size();
    else if (requestIndex == REQUEST_BACKLINKS)
        scraper->countBls = scraptable->scrapResults.size()  - items.size();
    else if (requestIndex == REQUEST_OBL)
        scraper->countObl = scraptable->scrapResults.size()  - items.size();
    else if (requestIndex == REQUEST_LINK_ALIVE)
        scraper->countLinkAlive = scraptable->scrapResults.size()  - items.size();
    else if (requestIndex == REQUEST_PLATFORM)
        scraper->countPlatform = scraptable->scrapResults.size()  - items.size();
    else if (requestIndex == REQUEST_WHOIS)
        scraper->countWhois = Tools::dedupStringList(allDomainItems).size()  - items.size();


    if(items.size())
    {
        switch(requestIndex)
        {
        case REQUEST_STATUS_CODE:
            appendLog(tr("Get HTTP Response code launched"));
            threadMultiplicator =  this->idealThreadMultiplicator();
            break;
        case REQUEST_DOFOLLOW:
            appendLog(tr("Check Dofollow/Nofollow launched"));
            threadMultiplicator =  this->idealThreadMultiplicator(); // libtidy Effect !!
            break;
        case REQUEST_REDIRECTION:
            appendLog(tr("Resolve redirections launched"));
            threadMultiplicator =  this->idealThreadMultiplicator();
            break;
        case REQUEST_BACKLINKS:
            appendLog(tr("Get Backlinks launched"));
            break;
        case REQUEST_OBL:
            appendLog(tr("Get outbound links launched"));
            threadMultiplicator =  this->idealThreadMultiplicator(); // libtidy Effect !!
            break;
        case REQUEST_IP_ADDR:
            appendLog(tr("IP addresses lookup launched"));
            break;
        case REQUEST_LINK_ALIVE:
            appendLog(tr("Link alive launched"));
            threadMultiplicator =  this->idealThreadMultiplicator(); // libtidy Effect !!
            break;
        case REQUEST_WHOIS:
            appendLog(tr("Get domains available launched") + " ["+config->getConfigurationValue("whoisApiService")+" API]");
            threadMultiplicator = 2;
            break;
        case REQUEST_PLATFORM:
            appendLog(tr("Platform check launched"));
            threadMultiplicator =  this->idealThreadMultiplicator();
            break;
        }
    }

    this->disableButtons();
    this->setBusyCursorSlot();

    tools.clearUrlProxies();
    scraper->startAt    = 0;


    // New setting, based on computer capacity
    scraper->interval = QThread::idealThreadCount() * threadMultiplicator;


    // For whois, interval is equal to the number of domains we can send to one API request !!!
    if(requestIndex == REQUEST_WHOIS && config->getConfigurationValue("whoisApiService") == "Dynadot")
        scraper->interval   = 10;

    // For backlinks, we can do bulk for Majestic and Moz
    if(requestIndex == REQUEST_BACKLINKS && config->getConfigurationValue("blApiForScrap") != "Ahrefs")
        scraper->interval = 50;



    // We send informations on log tab
    if(requestIndex != REQUEST_BACKLINKS)
        emit appendLog(tr("Parallel threads : %1").arg(QString::number(scraper->interval)),"information");
    emit appendLog(tr("Threads timeout : %1").arg(QString::number(scraper->threadTimeout)),"information");

    scraper->urlList    = items;
    scraper->requestLauncher(requestIndex);
}

void MainWindow::on_trimToRoot_clicked()
{
    QtConcurrent::run(this, &MainWindow::trimToRoot);
}

void MainWindow::on_trimToLastFolderButton_clicked()
{
    QtConcurrent::run(this, &MainWindow::trimToLastFolder);
}

void MainWindow::on_keepOnlyDomainButton_clicked()
{
    QtConcurrent::run(this, &MainWindow::keepOnlyDomains);
}


void MainWindow::on_httpButton_clicked()
{
    if(this->getColumnIndexByName("httpCol") != -1)
        this->requestLauncher(REQUEST_STATUS_CODE,TIMER_HTTPSTATUS);
    else
        emit displayMsg(tr("HTTP column not present"));
}

void MainWindow::on_checkDofollowNofollow_clicked()
{
    if(this->getColumnIndexByName("dfCol") != -1)
        this->requestLauncher(REQUEST_DOFOLLOW,TIMER_DOFOLLOW);
    else
        emit displayMsg(tr("Dofollow column not present"));
}

void MainWindow::on_resolveRedirectButton_clicked()
{ 
    int response;

    if(this->getColumnIndexByName("httpCol") != -1)
    {
        if (!scraptable->getNbItemsByCol(scraptable->columnIndexes.value("httpCol")))
        {
            response = QMessageBox::information(this, "No HTTP Header found", tr("You must first launch an HTTP header check in order to launch a resolve of all the redirections.Would you like to run it now ?"),QMessageBox::Yes | QMessageBox::No);
            if(response == QMessageBox::No)
                return;
            this->on_httpButton_clicked();
            return ;
        }
        this->requestLauncher(REQUEST_REDIRECTION,TIMER_HTTPSTATUS);
    }
    else
        emit displayMsg(tr("HTTP column not present"));
}


void MainWindow::getToolBacklinksDomain()
{
    if(config->getConfigurationValue("blApiForScrap","Free") == "Free")
    {
        emit displayMsg(tr("We don't support the Majestic Free API anymore. Please choose another backlinks API."));
        return;
    }

    getBacklinksMethod = 2;
    this->getToolBacklinksButton();
}

void MainWindow::getToolBacklinksSubomain()
{
    if(config->getConfigurationValue("blApiForScrap","Free") == "Free")
    {
        emit displayMsg(tr("We don't support the Majestic Free API anymore. Please choose another backlinks API."));
        return;
    }

    getBacklinksMethod = 1;
    this->getToolBacklinksButton();
}

void MainWindow::getToolBacklinksUrl()
{
    if(config->getConfigurationValue("blApiForScrap","Free") == "Free")
    {
        emit displayMsg(tr("We don't support the Majestic Free API anymore. Please choose another backlinks API."));
        return;
    }

    getBacklinksMethod = 0;
    this->getToolBacklinksButton();
}


void MainWindow::getToolBacklinksButton()
{
    if(this->getColumnIndexByName("blCol") != -1)
        this->requestLauncher(REQUEST_BACKLINKS, TIMER_BACKLINKS);
    else
        emit displayMsg(tr("Backlinks column not present"));
}

void MainWindow::on_getOblButton_clicked()
{
    if(this->getColumnIndexByName("oblCol") != -1)
        this->requestLauncher(REQUEST_OBL, TIMER_OBL);
    else
        emit displayMsg(tr("Outbound Link column not present"));
}

void MainWindow::on_getPlatformButton_clicked()
{
    if(this->getColumnIndexByName("platformCol") != -1)
        this->requestLauncher(REQUEST_PLATFORM, TIMER_PLATFORM);
    else
        emit displayMsg(tr("Platform column not present"));
}

void MainWindow::on_getIpAddrButton_clicked()
{
    if(this->getColumnIndexByName("ipCol") != -1)
        this->requestLauncher(REQUEST_IP_ADDR, TIMER_IP);
    else
        emit displayMsg(tr("IP Address column not present"));
}

void MainWindow::on_linkAliveButton_clicked()
{
    if(this->getColumnIndexByName("linkAliveCol") != -1)
        this->requestLauncher(REQUEST_LINK_ALIVE, TIMER_LINKALIVE);
    else
        emit displayMsg(tr("Link Alive column not present"));
}

void MainWindow::on_whoisButton_clicked()
{
    if(this->getColumnIndexByName("domainAvailableCol") != -1)
        this->requestLauncher(REQUEST_WHOIS, TIMER_WHOIS);
    else
        emit displayMsg(tr("Domain Available column not present"));
}

void MainWindow::transferToCustom1()
{
    QString content, custom1Str;
    QVariantList variantList;
    QStringList myList, custom1List;

    QAction *acSender = qobject_cast<QAction*>(QObject::sender());

    variantList  = (acSender->data().type() == QVariant::Int && acSender->data().toInt() >= 0) ? scraptable->getItemsByColumn(acSender->data().toInt()) : scraptable->getItemsByColumn(scraptable->columnIndexes.value("urlCol"));

    foreach (QVariant variantValue, variantList) {
        myList.append(variantValue.toString());
    }

    // We get custom1 content
    custom1Str      = ui->custom1List->toPlainText().trimmed();
    custom1List     = custom1Str.split("\n");
    // We append it to the list
    myList.append(custom1List);
    // And we dedup
    myList.removeDuplicates();

    content = myList.join("\n");
    ui->custom1List->document()->setPlainText(content);
}

void MainWindow::transferNonExistingToCustom1()
{
    QString content, custom1Str;
    QVariantList variantList;
    QStringList myList,custom1List,diff;

    QAction *acSender = qobject_cast<QAction*>(QObject::sender());

    variantList  = (acSender->data().type() == QVariant::Int && acSender->data().toInt() >= 0) ? scraptable->getItemsByColumn(acSender->data().toInt()) : scraptable->getItemsByColumn(scraptable->columnIndexes.value("urlCol"));

    foreach (QVariant variantValue, variantList) {
        myList.append(variantValue.toString());
    }

    custom1Str      = ui->custom1List->toPlainText().trimmed();
    custom1List     = custom1Str.split("\n");

    QSet<QString> subtraction = myList.toSet().subtract(custom1List.toSet());
    diff = subtraction.toList();
    content = diff.join("\n");
    ui->custom1List->document()->setPlainText(content);
}


void MainWindow::on_openSearchReplaceBox_clicked()
{
    int response;
    QString searchValue;
    QString replaceValue;
    bool isRegex;
    bool ignoreScheme;

    response = searchreplacebox.exec();
    if(response == 1)
    {
        searchValue = searchreplacebox.getSearchValue();
        replaceValue = searchreplacebox.getReplaceValue();
        isRegex = searchreplacebox.isRegex();
        ignoreScheme = searchreplacebox.ignoreScheme();

        QtConcurrent::run(scraptable, &scrapTable::searchAndReplace, searchValue, replaceValue,isRegex, ignoreScheme);

    }


}

void MainWindow::on_openDeleteBoxButton_clicked()
{
    QStringList columnsName;
    QVariant removeValue;
    QString removeType;
    int i,itemsRemoved;
    QFuture<int> future;

    for(i=0;i<scraptable->colCount;i++)
        columnsName.append(this->scraptable->headerData(i,Qt::Horizontal).toString());

    deletebox.setColumnsDropDown(columnsName);
    int         response = deletebox.exec();

    itemsRemoved = 0;
    if(response == 1)
    {
        removeValue = deletebox.getRemoveValue();
        removeType = deletebox.getRemoveType();
        if(removeValue.toString().isEmpty())
            return;

        if(removeType == "stringContainingRadioButton")
        {
            if(this->deletebox.isRegex())
                future = QtConcurrent::run(scraptable, &scrapTable::removeColumnMatching, this->deletebox.getColumnIndex(), removeValue.toString(),true);
            else
                future = QtConcurrent::run(scraptable, &scrapTable::removeColumnContaining, this->deletebox.getColumnIndex(), removeValue.toString(),true);
        }
        else if(removeType == "stringNotContainingRadioButton")
        {
            if(this->deletebox.isRegex())
                future = QtConcurrent::run(scraptable, &scrapTable::removeColumnMatching, this->deletebox.getColumnIndex(), removeValue.toString(),false);
            else
                future = QtConcurrent::run(scraptable, &scrapTable::removeColumnContaining, this->deletebox.getColumnIndex(), removeValue.toString(),false);
        }
        else if(removeType.startsWith("number"))
        {
            QRegExp filterRx("^number([A-Za-z]+)RadioButton$");
            if(filterRx.indexIn(removeType) != -1)
                future = QtConcurrent::run(scraptable, &scrapTable::removeColumnByFilter,this->deletebox.getColumnIndex(),removeValue.toDouble(),filterRx.cap(1).toLower());
        }
        itemsRemoved = future.result();

        emit deleteItemsFromTable(itemsRemoved,false,false);
        emit updateNbUrlSignal();
    }

}

void MainWindow::on_removeBadUrlButton_clicked()
{
    if(this->getColumnIndexByName("httpCol") != -1)
    {
        emit setBusyCursor();
        this->removeBadUrl(true,false);
    }
    else
        emit displayMsg(tr("HTTP column not present"));

}

void  MainWindow::removeDuplicateUrlAction()
{
    QtConcurrent::run(this, &MainWindow::removeDuplicateUrlSlot,true,false,true);
}

void MainWindow::removeDuplicateDomainsAction()
{
    this->disableButtons();
    QtConcurrent::run(this, &MainWindow::removeDuplicateDomains);
}

void  MainWindow::on_removeSelectedUrlButton_clicked()
{
    int itemsRemoved;

    this->disableButtons();
    itemsRemoved = this->scraptable->removeSelectedRows(ui->listResult->selectionModel()->selectedIndexes());

    this->deleteItemsFromTableSlot(itemsRemoved,true,false);
    emit updateNbUrlSignal();
}

void MainWindow::on_removeSubdomainButton_clicked()
{
    int itemsRemoved;

    this->disableButtons();
    itemsRemoved = this->scraptable->removeSubdomains();
    if(!itemsRemoved)
        emit noDuplicateUrl(false, tr("All subdomains already removed"),tr("Remove subdomains"));
    this->deleteItemsFromTableSlot(itemsRemoved,true,false);
    emit updateNbUrlSignal();
}

void MainWindow::on_removeRegexMaskButton_clicked()
{
    QString userRegex;
    int itemsRemoved;


    emit setBusyCursor();
    userRegex = QInputDialog::getText(this, tr("Remove matched string/regex"), tr("Remove matched string/regex on each line."));
    if (!userRegex.isEmpty())
    {
        itemsRemoved = scraptable->removeRegexMask(userRegex);
        emit deleteItemsFromTable(itemsRemoved,false,false);
        emit updateNbUrlSignal();
    }
    emit restoreCursor();
}


void MainWindow::fullScrapExportButtonEmulate()
{
    tools.saveFile(this->GetScrapListResult(), QStringList() << ".txt" << ".csv", _delimiters);
}


void MainWindow::stdScrapExportButtonEmulate()
{
    tools.saveFile(this->GetScrapListResult(), QStringList() << ".txt" << ".csv",_delimiters);
}

QMap<int,QStringList> MainWindow::GetScrapListResult()
{

    QMap<int,QStringList> stack;
    QStringList lineStack;

    int i,col,itemCols;

    itemCols = scraptable->colCount;

    // We retrieve the column headers
    for(col=0;col<itemCols;col++)
    {
        lineStack.append(scraptable->headerData(col,Qt::Horizontal,Qt::DisplayRole).toString());
    }
    stack.insert(0,lineStack);

    // And now the content
    for (i = 0; i < scraptable->scrapResults.size();i++)
    {
        lineStack.clear();
        for(col=0;col<itemCols;col++)
        {
            lineStack.append(scraptable->scrapResults.value(i).at(col).toString());
        }
        stack.insert(i+1,lineStack);
    }

    return stack;
}

void MainWindow::importScrapButtonEmulate()
{
    QByteArray content;
    QString csvPattern;
    int response;

    QString passedMode = "urlsonly";

    content = tools.openFile();
    csvPattern.append(_csvDelimiter);
    csvPattern.append(_csvSeparator);
    csvPattern.append(_csvDelimiter);



    if(!content.isEmpty())
    {
        if(scraptable->scrapResults.size())
        {
            response =  QMessageBox::question(this, tr("Import option"), tr("Do you want to replace current list ?<br>Answer Yes to replace, or No to append to current list"), QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);

            if (response == QMessageBox::Yes)
            {
                scraptable->clearScrapResultsHash();
                this->scraper->countHeader = 0;
                this->scraper->countRel = 0;
                this->scraper->countBls = 0;
                this->scraper->countObl = 0;
            }
        }
        if(content.contains(csvPattern.toLatin1()))
            this->updateScrapList(content,"csv");
        else if(content.contains(txtDelimiter.toLatin1()))
        {
            // We check if we have a ration between the number of | and the number of rows
            int fileLines = content.split('\n').size();
            int foundPipes = content.count("|");

            if(foundPipes)
            {
                if(((fileLines * foundPipes) % foundPipes) % 2 == 0)
                {
                    passedMode = "txt";
                    this->updateScrapList(content,"txt");
                }
                else
                    this->updateScrapList(content);
            }
            else
                this->updateScrapList(content);
        }
        else
            this->updateScrapList(content);
    }
}


void MainWindow::updateScrapList(QByteArray data, QString type)
{
    QList<QByteArray> list;
    QString separator, csvPattern, filetype;

    int i, countSeparator;
    csvPattern.append(_csvDelimiter);
    csvPattern.append(_csvSeparator);
    csvPattern.append(_csvDelimiter);

    QString headerMozFiletype("# API : Moz");
    QString headerAhrefsFiletype("# API : Ahrefs");
    QString headerMajesticFiletype("# API : Majestic");
    QString scrapFileHeader;
    QStringList scrapFileHeaderList;


    scrapFileHeader.clear();
    scrapFileHeaderList.clear();
    filetype = "scrap";

    this->disableButtons();
    emit setBusyCursor();

    list = data.split('\n');
    separator = QString(txtDelimiter);

    if(type == "urlsonly")
    {
        scrapFileHeader = "URL";
        scrapFileHeaderList.append(scrapFileHeader);
    }

    // We detect separator on the first line
    if (type == "csv")
    {
        QString firstLine(list.at(0).simplified());
        if (firstLine.contains(headerAhrefsFiletype))
        {
            filetype = "Ahrefs";
            scrapFileHeader = list.at(1).simplified();
        }
        else if (firstLine.contains(headerMajesticFiletype))
        {
            filetype = "Majestic";
            scrapFileHeader = list.at(1).simplified();
        }
        else if (firstLine.contains(headerMozFiletype))
        {
            filetype = "Moz";
            scrapFileHeader = list.at(1).simplified();
        }
        else
            scrapFileHeader = firstLine;

        //        rxSeparator.setPattern("\"(,|;)\"");
        separator = csvPattern;
    }
    else if (type != "urlsonly")
    {
        QString lineOne(list.at(1).simplified());
        for (i = countSeparator = 0; i < list.size(); i++)
        {
            QString line(list.at(i).simplified());

            if(!i)
            {
                if (line.contains(headerAhrefsFiletype))
                {
                    filetype = "Ahrefs";
                    scrapFileHeader = lineOne.remove("#");
                    i++;
                }
                else if(line.contains(headerMajesticFiletype))
                {
                    filetype = "Majestic";
                    scrapFileHeader = lineOne.remove("#");
                    i++;
                }
                else if(line.contains(headerMozFiletype))
                {
                    filetype = "Moz";
                    scrapFileHeader = lineOne.remove("#");
                    i++;
                }
                else if(line.startsWith('#'))
                {
                    scrapFileHeader = line.remove("#");
                    i++;
                }
                else if(!line.startsWith('#')) // Retro compatibilite, pour les files sans header
                {
                    scrapFileHeader = "URL"+separator+"PR";
                }
                break;
            }

            if(!line.startsWith('#'))
            {
                QStringList my_values = line.split(separator);

                if (my_values.at(0).isEmpty())
                    continue;
                if (i == 0 || countSeparator == 0)
                    countSeparator = my_values.size();
                else
                    if (countSeparator != my_values.size())
                    {
                        type = "urlsonly";
                        break;
                    }
            }
        }
    }


    scrapFileHeaderList = scrapFileHeader.split(separator);
    scrapFileHeaderList.replaceInStrings("\"","");

    if(filetype != "scrap")
        this->buildScrapTabResults("backlinks",filetype,scrapFileHeaderList);
    else
        this->buildScrapTabResults("customScrap","",scrapFileHeaderList);

    QStringList concurrentArgs;
    concurrentArgs << type << separator;
    QFuture<QMap<int,QVariantList> > future;
    future = QtConcurrent::run(&tools, &Tools::parseInputFile, data, concurrentArgs, scraptable->colCount, scrapFileHeaderList, this->scraptable->tableHeaderData);

    future.waitForFinished();
    this->scraptable->reBuildScrapResults(future.result());

    emit updateNbUrlSignal();
    emit restoreCursor();
    this->enableButtons();
}


void MainWindow::on_ClearUrlList_clicked()
{
    scraptable->clearScrapResultsHash();

    this->scraper->countHeader = 0;
    this->scraper->countRel = 0;
    this->scraper->countBls = 0;
    this->scraper->countObl = 0;
    ui->nbUrls->clear();
}



/////////////////////////////////////
/* Second tab functions : proxies */
///////////////////////////////////

void MainWindow::on_useProxiesCheckBox_clicked(bool activated)
{
    ui->proxiesGroupBox->setEnabled(activated);
    ui->testProxiesOnGoogleCheckBox->setEnabled(activated);
    ui->useProxyRotatingCheckBox->setEnabled(true);

    if(this->getProxiesFromList().isEmpty() && activated == true)
        activated = false;
    config->updateAutomation("useProxies",activated);

    config->loadProxies();
    this->initProxiesList();
}


void MainWindow::on_scrapWithoutProxyWarnCheckBox_clicked(bool activated)
{
    config->updateAutomation("disableWarnProxies",activated);
}

void MainWindow::on_testProxiesOnGoogleCheckBox_clicked(bool activated)
{
    config->updateAutomation("testProxiesOnGoogle",activated);
}

void MainWindow::on_useProxyRotatingCheckBox_clicked(bool activated)
{
    config->updateAutomation("useProxyRotating",activated);
}

void MainWindow::initProxiesList()
{

    QStringList proxies = this->config->get_proxies();
    this->model = new QStandardItemModel(1,4, ui->proxiesList);
    QStringList headers;

    headers<<tr("IP")<<tr("Port")<<tr("User")<<tr("Password")<<tr("Speed (ms)")<<tr("Status");

    model->setHorizontalHeaderLabels(headers);
    ui->proxiesList->setModel(this->model);
    ui->proxiesList->horizontalHeader()->setStretchLastSection(true);
    ui->proxiesList->horizontalHeader()->resizeSection(0,120);
    ui->proxiesList->horizontalHeader()->resizeSection(1,50);
    ui->proxiesList->horizontalHeader()->resizeSection(2,80);
    ui->proxiesList->horizontalHeader()->resizeSection(3,80);
    ui->proxiesList->horizontalHeader()->resizeSection(4,100);
    ui->proxiesList->horizontalHeader()->resizeSection(5,220);
    ui->proxiesList->verticalHeader()->setDefaultSectionSize(20);

    this->updateProxiesList(proxies, "add");
}



void MainWindow::markDeadProxy(QString proxy, int error)
{
    QString msgError;
    msgError.clear();

    if(error == 1)
        msgError = tr("Connection refused");
    else if(error == 3)
        msgError = tr("Host not found");
    else if(error == 99)
        msgError = tr("Unknown network-related error");
    else if(error == 101)
        msgError = tr("Proxy connection refused");
    else if(error == 102)
        msgError = tr("Proxy closed connection prematurely");
    else if(error == 103)
        msgError = tr("Proxy hostname not found");
    else if(error == 104)
        msgError = tr("Proxy Timeout");
    else if(error == 105)
        msgError = tr("Proxy authentication failed");
    else if(error == 199)
        msgError = tr("Unknown proxy-related error was detected");
    else if(error == 202 && config->getTestProxiesOnGoogle() == false)
        msgError = tr("Content operation not permitted");
    else if(error == 299)
        msgError = tr("Unknown error related to the remote content was detected");
    else if(error == 399)
        msgError = tr("Protocol failure");
    else if(error == 500)
        msgError = tr("Proxy connection failed");
    else if(error == 503 && config->getTestProxiesOnGoogle()) // Code perso pour captcha google
        msgError = tr("Google test failed (captcha)");
    else if(error == 504 || (error == 202 && config->getTestProxiesOnGoogle())) // Code perso pour captcha google
        msgError = tr("Google test failed (blocked)");
    else
        msgError = tr("Proxy doesn't work");

    if (error != 503 || (error == 503 && config->getTestProxiesOnGoogle() == false))
        msgError.prepend("Error : ");
    this->updateProxiesList(QStringList(proxy),"update", this->errorBrush, msgError);
}

void MainWindow::markGoodProxy(QString proxy)
{
    this->updateProxiesList(QStringList(proxy),"update", this->okBrush,"OK");
}

void MainWindow::on_proxiesAddButton_clicked()
{
    bool ok;
    int nb_proxies;

    nb_proxies = QInputDialog::getInt(this, tr("Add proxies"),
                                      tr("How many proxies do you want to add ?"), 1, 0, 100, 1, &ok);
    if (ok)
        this->model->insertRows(this->model->rowCount(),nb_proxies);
}

void MainWindow::on_proxiesDeleteSelectedButton_clicked()
{
    int i;
    QHash<int,int> rowHash;
    QList<int>     rowList;

    QModelIndexList indexes = ui->proxiesList->selectionModel()->selectedIndexes();
    std::sort(indexes.begin(), indexes.end());

    rowHash.clear();
    for (i = indexes.count() - 1; i > -1; --i)
        rowHash.insert(indexes.at(i).row(), indexes.at(i).row());
    rowList = rowHash.values();
    std::sort(rowList.begin(), rowList.end(), std::greater<int>());
    ui->proxiesList->setUpdatesEnabled(false);
    for (i = 0; i < rowList.size(); i++)
        model->removeRow(rowList.value(i));
    ui->proxiesList->setUpdatesEnabled(true);
}

void MainWindow::on_proxiesClearAllButton_clicked()
{
    this->model->removeRows(0,this->model->rowCount());
    this->saveMsg = "Silent";
    config->deleteAllProxies();
    this->on_SaveProxies_clicked();
}

void MainWindow::on_proxiesDeleteButton_clicked()
{
    QList<QStandardItem *> itemlist;
    int row;
    itemlist = this->model->findItems("Error",Qt::MatchStartsWith, 5);
    for(int i=0;i<itemlist.count();i++)
    {
        row = itemlist.at(i)->row();
        config->delete_proxy(model->item(row,0)->text());
        model->removeRow(row);
    }
    this->on_SaveProxies_clicked();
}

void MainWindow::timeoutProxies()
{
    int i,j,row,abort = 0;
    QString proxy;
    QStandardItem *item, *itemProxy;

    row = this->model->rowCount();

    for(i=0;i<row;i++)
    {
        item = this->model->item(i,5);
        if (!item || item->text().isEmpty())
        {
            if (!abort) {
                appendLog(tr("Aborting proxy test operations"));
                scraper->sendAbortNetwork();
                abort = 1;
            }
            for (j = 0;j<4;j++)
            {
                itemProxy = this->model->item(i,j);
                if(j && itemProxy && !itemProxy->text().isEmpty())
                    proxy.append(':');
                if(itemProxy)
                    proxy.append(itemProxy->text());
            }
            markDeadProxy(proxy, 104);
            proxy.clear();
        }
    }
}

void MainWindow::on_proxiesTestButton_clicked()
{
    this->initScrapClass();

    QHash<int,QStringList> proxiesHash = this->getProxiesFromList();
    QHashIterator<int,QStringList> proxiesIterator(proxiesHash);
    QStringList proxies;


    proxies.clear();
    proxiesIterator.toFront();
    while (proxiesIterator.hasNext())
    {
        proxiesIterator.next();
        proxies.append(proxiesIterator.value().filter(QRegExp("^.+$")).join(':'));
    }


    proxyTimeStart.start();
    int i;

    this->disableButtons();
    // We reset background to default
    for(i=0;proxies.size() > i;i++)
        this->updateProxiesList(QStringList(proxies.at(i)),"update", QBrush(),"");

    for (i = 0; i < proxies.size(); i++)
        network->createHttpRequest(proxies[i], "standard", NETWORK_PROXY, QStringList());

}

void MainWindow::on_proxiesImportButton_clicked()
{
    QByteArray content;
    QStringList list, fields;
    QStandardItem *item;
    content = tools.openFile("proxies");
    int i,j,line,field_size;
    QString fileContent(content);
    QString lastField;
    QRegExp ipFilter("\\d{1,3}\\.\\d{1,3}\\.\\d{1,3}\\.\\d{1,3}");

    if (!fileContent.isEmpty())
    {
        model->removeRows(0,model->rowCount());
        list = fileContent.trimmed().split(QRegExp("[\r\n]"),QString::SkipEmptyParts);

        for(i=line=0;i<list.size();i++)
        {
            fields      = list.at(i).trimmed().split(':');
            field_size  = fields.count();

            if(field_size>4)
            {
                lastField.clear();
                for(j=3;j<field_size;j++)
                {
                    lastField.append(fields.at(j));
                    if(j+1 != field_size)
                        lastField.append(":");
                }
                fields[3] = lastField;
                field_size = 4;
            }

            if(fields[0].size())
            {
                if(ipFilter.indexIn(fields[0]) != -1)
                {
                    for (j=0;j<field_size;j++)
                    {
                        item = new QStandardItem();
                        item->setText(fields.at(j));
                        //item->setEditable(false);
                        this->model->setItem(line,j,item);
                    }
                    line++;
                }
            }

        }
        this->saveMsg = "Proxies loaded succesfully";
        on_SaveProxies_clicked();
    }

}

QHash<int,QStringList> MainWindow::getProxiesFromList()
{
    int i,j,row;
    QStringList proxies;
    QStandardItem *item;
    QHash<int,QStringList> hash;

    row = this->model->rowCount();

    for(i=0;i<row;i++)
    {
        proxies.clear();
        for(j=0;j<4;j++)
        {
            item = this->model->item(i,j);
            if(!item)
                proxies.append("");
            else
                proxies.append(item->text());
        }
        hash.insert(i,proxies);
    }
    return hash;
}

void MainWindow::updateProxiesList(QStringList proxies,QString action,QBrush brush, QString errMsg)
{
    ui->proxiesList->setSortingEnabled(false);
    QStandardItem *item;
    QStringList fields;
    QString lastField;
    int i,j,field_size,items;

    if(action == "add")
    {
        for(i=0;i<proxies.size();i++)
        {
            lastField.clear();
            fields      = proxies.at(i).trimmed().split(':');
            field_size  = fields.count();

            if(field_size>4)
            {
                for(j=3;j<field_size;j++)
                {
                    lastField.append(fields.at(j));
                    if(j+1 != field_size)
                        lastField.append(":");
                }
                fields[3] = lastField;
                field_size = 4;
            }

            for (j=0;j<field_size;j++)
            {
                item = new QStandardItem();
                item->setText(fields.at(j));
                //item->setEditable(false);
                this->model->setItem(i,j,item);
            }
        }
    }
    else if(action == "update")
    {
        QList<QStandardItem *> itemlist, emptyItemList;
        int row;

        for(i=0;i<proxies.size();i++)
        {
            fields      = proxies.at(i).trimmed().split(':');
            itemlist    = this->model->findItems(fields.at(0));



            for (items = 0; items < itemlist.size();items++)
            {
                row = itemlist.at(items)->row();

                if(fields.size() > 1)
                {
                    if(fields.at(1) == model->item(row, 1)->text())
                    {
                        //if(errMsg == "OK")
                        model->setData(model->index(row, 4),proxyTimeStart.elapsed());
                        model->setData(this->model->index(row,4), Qt::AlignRight, Qt::TextAlignmentRole);
                        model->setData(model->index(row, 5),errMsg);
                        for(j=0;j<this->model->columnCount();j++)
                            model->setData(model->index(row, j),brush, Qt::BackgroundRole);
                    }
                }
                else
                {
                    model->setData(model->index(row, 1),80);
                }
            }
        }
        emptyItemList    = this->model->findItems("",Qt::MatchExactly,5);
        if(!emptyItemList.size())
            this->enableButtons();

    }
    ui->proxiesList->setSortingEnabled(true);
}

void MainWindow::on_SaveProxies_clicked()
{
    QHash<int,QStringList> proxiesList;

    this->saveMsg = (this->saveMsg.isEmpty()) ? "Proxies saved successfully" : this->saveMsg;
    proxiesList = this->getProxiesFromList();
    config->deleteAllProxies();
    this->config->insertProxies(proxiesList);

    if(this->saveMsg != "Silent")
    {
        if(config->getConfigurationValue("silentMode") == "false")
            QMessageBox::information(this, "Save configuration", this->saveMsg);
        else
            ui->statusBar->showMessage(this->saveMsg, 2000);
    }
    this->saveMsg.clear();
}


/////////////////////////////////////////
/* Third tab functions : scrap config */
///////////////////////////////////////

void MainWindow::on_SESelect_activated(QString /*se*/)
{
    QHash<QString, QVariant> scrapEngineConfig;

    scrapEngineConfig = config->getScrapEngineConfigurationHash(ui->SESelect->currentData(Qt::AccessibleTextRole).toInt());

    ui->seDisplayNameLabel->setText(scrapEngineConfig.value("url").toString());
    ui->SESelect->setToolTip(scrapEngineConfig.value("url").toString());
    ui->custom1List->setStyleSheet("background-color: rgb(255,255,255);");
    ui->custom2List->setStyleSheet("background-color: rgb(255,255,255);");

    QPair<int,QString> currentSE;
    currentSE.first = ui->SESelect->currentData(Qt::AccessibleTextRole).toInt();
    currentSE.second = ui->SESelect->currentData(Qt::WhatsThisRole).toString();
    config->setCurrentSe(currentSE);


    config->loadCustoms(ui->keepCustom1OptionCheckbox->isChecked(), ui->keepCustom2OptionCheckbox->isChecked());

    if(scrapEngineConfig.value("url").toString().contains("{%custom1%}") && ui->custom1List->document()->toPlainText().isEmpty())
        ui->custom1List->setStyleSheet("background-color: rgb(229,120,122);");
    if(scrapEngineConfig.value("url").toString().contains("{%custom2%}") && ui->custom2List->document()->toPlainText().isEmpty())
        ui->custom2List->setStyleSheet("background-color: rgb(229,120,122);");


    // We set the combobox in the expert mode tab
    ui->customSeComboBox->setCurrentIndex(ui->customSeComboBox->findData(currentSE.second,Qt::WhatsThisRole));
    // And we load data via slot
    this->on_customSeComboBox_currentIndexChanged(ui->customSeComboBox->findData(currentSE.second,Qt::WhatsThisRole));

}

void MainWindow::on_saveConfig_clicked()
{
    QHash<QString, QVariant> scrapEngineConfig;

    scrapEngineConfig = config->getScrapEngineConfigurationHash(ui->SESelect->currentData(Qt::AccessibleTextRole).toInt());


    scrapEngineConfig.insert("custom1", ui->custom1List->toPlainText());
    scrapEngineConfig.insert("custom2", ui->custom2List->toPlainText());

    if(scrapEngineConfig.value("url").toString().contains(QRegExp("\\{%custom1(::encoded)?%\\}")) && ui->custom1List->document()->toPlainText().isEmpty())
        ui->custom1List->setStyleSheet("background-color: rgb(229,120,122);");
    else if(scrapEngineConfig.value("url").toString().contains(QRegExp("\\{%custom1(::encoded)?%\\}")) && !ui->custom1List->document()->toPlainText().isEmpty())
        ui->custom1List->setStyleSheet("background-color: rgb(255,255,255);");
    if(scrapEngineConfig.value("url").toString().contains(QRegExp("\\{%custom2(::encoded)?%\\}")) && ui->custom2List->document()->toPlainText().isEmpty())
        ui->custom2List->setStyleSheet("background-color: rgb(229,120,122);");
    else if(scrapEngineConfig.value("url").toString().contains(QRegExp("\\{%custom2(::encoded)?%\\}")) && !ui->custom2List->document()->toPlainText().isEmpty())
        ui->custom2List->setStyleSheet("background-color: rgb(255,255,255);");

    config->updateSeList(scrapEngineConfig,scrapEngineConfig.value("display_name").toString(),"customsOnly");
}



//////////////////////////////////////////////////
/* Fourth tab functions : custom search engine */
////////////////////////////////////////////////

void MainWindow::clearCustomSeFields()
{
    ui->customSeDisplayName->clear();
    ui->customSeColumnHeaderTable->clearContents();
    ui->customSeUrl->clear();
    ui->customSeContent->clear();
    ui->customSePagination->clear();
    ui->customSePrefix->clear();
    ui->customSeSuffix->clear();

    ui->resendSourceCodeString->clear();
    ui->resendSourceCodeContainsComboBox->setCurrentIndex(0);
    ui->resendSourceCodeContainsActioncomboBox->setCurrentIndex(0);


    ui->resendHTTPIsComboBox->setCurrentIndex(0);
    ui->resendHTTPCondtionComboBox->setCurrentIndex(0);
    ui->resendHTTPcomboBox->setCurrentIndex(3);
    ui->resendHTTPActioncomboBox->setCurrentIndex(0);

    ui->customDelimiterLineEdit->setText(this->_csvDelimiter);
    ui->customSeparatorLineEdit->setText(this->_csvSeparator);
    ui->dontDedupResultsExpertCheckBox->setChecked(false);

    //ui->specificUaCheckBox->setChecked(false);
    ui->javascriptInterpretationEnable->setChecked(false);
    ui->keepHtmlEncoding->setChecked(false);
    ui->forceOutputEncoding->clear();
    ui->jsonStructure->clear();
    ui->scrapPauseSE->setText("0");
    ui->scrapPausePaginationOnly->setChecked(false);
    ui->customSeInformation->clear();

    ui->requestTypeComboBox->setCurrentIndex(0);
    ui->postArrayEdit->clear();
    ui->specificUaEdit->clear();
    ui->acceptEncodingEdit->clear();
    ui->acceptLanguageEdit->clear();
    ui->hostEdit->clear();
    ui->refererEdit->clear();


}


QHash<QString, QVariant> MainWindow::buildSEConfigHash()
{
    QHash<QString, QVariant> scrapEngineConfig;
    scrapEngineConfig.clear();

    scrapEngineConfig.insert("display_name",ui->customSeDisplayName->text());
    scrapEngineConfig.insert("url",ui->customSeUrl->text());
    scrapEngineConfig.insert("xpath_content",ui->customSeContent->text());
    scrapEngineConfig.insert("xpath_pagination",ui->customSePagination->text());
    scrapEngineConfig.insert("prefix",ui->customSePrefix->text());
    scrapEngineConfig.insert("suffix",ui->customSeSuffix->text());
    //scrapEngineConfig.insert("specificUaActive",ui->specificUaCheckBox->isChecked());
    scrapEngineConfig.insert("jsEnable",ui->javascriptInterpretationEnable->isChecked());
    scrapEngineConfig.insert("keepHtmlEncode",ui->keepHtmlEncoding->isChecked());
    scrapEngineConfig.insert("informations",ui->customSeInformation->toPlainText());

    scrapEngineConfig.insert("jsonStructure",ui->jsonStructure->text());

    // New from 1.7.3
    scrapEngineConfig.insert("pause",ui->scrapPauseSE->text());
    scrapEngineConfig.insert("pausePagination",ui->scrapPausePaginationOnly->isChecked());

    // New from 1.7.4
    scrapEngineConfig.insert("customHeader",this->genCsvLine(this->getSeCustomColumns()));
    scrapEngineConfig.insert("resendReqStr",ui->resendSourceCodeString->text());

    // New from 1.7.5
    scrapEngineConfig.insert("resendReqStrNotContains",ui->resendSourceCodeContainsComboBox->currentIndex());
    scrapEngineConfig.insert("resendReqStrAction",ui->resendSourceCodeContainsActioncomboBox->currentIndex());
    scrapEngineConfig.insert("resendHTTP", ui->resendHTTPIsComboBox->currentIndex());
    scrapEngineConfig.insert("resendHTTPCondition", ui->resendHTTPCondtionComboBox->currentIndex());
    scrapEngineConfig.insert("resendHTTPCode", ui->resendHTTPcomboBox->currentText().toInt());
    scrapEngineConfig.insert("resendHTTPAction",ui->resendHTTPActioncomboBox->currentIndex());
    scrapEngineConfig.insert("csvDelimiter",ui->customDelimiterLineEdit->text());
    scrapEngineConfig.insert("csvSeparator",ui->customSeparatorLineEdit->text());
    scrapEngineConfig.insert("deduplicate",ui->dontDedupResultsExpertCheckBox->isChecked());

    // New from 1.7.6
    scrapEngineConfig.insert("specificUa",(ui->specificUaEdit->text().isEmpty()) ? "" : ui->specificUaEdit->text());
    scrapEngineConfig.insert("acceptLanguage",(ui->acceptLanguageEdit->text().isEmpty()) ? "" : ui->acceptLanguageEdit->text());
    scrapEngineConfig.insert("acceptEncoding",(ui->acceptEncodingEdit->text().isEmpty()) ? "" : ui->acceptEncodingEdit->text());
    scrapEngineConfig.insert("host",(ui->hostEdit->text().isEmpty()) ? "" : ui->hostEdit->text());
    scrapEngineConfig.insert("contentType",(ui->contentTypeEdit->text().isEmpty()) ? "" : ui->contentTypeEdit->text());
    scrapEngineConfig.insert("requestType",ui->requestTypeComboBox->currentText());
    scrapEngineConfig.insert("postArray",(ui->postArrayEdit->text().isEmpty()) ? "" : ui->postArrayEdit->text());
    scrapEngineConfig.insert("sourceCodeType",ui->sourceCodeTypeComboBox->currentIndex());

    // New from 1.7.8
    scrapEngineConfig.insert("forceOutputEncoding",(ui->forceOutputEncoding->text().isEmpty()) ? "" : ui->forceOutputEncoding->text());
    scrapEngineConfig.insert("referer",(ui->refererEdit->text().isEmpty()) ? "" : ui->refererEdit->text());


    scrapEngineConfig.insert("seGroup_id",config->getSEGroupIdByName(ui->scrapEngineGroupComboBox->currentText()));

    return scrapEngineConfig;

}

void MainWindow::on_customSeComboBox_currentIndexChanged(int index)
{
    int seGroupIndex;
    QHash<QString, QVariant> scrapEngineConfig;

    scrapEngineConfig = config->getScrapEngineConfigurationHash(ui->customSeComboBox->itemData(index, Qt::AccessibleTextRole).toInt());


    seGroupIndex = ui->scrapEngineGroupComboBox->findText(config->getSeGroupNameById(scrapEngineConfig.value("seGroup_id").toInt()));
    ui->scrapEngineGroupComboBox->setCurrentIndex(seGroupIndex);

    // We set the csvDelimiter HERE
    if(ui->csvDelimiterLineEdit->text().size())
        _csvDelimiter = ui->csvDelimiterLineEdit->text().at(0);
    if(ui->csvSeparatorLineEdit->text().size())
        _csvSeparator = ui->csvSeparatorLineEdit->text().at(0);

    _csvDelimiter = (!scrapEngineConfig.value("csvDelimiter").toString().isEmpty()) ?  scrapEngineConfig.value("csvDelimiter").toString().at(0) : _csvDelimiter;
    _csvSeparator = (!scrapEngineConfig.value("csvSeparator").toString().isEmpty()) ? scrapEngineConfig.value("csvSeparator").toString().at(0) : _csvSeparator;

    ui->customSeDisplayName->setText(scrapEngineConfig.value("display_name").toString());
    ui->customSeColumnHeaderTable->clearContents();
    this->populateCustomSeColumn(this->getSeCustomColumnsName(scrapEngineConfig.value("customHeader").toString()));
    ui->customSeUrl->setText(scrapEngineConfig.value("url").toString());
    ui->customSeContent->setText(scrapEngineConfig.value("xpath_content").toString());
    ui->customSePagination->setText(scrapEngineConfig.value("xpath_pagination").toString());
    ui->customSePrefix->setText(scrapEngineConfig.value("prefix").toString());
    ui->customSeSuffix->setText(scrapEngineConfig.value("suffix").toString());
    ui->customSeInformation->setText(scrapEngineConfig.value("informations").toString());
    ui->javascriptInterpretationEnable->setChecked(scrapEngineConfig.value("jsEnable").toBool());
    ui->keepHtmlEncoding->setChecked(scrapEngineConfig.value("keepHtmlEncode").toBool());
    ui->sourceCodeTypeComboBox->setCurrentIndex(scrapEngineConfig.value("sourceCodeType").toInt());
    ui->forceOutputEncoding->setText(scrapEngineConfig.value("forceOutputEncoding").toString());
    ui->jsonStructure->setText(scrapEngineConfig.value("jsonStructure").toString());

    // Pause
    ui->scrapPauseSE->setText(scrapEngineConfig.value("pause").toString());
    ui->scrapPausePaginationOnly->setChecked(scrapEngineConfig.value("pausePagination").toBool());

    // For sourceCode actions
    ui->resendSourceCodeString->setText(scrapEngineConfig.value("resendReqStr").toString());
    ui->resendSourceCodeContainsComboBox->setCurrentIndex(scrapEngineConfig.value("resendReqStrNotContains").toInt());
    ui->resendSourceCodeContainsActioncomboBox->setCurrentIndex(scrapEngineConfig.value("resendReqStrAction").toInt());


    // For HTTP actions
    ui->resendHTTPIsComboBox->setCurrentIndex(scrapEngineConfig.value("resendHTTP").toInt());
    ui->resendHTTPCondtionComboBox->setCurrentIndex(scrapEngineConfig.value("resendHTTPCondition").toInt());
    ui->resendHTTPcomboBox->setCurrentText(QString::number(scrapEngineConfig.value("resendHTTPCode").toInt()));
    ui->resendHTTPActioncomboBox->setCurrentIndex(scrapEngineConfig.value("resendHTTPAction").toInt());

    // For Headers section
    ui->specificUaEdit->setText(scrapEngineConfig.value("specificUa").toString());
    ui->acceptEncodingEdit->setText(scrapEngineConfig.value("acceptEncoding").toString());
    ui->acceptLanguageEdit->setText(scrapEngineConfig.value("acceptLanguage").toString());
    ui->hostEdit->setText(scrapEngineConfig.value("host").toString());
    ui->refererEdit->setText(scrapEngineConfig.value("referer").toString());

    ui->contentTypeEdit->setText(scrapEngineConfig.value("contentType").toString());
    ui->postArrayEdit->setText(scrapEngineConfig.value("postArray").toString());
    ui->requestTypeComboBox->setCurrentText(scrapEngineConfig.value("requestType").toString());

    // For overwrite functions
    ui->customDelimiterLineEdit->setText((scrapEngineConfig.value("csvDelimiter", _csvDelimiter).toString().isNull()) ? QString(_csvDelimiter) : scrapEngineConfig.value("csvDelimiter").toString());
    ui->customSeparatorLineEdit->setText((scrapEngineConfig.value("csvSeparator", _csvSeparator).toString().isNull()) ? _csvSeparator : scrapEngineConfig.value("csvSeparator").toString());
    ui->dontDedupResultsExpertCheckBox->setChecked(scrapEngineConfig.value("deduplicate").toBool());
    _customNoDedup = scrapEngineConfig.value("deduplicate").toBool();
}


void MainWindow::on_customSeNewSeButton_clicked()
{
    this->clearCustomSeFields();
}

void MainWindow::on_customSeSaveButton_clicked()
{
    QStringList allDisplayName;
    QHash<QString, QVariant> scrapEngineConfig;
    int newSeId;

    // Only local search engines
    allDisplayName = config->getSeDisplayNames();

    if(allDisplayName.contains(ui->customSeDisplayName->text()))
    {
        appendLog(tr("Custom search engine already exists. Updating"));
        this->on_customSeUpdateButton_clicked();
        return;
    }
    else if(!ui->customSeDisplayName->text().isEmpty())
    {
        // The overwrite csv scheme come here
        if (ui->customSeparatorLineEdit->text().size() > 0)
            _csvSeparator = ui->customSeparatorLineEdit->text().at(0);
        if (ui->customDelimiterLineEdit->text().size() > 0)
            _csvDelimiter = ui->customDelimiterLineEdit->text().at(0);
        _customNoDedup = ui->dontDedupResultsExpertCheckBox->isChecked();

        scrapEngineConfig = this->buildSEConfigHash();
        newSeId = config->insertSE(scrapEngineConfig);
        config->loadSE();

        if(newSeId)
        {
            ui->customSeComboBox->setCurrentIndex(ui->customSeComboBox->findData("local-"+QString::number(newSeId), Qt::WhatsThisRole));
            // If the new scrap engine has a parent, we select it on the top SE combo too
            if(scrapEngineConfig.value("seGroup_id").toInt())
                ui->SESelect->setCurrentIndex(ui->SESelect->findData("local-"+QString::number(newSeId), Qt::WhatsThisRole));
        }

    }

}

void MainWindow::on_customSeUpdateButton_clicked()
{
    QString oldDisplayName, newDisplayName;
    QHash<QString, QVariant> scrapEngineConfig;

    oldDisplayName = ui->customSeComboBox->currentText();
    newDisplayName = ui->customSeDisplayName->text();

    if(ui->customSeComboBox->currentIndex() && !ui->customSeDisplayName->text().isEmpty())
    {
        ui->customSeComboBox->setItemText(ui->customSeComboBox->currentIndex(), newDisplayName);
        ui->SESelect->setItemText(ui->SESelect->findText(oldDisplayName),newDisplayName);

        // The overwrite csv scheme come here
        if (ui->customSeparatorLineEdit->text().size() > 0)
            _csvSeparator = ui->customSeparatorLineEdit->text().at(0);
        if (ui->customDelimiterLineEdit->text().size() > 0)
            _csvDelimiter = ui->customDelimiterLineEdit->text().at(0);
        _customNoDedup = ui->dontDedupResultsExpertCheckBox->isChecked();

        scrapEngineConfig = this->buildSEConfigHash();

        config->updateSeList(scrapEngineConfig,oldDisplayName,"dontupdatecustom");
        ui->seDisplayNameLabel->setText(scrapEngineConfig.value("url").toString());

    }

}

void MainWindow::on_customDeleteButton_clicked()
{
    if(ui->customSeComboBox->currentIndex())
    {
        config->deleteSe(config->getRealSeId(ui->customSeComboBox->currentData(Qt::WhatsThisRole).toString()));
        ui->SESelect->removeItem(ui->SESelect->findText(ui->customSeComboBox->currentText()));
        ui->customSeComboBox->removeItem(ui->customSeComboBox->currentIndex());
        this->clearCustomSeFields();
        ui->customSeComboBox->setCurrentIndex(0);
    }
}

QStringList MainWindow::getSeCustomColumns()
{
    int i;
    QStringList output;
    for (i=0; i < ui->customSeColumnHeaderTable->columnCount();i++)
    {
        if(ui->customSeColumnHeaderTable->item(0,i) && !ui->customSeColumnHeaderTable->item(0,i)->text().isEmpty())
            output.append(ui->customSeColumnHeaderTable->item(0,i)->text());
    }
    return output;
}

QString MainWindow::genCsvLine(QStringList input)
{
    QString output;

    if(!input.size())
        return output;

    foreach (QString data, input) {
        output.append(_csvDelimiter);
        output+=data;
        output.append(_csvDelimiter);
        output.append(_csvSeparator);
    }
    output.chop(1);
    return output;
}

QStringList MainWindow::getSeCustomColumnsName(QString columns)
{
    QStringList columnsNames;

    if(columns.isEmpty())
        return columnsNames;

    columnsNames = columns.split(_csvSeparator);
    columnsNames.replaceInStrings(QString(_csvDelimiter),"");
    return columnsNames;

}

void MainWindow::populateCustomSeColumn(QStringList columnNames)
{
    int i;

    if(columnNames.size())
    {
        for(i = 0;i<columnNames.size();i++)
        {
            QTableWidgetItem *widgetItem = new QTableWidgetItem(columnNames.at(i));
            ui->customSeColumnHeaderTable->setItem(0,i,widgetItem);
        }
    }
}


///////////////////////////
/* Configuration tab */
/////////////////////////

void MainWindow::on_configurationSaveButton_clicked()
{
    int columnToSort;
    QString previousblApiForScrap;
    QHash<QString,QString> newHashConfig;
    bool refreshScrapTabResults;

    refreshScrapTabResults = false;


    /**********************************
        List of Backlinks Services
    **********************************/
    newHashConfig.insert("backlinksCheckMode",ui->backlinksCheckMode->currentText());
    if (!ui->ahrefsApiKeyEdit->text().isEmpty())
        newHashConfig.insert("ahrefsApiKey",ui->ahrefsApiKeyEdit->text());
    else
        newHashConfig.insert("ahrefsApiKey","None");

    if (!ui->majesticAccessTokenEdit->text().isEmpty())
        newHashConfig.insert("majesticAccessToken",ui->majesticAccessTokenEdit->text());
    else
        newHashConfig.insert("mozAccessId","None");
    if (!ui->seobserverApiEdit->text().isEmpty())
        newHashConfig.insert("seobserverApiKey",ui->seobserverApiEdit->text());
    else
        newHashConfig.insert("seobserverApiKey","None");
    if (!ui->mozAccessIdEdit->text().isEmpty())
        newHashConfig.insert("mozAccessId",ui->mozAccessIdEdit->text());
    else
        newHashConfig.insert("mozAccessId","None");
    if (!ui->mozSecretKeyEdit->text().isEmpty())
        newHashConfig.insert("mozSecretKey",ui->mozSecretKeyEdit->text());
    else
        newHashConfig.insert("mozSecretKey","None");


    newHashConfig.insert("ahrefsDedupOn",ui->ahrefsDedupDomainComboBox->currentText());
    newHashConfig.insert("majesticDedupOn",ui->majesticDedupDomainComboBox->currentText());
    newHashConfig.insert("mozDedupOn",ui->mozDedupDomainComboBox->currentText());

    if(ui->backlinksAhrefsOptionRadio->isChecked() && !ui->ahrefsApiKeyEdit->text().isEmpty())
        newHashConfig.insert("backlinksActiveService","Ahrefs");
    else if(ui->backlinksMajesticOptionRadio->isChecked() && !ui->majesticAccessTokenEdit->text().isEmpty())
        newHashConfig.insert("backlinksActiveService","Majestic");
    else if(ui->backlinksMozOptionRadio->isChecked() && (!ui->mozAccessIdEdit->text().isEmpty() && !ui->mozSecretKeyEdit->text().isEmpty()))
        newHashConfig.insert("backlinksActiveService","Moz");
    else if(ui->backlinksSeobserverOptionRadio->isChecked() && !ui->majesticAccessTokenEdit->text().isEmpty())
        newHashConfig.insert("backlinksActiveService","SEObserver");

    newHashConfig.insert("majesticIndex",ui->majesticIndexComboBox->currentText());

    newHashConfig.insert("majesticReferring",(ui->majesticReferringComboBox->currentIndex()) ? QString::number(1) : QString::number(-1));
    newHashConfig.insert("majesticRetrievingMethod", QString::number(ui->majesticRetrievingComboBox->currentIndex()));
    newHashConfig.insert("mozBacklinksMode",ui->backlinksMozCheckMode->currentText());




    /**********************************
         Tierce API
     **********************************/
    if(!ui->whoisAPIKey->text().isEmpty())
        newHashConfig.insert("whoisApiKey"+QString::number(ui->whoisApiComboBox->currentIndex()),ui->whoisAPIKey->text());
    newHashConfig.insert("whoisApiService",ui->whoisApiComboBox->currentText());

    /**********************************
        Global Configuration
    **********************************/

    // On recup les timeout
    if(ui->scrapTimeoutEdit->text().toInt() != 0)
        newHashConfig.insert("scraperTimeout",ui->scrapTimeoutEdit->text());
    if(ui->pageLoadingTimeoutEdit->text().toInt() != 0)
        newHashConfig.insert("pageloadingTimeout",ui->pageLoadingTimeoutEdit->text());
    if(ui->statusCodeTimeoutEdit->text().toInt() != 0)
        newHashConfig.insert("statuscodeTimeout",ui->statusCodeTimeoutEdit->text());
    if(ui->dofollowTimeoutEdit->text().toInt() != 0)
        newHashConfig.insert("dofollowTimeout",ui->dofollowTimeoutEdit->text());
    if(ui->backlinksTimeoutEdit->text().toInt() != 0)
        newHashConfig.insert("backlinksTimeout",ui->backlinksTimeoutEdit->text());
    if(ui->oblTimeoutEdit->text().toInt() != 0)
        newHashConfig.insert("oblTimeout",ui->oblTimeoutEdit->text());
    if(ui->testProxiesTimeoutEdit->text().toInt() != 0)
        newHashConfig.insert("proxiesTestTimeout",ui->testProxiesTimeoutEdit->text());


    // We get global options
    newHashConfig.insert("silentMode",QVariant(ui->silentModeOptionCheckbox->isChecked()).toString());
    newHashConfig.insert("minimalLog",QVariant(ui->minimalLogCheckBox->isChecked()).toString());
    newHashConfig.insert("logFile",QVariant(ui->logFileCheckBox->isChecked()).toString());
    newHashConfig.insert("htmlStack",QVariant(ui->htmlStackCheckBox->isChecked()).toString());
    newHashConfig.insert("autoSaveScrap",QVariant(ui->scrapResultsAutoSaveCheckBox->isChecked()).toString());
    newHashConfig.insert("lineBreak", QString::number(ui->lineBreakComboBox->currentIndex()));
    newHashConfig.insert("txtSeparator",(ui->textSeparatorLineEdit->text().isEmpty() ? "|" :  ui->textSeparatorLineEdit->text()));
    newHashConfig.insert("csvSeparator",(ui->csvSeparatorLineEdit->text().isEmpty() ? "," :  ui->csvSeparatorLineEdit->text()));
    newHashConfig.insert("csvDelimiter",(ui->csvDelimiterLineEdit->text().isEmpty() ? "\"" :  ui->csvDelimiterLineEdit->text()));

    // We set the linebreak
    lineBreak = (ui->lineBreakComboBox->currentIndex()) ? "\n" : "\r\n";
    // And delimiter
    txtDelimiter = ui->textSeparatorLineEdit->text().at(0);
    _csvSeparator = ui->csvSeparatorLineEdit->text().at(0);
    _csvDelimiter = ui->csvDelimiterLineEdit->text().at(0);
    this->updateDelimiters();


    previousblApiForScrap = this->config->getConfigurationValue("blApiForScrap","Free");
    columnToSort = (ui->disableAutoSort->isChecked() == true) ? -1 : 0;
    newHashConfig.insert("disableAutoSort",QString::number(columnToSort));
    newHashConfig.insert("disableDedupe",QVariant(ui->disableDedupe->isChecked()).toString());

    newHashConfig.insert("autosaveSeConfig",QVariant(ui->autoSaveCustomsOptionCheckbox->isChecked()).toString());
    newHashConfig.insert("keepCustom1OnChange",QVariant(ui->keepCustom1OptionCheckbox->isChecked()).toString());
    newHashConfig.insert("keepCustom2OnChange",QVariant(ui->keepCustom2OptionCheckbox->isChecked()).toString());
    newHashConfig.insert("keepScrapResults",QVariant(ui->keepScrapResultCheckbox->isChecked()).toString());

    blApiForScrap = ui->scrapBacklinkServiceComboBox->currentText();

    // We refresh the scrapTabResult if the BL metrics API is different than  that in the config file
    if(previousblApiForScrap != blApiForScrap)
    {
        int returnButton = QMessageBox::warning(this,tr("Cleaning scrap results"),tr("<b>Warning !!</b> You're about to change the API for backlinks metrics. This will <b>clear ALL the results</b> in the \"Scrap Results\" tab. Please, be sure to have save all your results before doing this. Click OK if you've already saved your results, otherwise click Cancel."), (QMessageBox::Cancel | QMessageBox::Ok));

        if(returnButton == QMessageBox::Ok)
        {
            refreshScrapTabResults = true;
            newHashConfig.insert("blApiForScrap",QVariant(ui->scrapBacklinkServiceComboBox->currentText()).toString());
        }
        else
            ui->scrapBacklinkServiceComboBox->setCurrentText(previousblApiForScrap);
    }

    newHashConfig.insert("freeDedupOn",ui->dedupDomainBasedOnComboBox->currentText());

    newHashConfig.insert("retryOnTimeout",QVariant(ui->retryOnTimeout->isChecked()).toString());

    config->updateConfiguration(newHashConfig);

    if(config->getConfigurationValue("silentMode") == "false")
        QMessageBox::information(this,tr("Configuration"),tr("Configuration saved successfully"));
    else
        ui->statusBar->showMessage(tr("Configuration saved successfully"), 2000);


    if(refreshScrapTabResults == true)
        this->buildScrapTabResults("scrap",blApiForScrap, QStringList() << "Backlinks");

}


void MainWindow::on_ahrefsBalanceButton_clicked()
{
    if(!ui->ahrefsApiKeyEdit->text().isEmpty())
    {
        this->initScrapClass();
        this->scrapButtonDisabled();
        scraper->backlinks->getAhrefsBalance(ui->ahrefsApiKeyEdit->text());
    }
}

void MainWindow::on_majesticBalanceButton_clicked()
{
    if(!ui->majesticAccessTokenEdit->text().isEmpty())
    {
        this->initScrapClass();
        this->scrapButtonDisabled();
        if(ui->backlinksMajesticOptionRadio->isChecked())
            scraper->backlinks->getMajesticBalance(ui->majesticAccessTokenEdit->text(),"Majestic");
        else if (ui->backlinksSeobserverOptionRadio->isChecked())
            scraper->backlinks->getMajesticBalance(ui->seobserverApiEdit->text(),"SEObserver");
    }
}


void MainWindow::updateProgressBar(int current, int total, QString label)
{
    ui->taskProgressBar->setMinimum(0);
    if(!current && !total)
        ui->taskProgressBar->setMaximum(0);
    else
    {
        int percent = (current / total) * 100;
        ui->taskProgressBar->setMaximum(100);
        ui->taskProgressBar->setValue(percent);
    }

    if(!label.isEmpty())
        ui->taskProgressBarLabel->setText(label);

}

void MainWindow::showSystrayMsgSlot(QString msg)
{
    if(QSystemTrayIcon::isSystemTrayAvailable())
        this->sysTrayIcon.showMessage("RDDZ Scraper", msg+"\n"+QString::number(this->scraptable->scrapResults.size())+ tr(" results"));
}



/**
 * @brief MainWindow::customContextMenuRequestedSlot
 * @param pos
 * Context menu for tableView
 */
void MainWindow::customContextMenuRequestedSlot(QPoint pos)
{
    QModelIndex index = ui->listResult->indexAt(pos);

    QList<QAction *> copyActions;
    QList<QAction *> exportActions;
    QList<QAction *> clearActions;

    QMenu *menu=new QMenu(this);
    QMenu *exportMenu = new QMenu(this);
    QMenu *clearMenu = new QMenu(this);
    QMenu *copyMenu = new QMenu(this);
    exportMenu->setTitle(tr("Export ..."));
    clearMenu->setTitle(tr("Clear ..."));
    copyMenu->setTitle(tr("Copy ..."));


    QAction *doSelectAllAction = new QAction(tr("Select all items"), this);
    QAction *doDeselectAllAction = new QAction(tr("Deselect all items"), this);

    QAction *doFullCopyAction = new QAction(tr("All selected items"), this);
    QAction *doUrlCopyAction = new QAction(tr("All selected items in column %1").arg(this->scraptable->tableHeaderData.value(index.column())), this);
    QAction *doColumnCopyAction = new QAction(tr("Column ") + this->scraptable->tableHeaderData.value(index.column()), this);

    QAction *doPasteAction = new QAction(tr("Paste"), this);

    QAction *transferToCustom1 = new QAction(tr("Transfer column %1 to custom1").arg(this->scraptable->tableHeaderData.value(index.column())), this);
    transferToCustom1->setData(index.column());
    QAction *transferToCustom1Diff = new QAction(tr("Transfer column %1 to custom1 (diff mode)").arg(this->scraptable->tableHeaderData.value(index.column())), this);
    transferToCustom1Diff->setData(index.column());

    QAction *doImportAction = new QAction(tr("Import"), this);
    QAction *doUrlExportAction = new QAction(tr("URL only"), this);
    //    QAction *doUrlPrExportAction = new QAction(tr("URL and PR only"), this);
    QAction *doFullExportAction = new QAction(tr("Full"), this);

    QAction *doClearAllAction = new QAction(tr("All"), this);
    QAction *doClearSelectedAction = new QAction(tr("Selected"), this);
    QAction *doClearColumnAction = new QAction(tr("Column ") + this->scraptable->tableHeaderData.value(index.column()), this);

    this->scraptable->columnClicked =  index.column();

    // Build submenu for Copy
    copyActions.append(doFullCopyAction);
    copyActions.append(doUrlCopyAction);
    copyActions.append(doColumnCopyAction);
    copyMenu->addActions(copyActions);

    // Build submenu for Export
    exportActions.append(doUrlExportAction);
    exportActions.append(doFullExportAction);
    exportMenu->addActions(exportActions);


    // Build submenu for Clear
    clearActions.append(doClearAllAction);
    clearActions.append(doClearSelectedAction);
    clearActions.append(doClearColumnAction);
    clearMenu->addActions(clearActions);


    menu->addAction(doSelectAllAction);
    menu->addAction(doDeselectAllAction);
    menu->addSeparator();
    menu->addMenu(clearMenu);
    menu->addSeparator();
    menu->addMenu(copyMenu);
    menu->addSeparator();
    menu->addAction(transferToCustom1);
    menu->addAction(transferToCustom1Diff);
    menu->addSeparator();
    menu->addAction(doPasteAction);
    menu->addSeparator();
    menu->addAction(doImportAction);
    menu->addMenu(exportMenu);


    // Now we connect ALL the actions !!
    QObject::connect(doSelectAllAction,SIGNAL(triggered()),ui->listResult,SLOT(selectAll()));
    QObject::connect(doDeselectAllAction,SIGNAL(triggered()),ui->listResult,SLOT(clearSelection()));

    QObject::connect(doClearAllAction,SIGNAL(triggered()),this,SLOT(on_ClearUrlList_clicked()));
    QObject::connect(doClearSelectedAction,SIGNAL(triggered()),this,SLOT(on_removeSelectedUrlButton_clicked()));
    QObject::connect(doClearColumnAction,SIGNAL(triggered()),this->scraptable,SLOT(clearColumn()));

    QObject::connect(doFullCopyAction,SIGNAL(triggered()),this,SLOT(doFullCopy()));
    QObject::connect(doUrlCopyAction,SIGNAL(triggered()),this,SLOT(doUrlCopy()));
    QObject::connect(doColumnCopyAction,SIGNAL(triggered()),this,SLOT(doColumnCopy()));

    QObject::connect(doPasteAction,SIGNAL(triggered()),this,SLOT(doPaste()));

    QObject::connect(transferToCustom1,SIGNAL(triggered()),this,SLOT(transferToCustom1()));
    QObject::connect(transferToCustom1Diff,SIGNAL(triggered()),this,SLOT(transferNonExistingToCustom1()));

    QObject::connect(doImportAction,SIGNAL(triggered()),this,SLOT(importScrapButtonEmulate()));
    QObject::connect(doUrlExportAction,SIGNAL(triggered()),this,SLOT(stdScrapExportButtonEmulate()));
    QObject::connect(doFullExportAction,SIGNAL(triggered()),this,SLOT(fullScrapExportButtonEmulate()));


    menu->popup(ui->listResult->viewport()->mapToGlobal(pos));

}

void MainWindow::on_whoisApiComboBox_currentIndexChanged(int index)
{
    if(index >= 0 && _configLoaded == true)
        ui->whoisAPIKey->setText(this->config->getConfigurationValue("whoisApiKey"+QString::number(index)));
}

//////////////////////////////////////////////
/* Advanced tab  (Configuration extension) */
////////////////////////////////////////////

void MainWindow::on_saveAdvancedConfiguration_clicked()
{
    this->on_configurationSaveButton_clicked();
}

/**
 * @brief MainWindow::on_saveColumnButton_clicked
 *
 * Column Tab Configuration
 */

void MainWindow::on_saveColumnButton_clicked()
{
    this->config->writeGeneralConfig("StdColumn","HTTP",QVariant(ui->displayColumnHttp->isChecked()).toString());
    this->config->writeGeneralConfig("StdColumn","DF",QVariant(ui->displayColumnDofollow->isChecked()).toString());
    this->config->writeGeneralConfig("StdColumn","BL",QVariant(ui->displayColumnBacklinks->isChecked()).toString());
    this->config->writeGeneralConfig("StdColumn","OBL",QVariant(ui->displayColumnObl->isChecked()).toString());
    this->config->writeGeneralConfig("StdColumn","Platform",QVariant(ui->displayColumnPlatform->isChecked()).toString());
    this->config->writeGeneralConfig("StdColumn","IP",QVariant(ui->displayColumnIP->isChecked()).toString());
    this->config->writeGeneralConfig("StdColumn","LinkAlive",QVariant(ui->displayColumnLinkAlive->isChecked()).toString());
    this->config->writeGeneralConfig("StdColumn","DomainAvailable",QVariant(ui->displayColumnDomainAvailable->isChecked()).toString());


    ui->httpButton->setEnabled(this->config->readGeneralConfig("StdColumn","HTTP",true).toBool());
    ui->resolveRedirectButton->setEnabled(this->config->readGeneralConfig("StdColumn","HTTP",true).toBool());
    ui->checkDofollowNofollow->setEnabled(this->config->readGeneralConfig("StdColumn","DF",true).toBool());
    ui->toolBacklinksButton->setEnabled(this->config->readGeneralConfig("StdColumn","BL",true).toBool());
    ui->getOblButton->setEnabled(this->config->readGeneralConfig("StdColumn","OBL",true).toBool());
    ui->getPlatformButton->setEnabled(this->config->readGeneralConfig("StdColumn","Platform",true).toBool());
    ui->getIpAddrButton->setEnabled(this->config->readGeneralConfig("StdColumn","IP",true).toBool());
    ui->linkAliveButton->setEnabled(this->config->readGeneralConfig("StdColumn","LinkAlive",true).toBool());
    ui->whoisButton->setEnabled(this->config->readGeneralConfig("StdColumn","DomainAvailable",true).toBool());


    // Moz

    this->config->writeGeneralConfig("mozApiColumn","Anchor",QVariant(ui->displayMozColumnAnchor->isChecked()).toString());
    this->config->writeGeneralConfig("mozApiColumn","Backlinks",QVariant(ui->displayMozColumnBacklinks->isChecked()).toString());
    this->config->writeGeneralConfig("mozApiColumn","Mozrank",QVariant(ui->displayMozColumnMozrank->isChecked()).toString());
    this->config->writeGeneralConfig("mozApiColumn","PA",QVariant(ui->displayMozColumnPa->isChecked()).toString());
    this->config->writeGeneralConfig("mozApiColumn","DA",QVariant(ui->displayMozColumnDa->isChecked()).toString());


    // Ahrefs
    this->config->writeGeneralConfig("ahrefsApiColumn","Anchor",QVariant(ui->displayAhrefsColumnAnchor->isChecked()).toString());
    this->config->writeGeneralConfig("ahrefsApiColumn","TargetUrl",QVariant(ui->displayAhrefsColumnTargetUrl->isChecked()).toString());
    this->config->writeGeneralConfig("ahrefsApiColumn","LinkType",QVariant(ui->displayAhrefsColumnLinkType->isChecked()).toString());
    this->config->writeGeneralConfig("ahrefsApiColumn","UrlRank",QVariant(ui->displayAhrefsColumnUrlRank->isChecked()).toString());
    this->config->writeGeneralConfig("ahrefsApiColumn","DomainRank",QVariant(ui->displayAhrefsColumnDomainRank->isChecked()).toString());
    this->config->writeGeneralConfig("ahrefsApiColumn","Nofollow",QVariant(ui->displayAhrefsColumnNofollow->isChecked()).toString());



    // Majestic
    this->config->writeGeneralConfig("majesticApiColumn","Anchor",QVariant(ui->displayMajesticColumnAnchor->isChecked()).toString());
    this->config->writeGeneralConfig("majesticApiColumn","LinkType",QVariant(ui->displayMajesticColumnLinkType->isChecked()).toString());
    this->config->writeGeneralConfig("majesticApiColumn","TargetUrl",QVariant(ui->displayMajesticColumnTargetUrl->isChecked()).toString());
    this->config->writeGeneralConfig("majesticApiColumn","Deleted",QVariant(ui->displayMajesticColumnDeleted->isChecked()).toString());
    this->config->writeGeneralConfig("majesticApiColumn","Nofollow",QVariant(ui->displayMajesticColumnNofollow->isChecked()).toString());
    this->config->writeGeneralConfig("majesticApiColumn","Backlinks",QVariant(ui->displayMajesticColumnBacklinks->isChecked()).toString());
    this->config->writeGeneralConfig("majesticApiColumn","CF",QVariant(ui->displayMajesticColumnCf->isChecked()).toString());
    this->config->writeGeneralConfig("majesticApiColumn","TF",QVariant(ui->displayMajesticColumnTf->isChecked()).toString());
    this->config->writeGeneralConfig("majesticApiColumn","TTFT",QVariant(ui->displayMajesticColumnTtft->isChecked()).toString());
    this->config->writeGeneralConfig("majesticApiColumn","TTFV",QVariant(ui->displayMajesticColumnTtfv->isChecked()).toString());

    this->action.clear();
    this->buildScrapTabResults("scrap",ui->scrapBacklinkServiceComboBox->currentText());

}

///////////////////////////
/* Logs tab */
/////////////////////////

void MainWindow::on_clearLogsButton_clicked()
{
    ui->logResults->clear();
}


void MainWindow::openUrlWidget(QModelIndex modelIndex)
{
    QStringList elements = this->scraptable->getItemByIndex(modelIndex).toString().split(" ");

    foreach(QString url, elements)
    {
        if(url.contains(QRegExp("^https?://.*$")))
        {
            QDesktopServices::openUrl(QUrl(url.trimmed()));
            return;
        }
    }
    return;
}

///////////////////////////
/* List of public slots */
/////////////////////////

void MainWindow::addFields(QMap<int,QList<QVariant> > map, int resultCount)
{
    int startAt;
    QMapIterator<int,QList<QVariant> > mapiterator(map);

    startAt = this->scraptable->scrapResults.size();


    mapiterator.toFront();
    while(mapiterator.hasNext())
    {
        mapiterator.next();
        this->scraptable->buildScrapResultsHash(mapiterator.value(),mapiterator.key(),startAt);
    }

    emit updateNbUrlSignal();

    if (!resultCount)
    {
        this->scraptable->updateLayout();

        if (    config->getAutoGetStatus() ||
                config->getAutoRemoveBadUrl() ||
                config->getAutoResolveRedir())
            autoHttpStatus();
        else if(config->getAutoDF())
            autoCheckDofollowNofollow();
        else
            scrapButtonEnabled();
    }
}


void MainWindow::removeBadUrl(bool enableButtons, bool silentMode)
{
    int response, itemsRemoved;

    if (this->scraptable->getNbItemsByCol(this->scraptable->columnIndexes.value("httpCol")) == 0)
    {
        response = QMessageBox::information(this, tr("No HTTP Header found"), tr("You must first launch an HTTP header check in order to remove the bad urls. Would you like to run it now ?"),QMessageBox::Yes | QMessageBox::No);
        if(response == QMessageBox::No)
        {
            emit restoreCursor();
            return ;
        }
        this->on_httpButton_clicked();
        return ;
    }

    itemsRemoved = scraptable->removeBadUrls();
    emit setBusyCursor();
    if(!itemsRemoved)
        emit noDuplicateUrl(silentMode,tr("Bad url already removed"),tr("Remove bad url"));
    else
        emit deleteItemsFromTable(itemsRemoved,enableButtons,silentMode);

    emit restoreCursor();
    emit updateNbUrlSignal();
}

void MainWindow::removeDuplicateUrlSlot(bool enableButtons, bool silentMode,bool clickedButton)
{
    if(config->getConfigurationValue("disableDedupe") == "true" && clickedButton == false)
        return;

    // Since 1.7.5 we can overwrite the disable dedup !!
    if(_customNoDedup == true)
        return;

    int itemsRemoved;
    itemsRemoved = scraptable->removeDuplicateUrl();

    emit setBusyCursor();
    if(!itemsRemoved)
        emit noDuplicateUrl(silentMode);
    else
    {
        if(config->getConfigurationValue("disableDedupe") == "false")
            appendLog(tr("[Auto remove duplicate URL] ")+QString::number(itemsRemoved)+tr(" items removed"));
        emit deleteItemsFromTable(itemsRemoved,enableButtons,silentMode);
    }
    emit updateNbUrlSignal();
    emit restoreCursor();
}

void MainWindow::trimToRoot()
{
    emit setBusyCursor();
    this->scraptable->trimToRoot();
    emit updateNbUrlSignal();
    emit restoreCursor();
}

void MainWindow::trimToLastFolder()
{
    emit setBusyCursor();
    this->scraptable->trimToLastFolder();
    emit updateNbUrlSignal();
    emit restoreCursor();
}

void MainWindow::keepOnlyDomains()
{

    emit setBusyCursor();
    this->scraptable->keepOnlyDomain();
    emit updateNbUrlSignal();
    emit restoreCursor();
}

void MainWindow::removeDuplicateDomains()
{
    int itemsRemoved;
    QString metricCol, apiUsed;

    if(this->action == "scrap")
        apiUsed = config->getConfigurationValue("blApiForScrap","Free");
    else
        apiUsed =  this->scraptable->currentTableModel;


    // We get the metric in terms of BL API
    if(apiUsed == "Free")
        metricCol = config->getConfigurationValue("freeDedupOn","PR");
    else if (apiUsed == "Ahrefs")
        metricCol = config->getConfigurationValue("ahrefsDedupOn","PR");
    else if (apiUsed == "Majestic")
        metricCol = config->getConfigurationValue("majesticDedupOn","PR");
    else if (apiUsed == "Moz")
        metricCol = config->getConfigurationValue("mozDedupOn","PR");
    else metricCol = "PR";

    emit setBusyCursor();
    itemsRemoved = this->scraptable->removeDuplicateDomain(metricCol,apiUsed);
    emit updateNbUrlSignal();
    emit restoreCursor();

    if(!itemsRemoved)
        emit noDuplicateUrl(false,tr("Duplicate domains already removed"), tr("Remove duplicate domains"));
    else
        emit deleteItemsFromTable(itemsRemoved, true,false);

    emit enableButtonsSignal();
}

void MainWindow::scrapButtonEnabled()
{
    emit enableButtonsSignal();
    emit restoreCursor();
}

void MainWindow::scrapButtonDisabled()
{
    emit disableButtonsSignal();
    emit setBusyCursor();
}

void MainWindow::updateCustom1(QString list)
{
    ui->custom1List->document()->setPlainText(list);

}

void MainWindow::updateCustom2(QString list)
{
    ui->custom2List->document()->setPlainText(list);
}

void MainWindow::addComboParentItem(QStandardItemModel * model, const QString& text)
{
    QString itemLabel = text;
    QStandardItem* item = new QStandardItem( itemLabel );
    item->setFlags( item->flags() & ~( Qt::ItemIsEnabled | Qt::ItemIsSelectable ) );
    item->setData( "parent", Qt::AccessibleDescriptionRole );
    QFont font = item->font();
    font.setBold( true );
    item->setFont( font );
    model->appendRow( item );
}

void MainWindow::addComboChildItem(QStandardItemModel * model, const QString& text, const QVariant& data)
{
    QStandardItem* item = new QStandardItem(text);
    item->setData(data, Qt::UserRole );
    item->setData("child", Qt::AccessibleDescriptionRole );
    item->setData(data.toInt(),Qt::AccessibleTextRole);
    item->setData("local-"+data.toString(),Qt::WhatsThisRole);

    model->appendRow(item);
}

void MainWindow::addComboSeparator(QStandardItemModel * model)
{
    QStandardItem* item = new QStandardItem();
    item->setData("separator", Qt::AccessibleDescriptionRole);
    model->appendRow(item);
}

void MainWindow::updateCustomSeCombo(QMap<QString, QPair<QString, int> > SEMap, QString match)
{
    int selectIndex;
    QString currentSeGroup;
//    QString label = tr("Add new");

    QStandardItemModel * expertModemodel = new QStandardItemModel;
    QStandardItemModel * topComboSeModel = new QStandardItemModel;

    ui->customSeComboBox->clear();
    ui->SESelect->clear();
    currentSeGroup = "";

    // We get all values from each key
    // For user scrap engines
    QStringList groupNames;
    QList<QPair<QString, int> > childNames;
    groupNames = SEMap.uniqueKeys();

    foreach (QString groupName, groupNames)
    {
        if(!groupName.isEmpty() && config->getSeGroupStatus(groupName))
        {
            addComboParentItem(topComboSeModel, groupName);
        }
        addComboSeparator(expertModemodel);
        addComboParentItem(expertModemodel, groupName);
        childNames = SEMap.values(groupName);
        foreach (auto childName, childNames) {
            addComboChildItem(expertModemodel,childName.first,childName.second);
            if(!groupName.isEmpty() && config->getSeGroupStatus(childName.first))
            {
                addComboChildItem(topComboSeModel, childName.first,childName.second);
            }
        }
    }

    ui->SESelect->setModel(topComboSeModel);
    ui->SESelect->setItemDelegate(new ComboBoxDelegate);

    ui->customSeComboBox->setModel(expertModemodel);
    ui->customSeComboBox->setItemDelegate(new ComboBoxDelegate);

    ui->scrapEngineGroupComboBox->clear();
    ui->scrapEngineGroupComboBox->addItem(tr("unclassified"));
    ui->scrapEngineGroupComboBox->addItems(config->getSeGroupNames());

    selectIndex = ui->SESelect->findData(match, Qt::WhatsThisRole);

    ui->SESelect->setCurrentIndex(selectIndex);
    selectIndex = ui->customSeComboBox->findData(match, Qt::WhatsThisRole);
    ui->customSeComboBox->setCurrentIndex(selectIndex);

    ui->scrapEngineGroupComboBox->setCurrentText(config->getSeGroupNameById(config->getSeGroupIdByChildName(match)));
}

void MainWindow::updateHttpStatus(QString url, int httpstatus)
{
    QtConcurrent::run(this->scraptable, &scrapTable::updateTableView, url, httpstatus, scraptable->columnIndexes.value("httpCol"));
}

void MainWindow::UpdateRelUrls(QString url, double rel)
{
    QtConcurrent::run(this->scraptable, &scrapTable::updateTableView, url, rel, scraptable->columnIndexes.value("dfCol"));
}


void MainWindow::updateBacklinks(QString url, qlonglong bls)
{
    QtConcurrent::run(this->scraptable, &scrapTable::updateTableView, url, bls, scraptable->columnIndexes.value("blCol"));
}

void MainWindow::updateCitationFlow(QString url, int cf)
{
    QtConcurrent::run(this->scraptable, &scrapTable::updateTableView, url, cf, scraptable->columnIndexes.value("cfCol"));
}

void MainWindow::updateTrustFlow(QString url, int tf)
{
    QtConcurrent::run(this->scraptable, &scrapTable::updateTableView, url, tf, scraptable->columnIndexes.value("tfCol"));
}

void MainWindow::updateTableViewSlot(QString url, QVariant value, int col)
{
    QtConcurrent::run(this->scraptable, &scrapTable::updateTableView, url, value, col);
}

void MainWindow::updateTableMappingViewSlot(QPair<QString,QString> firstPair, QPair<QString,QString> secondPair, QVariant value, int col)
{
    QtConcurrent::run(this->scraptable, &scrapTable::updateTableViewMapping, firstPair, secondPair, value, col);
}


void MainWindow::updateMultipleCol(QString url, QHash<int, QVariant> values, bool basedOnUrl)
{
    QtConcurrent::run(this->scraptable, &scrapTable::updateMultipleColumn, url, values,basedOnUrl,_getDomainOnly);
}

void MainWindow::updateMultipleRows(QHash<QString, QHash<int, QVariant> > values, bool basedOnUrl)
{
    QtConcurrent::run(this->scraptable, &scrapTable::updateMultipleRows, values, basedOnUrl,_getDomainOnly);
}

void MainWindow::updateObl(QString url, int obl)
{
    QtConcurrent::run(this->scraptable, &scrapTable::updateTableView, url, obl, scraptable->columnIndexes.value("oblCol"));
}

void MainWindow::updateIpAddr(QString url, QString addr)
{
    QtConcurrent::run(this->scraptable, &scrapTable::updateTableView, url, addr, scraptable->columnIndexes.value("ipCol"));
}

void MainWindow::updateLinkAlive(QString url, QString status)
{
    QtConcurrent::run(this->scraptable, &scrapTable::updateTableView, url, status, scraptable->columnIndexes.value("linkAliveCol"));
}

void MainWindow::updatePlatform(QString url, QString platform)
{
    // We split the platform by |
    QStringList platformInfos = platform.split("|");
    QString platformCat = (platformInfos.size() != 2) ? tr("Not found") : platformInfos.at(0);
    QString platformName = (platformInfos.size() != 2) ? tr("Not found") : platformInfos.at(1);

    QtConcurrent::run(this->scraptable, &scrapTable::updateTableView, url, platformCat, scraptable->columnIndexes.value("platformCatCol"));
    QtConcurrent::run(this->scraptable, &scrapTable::updateTableView, url, platformName, scraptable->columnIndexes.value("platformCol"));
}


void MainWindow::updateDomainAvailable(QString domain, QVariant value, int columnIndex)
{
    if(columnIndex == -1)
        return;

    QtConcurrent::run(this->scraptable, &scrapTable::updateAllDomains, domain, value, columnIndex,_getDomainOnly);
}

void MainWindow::updateNbUrl(int nbUrls)
{
    if(nbUrls == -1)
        ui->nbUrls->setText(QString::number(scraptable->scrapResults.size())+" urls");
    else
        ui->nbUrls->setText(QString::number(nbUrls)+" urls");
}


void MainWindow::updateUrl(QString oldUrl, QString newUrl)
{
    QtConcurrent::run(this->scraptable, &scrapTable::updateTableView, oldUrl, newUrl, scraptable->columnIndexes.value("urlCol"));
}

void MainWindow::insertProxies(QStringList proxies)
{
    this->updateProxiesList(proxies);
}

void MainWindow::updateLastOperationTime()
{
    QTime currentElapsedTime = lastOperationTimeStart;
    lastOperationTimeStart = currentElapsedTime.addSecs(1);
    elapsedTimeLabel->setText(lastOperationTimeStart.toString("hh:mm:ss"));
}

void MainWindow::disableButtons()
{

    lastOperationTimeStart.setHMS(0,0,0);
    elapsedTimeLabel->setText("00:00:00");
    lastOperationTimer->start(1000);

    ui->listResult->setEnabled(false);
    ui->SendFootprint->setEnabled(false);
    ui->backlinksButton->setEnabled(false);
    ui->toolBox->setEnabled(false);
    ui->ClearUrlList->setEnabled(false);
    // Proxies
    //ui->proxiesGroupBox->setEnabled(false);
    ui->proxiesButtonGroupBox->setEnabled(false);
    ui->majesticBalanceButton->setEnabled(false);
    ui->ahrefsBalanceButton->setEnabled(false);
}

void MainWindow::enableButtons()
{
    lastOperationTimer->stop();
    if(this->updateViewTimer->isActive())
        this->updateViewTimer->stop();


    ui->listResult->setEnabled(true);
    ui->SendFootprint->setEnabled(true);
    ui->backlinksButton->setEnabled(true);
    ui->ClearUrlList->setEnabled(true);
    ui->toolBox->setEnabled(true);
    // Proxies
    ui->proxiesButtonGroupBox->setEnabled(true);
    ui->majesticBalanceButton->setEnabled(true);
    ui->ahrefsBalanceButton->setEnabled(true);

    emit restoreCursor();
    emit restoreSorting();
}

void MainWindow::displayMsgSlot(QString msg)
{
    if(config->getConfigurationValue("silentMode") == "false")
        QMessageBox::information(this,tr("Information"),msg);
    else
        ui->statusBar->showMessage(msg, 5000);
}

void MainWindow::restoreSortingSlot()
{
    QApplication::processEvents();

    if(config->getConfigurationValue("disableAutoSort").toInt() != -1)
    {
        ui->listResult->sortByColumn(config->getConfigurationValue("disableAutoSort").toInt(), Qt::AscendingOrder);
    }
}

void MainWindow::appendLog(QString text, QString logStyle)
{

    QString fulldate    = QDateTime::currentDateTime().toString("yyyy.MM.dd hh:mm:ss");
    if(logStyle == "default")
    {
        QTextCharFormat tf;
        tf = ui->logResults->currentCharFormat();
        tf.setForeground(QBrush(static_cast<Qt::GlobalColor>(Qt::black)));
        ui->logResults->setCurrentCharFormat(tf);
    }
    else if(logStyle == "error")
    {
        QTextCharFormat tf;
        tf = ui->logResults->currentCharFormat();
        tf.setForeground(QBrush(static_cast<Qt::GlobalColor>(Qt::red)));
        ui->logResults->setCurrentCharFormat(tf);
    }
    else if(logStyle == "success")
    {
        QTextCharFormat tf;
        tf = ui->logResults->currentCharFormat();
        tf.setForeground(QBrush(QColor(27,145,72)));
        ui->logResults->setCurrentCharFormat(tf);
    }
    else if(logStyle == "warning")
    {
        QTextCharFormat tf;
        tf = ui->logResults->currentCharFormat();
        tf.setForeground(QBrush(QColor(201,117,9)));
        ui->logResults->setCurrentCharFormat(tf);
    }
    else if(logStyle == "xpath")
    {
        QTextCharFormat tf;
        tf = ui->logResults->currentCharFormat();
        tf.setForeground(QBrush(QColor(0,150,14)));
        ui->logResults->setCurrentCharFormat(tf);
    }
    else if(logStyle == "fulldebug")
    {
        QTextCharFormat tf;
        tf = ui->logResults->currentCharFormat();
        tf.setForeground(QBrush(QColor(57,12,184)));
        ui->logResults->setCurrentCharFormat(tf);
    }
    else if(logStyle == "information")
    {
        QTextCharFormat tf;
        tf = ui->logResults->currentCharFormat();
        tf.setForeground(QBrush(QColor(64,61,227)));
        ui->logResults->setCurrentCharFormat(tf);
    }

    ui->logResults->appendPlainText("["+fulldate+"] "+text);


    if(config->getConfigurationValue("logFile") == "true")
    {
        QString today       = QDateTime::currentDateTime().toString("yyyy-MM-dd");

        QFile f(QApplication::applicationDirPath()+"/logs"+today+".txt");
        if (f.open(QFile::WriteOnly | QFile::Append))
        {
            QTextStream out(&f);
            out << "["+fulldate+"] "+text << lineBreak;
        }
    }
}

void MainWindow::populateFootprints(QStringList footprints)
{
    ui->footprintComboBox->addItems(footprints);
}

void MainWindow::clearFootprints()
{
    ui->footprintComboBox->clear();
}

void MainWindow::autoResolveRedirect()
{
    this->on_resolveRedirectButton_clicked();
}

void MainWindow::autoHttpStatus()
{
    this->on_httpButton_clicked();
}

void MainWindow::autoCheckDofollowNofollow()
{
    this->on_checkDofollowNofollow_clicked();
}

void MainWindow::autoBacklinks()
{
    if(this->action == "scrap")
    {
        getBacklinksMethod = config->getConfigurationValue("autogetBacklinksMethod").toInt();
        this->getToolBacklinksButton();
    }
    return;
}

void MainWindow::autoObl()
{
    this->on_getOblButton_clicked();
}

void MainWindow::autoIp()
{
    this->on_getIpAddrButton_clicked();
}

void MainWindow::setBusyCursorSlot()
{
    this->setCursor(Qt::BusyCursor);
}

void MainWindow::restoreCursorSlot()
{
    this->setCursor(Qt::ArrowCursor);
    this->scraptable->updateLayout();
}


void MainWindow::updateBacklinksBalanceUnits(QString units, QString service)
{
    if(service == "ahrefs")
        ui->ahrefsBalanceText->setText(units);
    else if(service == "majesticTotalAnalysisResUnits")
        ui->majesticTotalAnalysisResUnits->setText(units);
    else if(service == "majesticTotalIndexItemInfoResUnits")
        ui->majesticTotalIndexItemInfoResUnits->setText(units);
    else if(service == "majesticTotalRetrievalResUnits")
        ui->majesticTotalRetrievalResUnits->setText(units);
    else
        return;
}


void MainWindow::noDuplicateUrlSlot(bool silentMode, QString msg, QString title)
{
    if(silentMode==false)
    {
        if(config->getConfigurationValue("silentMode") == "false")
            QMessageBox::information(this,title,msg,QMessageBox::Ok);
        else
            ui->statusBar->showMessage(msg, 2000);
    }
}


void MainWindow::deleteItemsFromTableSlot(int itemsRemoved, bool enableButtons, bool silentMode)
{
    QString msg;
    msg = QString::number(itemsRemoved)+" ";
    msg += (itemsRemoved == 1) ? tr("element deleted") : tr("elements deleted");

    if(enableButtons == true)
        this->enableButtons();

    if(!itemsRemoved)
        return;

    if(silentMode==false)
    {
        if(config->getConfigurationValue("silentMode") == "false")
            QMessageBox::information(this,tr("Remove duplicate"),msg);
        else
            ui->statusBar->showMessage(msg, 2000);
    }

    // We update the view
    this->scraptable->updateLayout();
}


void MainWindow::updateCustomRatio(int left)
{
    ui->customRatioLabel->setText(QString::number(left) + tr(" customs left"));
}

void MainWindow::on_ExportSEButton_clicked()
{
    int index;
    QString content;
    bool exportCustoms;
    bool fullExport;
    QHash<QString, QVariant> se_config;
    QMap<int, QStringList> QMapContent;

    index = ui->customSeComboBox->currentIndex();
    exportCustoms = false;

    if(index)
    {
        int returnValue = QMessageBox::question(this, tr("RDDZ Scraper"),
                                                tr("Do you want to export custom1 and custom2 values ?"),
                                                QMessageBox::No|QMessageBox::Yes|QMessageBox::Cancel, QMessageBox::No);

        if(returnValue == QMessageBox::Cancel)
            return;

        int returnFullExportValue = QMessageBox::question(this, tr("RDDZ Scraper"),
                                                          tr("Do you want a full export (clicking No will create a minimal export configuration) ?"),
                                                          QMessageBox::No|QMessageBox::Yes|QMessageBox::Cancel, QMessageBox::No);

        if(returnFullExportValue == QMessageBox::Cancel)
            return;

        fullExport      = (returnFullExportValue == QMessageBox::Yes) ? true : false;
        exportCustoms   = (returnValue == QMessageBox::Yes) ? true : false;
        se_config       = config->getScrapEngineConfigurationHash(ui->customSeComboBox->itemData(index, Qt::AccessibleTextRole).toInt(),fullExport);
        content         = config->exportSE(se_config,exportCustoms);
        QMapContent.insert(0,QStringList(content));

        tools.saveFile(QMapContent, QStringList() << ".sql", _delimiters);
    }
    else
    {
        if(config->getConfigurationValue("silentMode") == "false")
            QMessageBox::critical(this,tr("Export fail"),tr("You have to select a Search Engine to export"));
        else
            ui->statusBar->showMessage(tr("Export fail : You have to select a Search Engine to export"), 2000);
    }
}

void MainWindow::on_actionImportSearchEngine_triggered()
{
    QString content(tools.openFile("se"));
    config->importSE(content);
}

void MainWindow::initScrapClass()
{
    bool reInitScraperClasse = true;

    if(this->scraper)
    {
        if (this->network)
            network->disconnectScraper();
        scraper->disconnectScraper();
        delete this->scraper;
    }
    if(this->network)
        delete this->network;

    if(reInitScraperClasse)
        scraper = new Scraper(this,config,scraptable);
    this->network = new Network(config,scraper,this);
}

void MainWindow::disableAllNetworkButtons()
{
    ui->httpButton->setEnabled(false);
    ui->resolveRedirectButton->setEnabled(false);
    ui->checkDofollowNofollow->setEnabled(false);
    ui->toolBacklinksButton->setEnabled(false);
    ui->getOblButton->setEnabled(false);
    ui->getPlatformButton->setEnabled(false);
    ui->getIpAddrButton->setEnabled(false);
    ui->linkAliveButton->setEnabled(false);
    ui->whoisButton->setEnabled(false);
}

int MainWindow::getUrlCol()
{
    return this->scraptable->columnIndexes.value("urlCol");
}

int MainWindow::getMaxCol()
{
    return this->maxCol;
}

void MainWindow::launchPhantomScrap(QHash<QString,QString> args)
{   
    if(args.value("url").isEmpty())
        return;

    if(scraper->isScrapRunning() == false)
        return;

    QString loadImages, renderArgs, encodedUrl, custom1, custom2;
    QProcess *phantomProcess = new QProcess(this);


    QObject::connect(phantomProcess,SIGNAL(finished(int,QProcess::ExitStatus)), scraper, SLOT(phantomRenderFinished(int,QProcess::ExitStatus)));
    QObject::connect(phantomProcess,SIGNAL(error(QProcess::ProcessError)), scraper, SLOT(phantomRenderError(QProcess::ProcessError)));

    QObject::connect(phantomProcess,SIGNAL(readyReadStandardError()),scraper,SLOT(phantomReadyReadStandardError()));
    QObject::connect(phantomProcess,SIGNAL(readyReadStandardOutput()),scraper,SLOT(phantomReadyReadStandardOutput()));



    QString phantomBinaryName;

    QString ua_used,proxy_parameter,proxyStringLog,proxyUsed,referrerUrl,captchaImg;
    QStringList proxies, proxy;
    QUrl myQurl, refererQurl;

    QString cleanUrl;
    QString newUrl;
    QString nonEncodedUrl;

    // We split url if we encounter the get_first function
    if(args.value("url").startsWith("get_first("))
    {
        newUrl = args.value("url");
        //newUrl.replace(QRegularExpression("get_first\\\(([^\\\)]+)\\\)"),"\\1");
        newUrl.replace(QRegularExpression(QRegularExpression::escape("get_first(") + "([^\\)]+)" + QRegularExpression::escape(")")),"\\1");
        cleanUrl = newUrl.section("~~",0,0);

        nonEncodedUrl = cleanUrl;
    }
    else if(args.value("url").contains("get_first("))
    {
        QString firstMotif;
        QString passedUrl;
        QString extractedParams;
        passedUrl = args.value("url");

        QRegularExpression regexGetFirst(QRegularExpression::escape("get_first(") + "([^\\)]+)" + QRegularExpression::escape(")"));
        QRegularExpressionMatch match = regexGetFirst.match(passedUrl);
        if (match.hasMatch()) {
            extractedParams = match.captured(1);
            firstMotif = extractedParams.section("~~",0,0);
        }

        passedUrl.replace(QRegularExpression(QRegularExpression::escape("get_first(") + "([^\\)]+)" + QRegularExpression::escape(")")),firstMotif);
        //        passedUrl.replace(QRegularExpression("get_first\\\(([^\\\)]+)\\\)"),firstMotif);

        nonEncodedUrl = passedUrl;
    }
    else
        nonEncodedUrl = args.value("url");

    myQurl.setUrl(nonEncodedUrl);


    proxies = (config->proxy_enable()) ? config->get_proxies() : QStringList();
//    ua_used = (config->getSpecificUa()!="") ? config->getSpecificUa() : tools.randomUa(config->get_ua(args.value("userAgent")));
    ua_used = args.value("userAgent");

    // We escape double quote into customs
    custom1 = args.value("custom1");
    custom2 = args.value("custom2");
    custom1.replace("\"","\\\"");
    custom2.replace("\"","\\\"");

    encodedUrl = myQurl.toEncoded();
    // Special sheet case for Google
    if(args.value("url").contains(QRegExp("^https?://(www|m|maps).google")))
    {
        if (!args.value("url").contains(QRegExp("/sorry/")))
        {
            QString googleUnencodedQuery, googleEncodedQuery;
            QUrlQuery googleQurlQuery(myQurl);
            googleUnencodedQuery = googleEncodedQuery = googleQurlQuery.queryItemValue("q", QUrl::FullyEncoded);
            googleEncodedQuery.replace("+","%2B");
            googleEncodedQuery.replace("%20","+");
            encodedUrl.replace(googleUnencodedQuery,googleEncodedQuery);
        }
    }

    // Args initialisation
    if(args.contains("referrerUrl") && !args.value("referrerUrl").isEmpty())
        refererQurl.setUrl(args.value("referrerUrl"));

    referrerUrl = (args.contains("referrerUrl") && !args.value("referrerUrl").isEmpty()) ? " \"referrerUrl="+refererQurl.toEncoded()+"\" " : "";
    captchaImg  = (args.contains("captchaImg") && !args.value("captchaImg").isEmpty()) ?  " \"captchaImg="+args.value("captchaImg")+"\" " : "";

#ifdef Q_OS_WIN
    phantomBinaryName = "phantomjs.exe";
#else
    phantomBinaryName = "phantomjs";
#endif

    // Check if phantom is present
    if(!QFile::exists(QApplication::applicationDirPath()+"/dist/"+phantomBinaryName))
    {
        QMessageBox::warning(this, tr("PhantomJs not found"), tr("PhantomJs binary not found. Unable to scrap."));
        enableButtons();
        return;
    }

    if(proxies.isEmpty())
    {
        proxy_parameter = " --proxy-type=none ";
        //        proxy_parameter = " ";
        proxyStringLog  = "No proxy";
    }
    else
    {
        if(config->useProxyRotating())
        {
            proxy = tools.proxyInfos(proxies.at(0));
        }
        else
        {
            // We have to verify if proxy is not blocked for this URL
            if(!args.contains("proxy") || args.value("proxy").isEmpty())
            {
                proxy = tools.proxyInfos(tools.switchProxy(proxies,args.value("url")));
                if(proxy.size())
                {
                    if(proxy.at(0).isEmpty())
                    {
                        emit appendLog(tr("Unable to use proxies ... [Aborting]"));
                        this->scraper->sendAbortNetwork();
                        return;
                    }
                }
            }
            else
            {
                proxy = tools.proxyInfos(args.value("proxy"));
                proxy = tools.proxyInfos(args.value("proxy"));
            }
        }


        if(proxy.at(0).isEmpty())
        {
            proxy_parameter = " --proxy-type=none ";
            proxyStringLog  = "No proxy";
        }
        else if(!proxy.at(0).isEmpty())
        {
            proxy_parameter = " --proxy-type=http --proxy="+proxy.at(0)+":"+proxy.at(1);
            proxyStringLog  = proxyUsed = proxy.at(0)+":"+proxy.at(1);
        }

        if(proxy.count() > 2)
        {
            if(!proxy.at(0).isEmpty() && !proxy.at(2).isEmpty())
            {
                proxy_parameter.append(" --proxy-auth="+proxy.at(2)+":"+proxy.at(3));
                proxyUsed += proxy.at(2)+":"+proxy.at(3);

            }
        }
    }

    QString cookieFile;
    if(args.value("url").contains(QRegExp("^https?://(www|m|maps|ipv4|ipv6).google")))
    {
        loadImages = cookieFile = "";
        renderArgs = " \"render=true\"";
        if(config->useProxyRotating() == false)
        {
            if(proxies.isEmpty())
                cookieFile = " \"cookieFilename="+QApplication::applicationDirPath()+"/tmp/localhost.rddz\" ";
            //        cookieFile = " \"cookieFilename="+QDir::tempPath()+"/localhost.rddz\" ";
            else
                cookieFile = " \"cookieFilename="+QApplication::applicationDirPath()+"/tmp/"+proxy.join("").replace(".","")+".rddz\" ";
        }
    }
    else
    {
        loadImages = " --load-images=false ";
        cookieFile = renderArgs = "";
    }
    QString pageLoadTimeout = " \"pageLoadTimeout="+QString::number(config->getConfigurationValue("pageloadingTimeout").toInt() * 1000)+"\" ";


    appendLog("[Proxy : "+proxyStringLog+"][User-Agent : "+ua_used+"][JS Enable] "+ encodedUrl);

    if(tools.proxiesBanned.isEmpty() || (!tools.proxiesBanned.isEmpty() && tools.proxiesBanned.contains(proxyUsed) == false) || (args.size() && args[0] == "true"))
    {
        QString phantomRunCmd;

        QString hashedUrl = QCryptographicHash::hash(args.value("url").toUtf8(),QCryptographicHash::Md5).toHex()+"rddz";

        phantomRunCmd = "\""+QDir::toNativeSeparators(QApplication::applicationDirPath()+"/dist/")+phantomBinaryName+"\""+proxy_parameter+loadImages+" --ignore-ssl-errors=true --web-security=false --config=\""+QDir::toNativeSeparators(QApplication::applicationDirPath()+"/dist/")+"config.json\" "+"\""+QDir::toNativeSeparators(QApplication::applicationDirPath()+"/dist/")+"scraper.js\"  "+"\""+encodedUrl+"\" \"userAgent="+ua_used+"\""+cookieFile+pageLoadTimeout+"\"url="+encodedUrl+"\" \"custom1="+custom1+"\" \"custom2="+custom2+"\""+renderArgs+referrerUrl+captchaImg+" \"clipRect="+QApplication::applicationDirPath()+"/tmp/"+hashedUrl+".png\"";


        phantomProcess->setProcessChannelMode(QProcess::SeparateChannels);
        phantomProcess->start(phantomRunCmd);
    }
}


void MainWindow::clearAutoCheckedBacklinksAction()
{
    ui->actionAutorunBacklinksDomain->setChecked(false);
    ui->actionAutorunBacklinksSubdomain->setChecked(false);
    ui->actionAutorunBacklinksURL->setChecked(false);
}

void MainWindow::updateDelimiters()
{
    _delimiters.insert("txtDelimiter",this->txtDelimiter);
    _delimiters.insert("csvSeparator",this->_csvSeparator);
    _delimiters.insert("csvDelimiter",this->_csvDelimiter);

}
