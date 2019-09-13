#ifndef MAINWINDOW_H
#define MAINWINDOW_H
#include "config.h"
#include "scraper.h"
//#include "backlinks.h"
#include "tools.h"
#include "appabout.h"
#include "footprintmgr.h"
#include "network.h"
#include "scraptable.h"
#include "deletebox.h"
#include "searchreplacebox.h"
#include "scrapenginemanager.h"
#include "comboboxdelegate.h"

#include <QMainWindow>
#include <QWidget>
#include <QStandardItemModel>
#include <QDesktopServices>
#include <QUrl>
#include <QChar>
#include <QProcess>
#include <QLabel>
#include <QSystemTrayIcon>
#include <QScrollBar>

#include <QApplication>
#include <QTranslator>
#include <QLocale>
#include <QLibraryInfo>

#include <QtConcurrent/QtConcurrent>
#include <QDateTime>
#include <QDir>


//#include <QtWebEngineWidgets>

//#define REQUEST_PAGERANK    0
#define REQUEST_STATUS_CODE 1
#define REQUEST_DOFOLLOW    2
#define REQUEST_REDIRECTION 3
#define REQUEST_BACKLINKS   4
#define REQUEST_OBL         5
#define REQUEST_IP_ADDR     6
#define REQUEST_LINK_ALIVE  7
#define REQUEST_PLATFORM    8
#define REQUEST_WHOIS       9


namespace Ui {
class MainWindow;
}


class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    MainWindow(QWidget *parent = 0);
    ~MainWindow();
    bool eventFilter(QObject *obj, QEvent *event);

//    int                 getNbUrl();
    int                 getNbUrl(int col=0);
    QStringList         getRedirUrls();
    QStringList         getUrls();
    Network * getNetwork();

    QProcess            process;

    int                 getUrlCol();
    int                 getMaxCol();

    int                 getColumnIndexByName(QString columnName);
    QSystemTrayIcon     sysTrayIcon;
    QString             getCurrentAction(); // Backlink or scrap
    QChar               getCsvSeparator();
    QChar               getCsvEnclosedBy();

    bool                errorDuringUpdate;



protected:
    void                changeEvent(QEvent *);

signals:
    void               setBusyCursor();
    void               restoreCursor();
    void               noDuplicateUrl(bool, QString msg="", QString title="");
    void               deleteItemsFromTable(int itemsRemoved,bool enableButtons,bool silentMode);
    void               removeSelected(QList<QTreeWidgetItem *>);

    void               enableButtonsSignal();
    void               disableButtonsSignal();
    void               updateNbUrlSignal();

    void               restoreSorting();
    void               displayMsg(QString);


private slots:
    void                on_ExportSEButton_clicked();
    void                on_actionImportSearchEngine_triggered();
    // Top menu
    void                on_about_triggered();
    void                on_aboutQT_triggered();
    void                on_actionFootprint_Manager_triggered();
    void                on_actionXPath_Expression_tester_triggered();

    void                launchSEManager();

    void                on_actionAuto_remove_bad_urls_after_http_status_check_triggered(bool checked);
    void                on_actionAuto_resolve_redirections_triggered(bool checked);
//    void                on_actionAutorun_Pagerank_after_scrap_triggered(bool checked);
    void                on_actionAutorun_HTTP_Status_check_after_scrap_triggered(bool checked);
    void                on_actionAutorun_check_dofollow_triggered(bool checked);
//    void                on_actionAutorun_backlinks_triggered(bool checked);
    void                on_actionAutorun_outbound_links_triggered(bool checked);
    void                on_actionAutorun_retrieve_IP_triggered(bool checked);


    void                on_actionAutorunBacklinksDomain_triggered(bool checked);
    void                on_actionAutorunBacklinksSubdomain_triggered(bool checked);
    void                on_actionAutorunBacklinksURL_triggered(bool checked);


    // Top scrap functions
    void                on_footprintComboBox_activated(QString fp);
    void                on_SendFootprint_clicked();
    void                on_backlinksButton_clicked();
    void                on_abortButton_clicked();

    // Right Scrap List buttons
    // Tools
    void                on_trimToRoot_clicked();
    void                on_trimToLastFolderButton_clicked();
    void                on_keepOnlyDomainButton_clicked();
    void                on_resolveRedirectButton_clicked();
    void                on_httpButton_clicked();
    void                on_checkDofollowNofollow_clicked();
    void                getToolBacklinksButton();
    void                on_getOblButton_clicked();
    void                on_getPlatformButton_clicked();
    void                on_getIpAddrButton_clicked();
    void                on_linkAliveButton_clicked();
    void                on_whoisButton_clicked();
//    void                on_transfertToCustom1Button_clicked();
    void                transferToCustom1();
    void                transferNonExistingToCustom1();
    void                getToolBacklinksDomain();
    void                getToolBacklinksSubomain();
    void                getToolBacklinksUrl();




    // Remove
    void                on_openSearchReplaceBox_clicked();
    void                on_openDeleteBoxButton_clicked();
    void                on_removeBadUrlButton_clicked();
    void                removeDuplicateUrlAction();
    void                removeDuplicateDomainsAction();
    void                on_removeSelectedUrlButton_clicked();
    void                on_removeSubdomainButton_clicked();
    void                on_removeRegexMaskButton_clicked();

    //  Import / Export
    void                importScrapButtonEmulate();
    void                fullScrapExportButtonEmulate();
    void                stdScrapExportButtonEmulate();

    // Le clear
    void                on_ClearUrlList_clicked();


    // Proxies tab
    void                on_scrapWithoutProxyWarnCheckBox_clicked(bool activated);
    void                on_useProxiesCheckBox_clicked(bool activated);
    void                on_testProxiesOnGoogleCheckBox_clicked(bool activated);
    void                on_useProxyRotatingCheckBox_clicked(bool activated);
    void                on_proxiesAddButton_clicked();
    void                on_SaveProxies_clicked();
    void                on_proxiesImportButton_clicked();
    void                on_proxiesTestButton_clicked();
    void                on_proxiesDeleteSelectedButton_clicked();
    void                on_proxiesDeleteButton_clicked();
    void                on_proxiesClearAllButton_clicked();


    // Scrap Config Tab
    void                on_saveConfig_clicked();
    void                on_SESelect_activated(QString);
//    void                on_SESelect_currentIndexChanged(int);

    // Custom Search Engine tab
    void                on_customSeComboBox_currentIndexChanged(int);
    void                on_customSeNewSeButton_clicked();
    void                on_customSeSaveButton_clicked();
    void                on_customSeUpdateButton_clicked();
    void                on_customDeleteButton_clicked();
    void                switchLanguage(QAction *action);

    // Configuration Tab
    void                on_configurationSaveButton_clicked();
    void                on_ahrefsBalanceButton_clicked();
    void                on_majesticBalanceButton_clicked();
//    void                on_dbcBalanceButton_clicked();
    void                on_whoisApiComboBox_currentIndexChanged(int);


    // Advanced Tab (Extend of configuration)
    void                on_saveAdvancedConfiguration_clicked();

    // Columns Tab
    void                on_saveColumnButton_clicked();

    // Logs Tab
    void                on_clearLogsButton_clicked();


    void                openUrlWidget(QModelIndex modelIndex);

    void                noDuplicateUrlSlot(bool silentMode=true, QString msg=tr("No duplicate URL found"), QString title=tr("Remove duplicate url"));
    void                deleteItemsFromTableSlot(int itemsRemoved, bool enableButtons=false, bool silentMode=true);

    void                keepOnlyDomains();
    void                updateLastOperationTime();

    void                restoreSortingSlot();
    void                disableButtons();
    void                enableButtons();

    void                displayMsgSlot(QString msg);
    void                emulateSelectAll();


//    void webPageLoaded(bool status);

public slots :
    void                addFields(QMap<int, QList<QVariant> > map, int resultCount);
    void                trimToRoot();
    void                trimToLastFolder();
    void                removeBadUrl(bool enableButtons=false, bool silentMode=true);
    void                removeDuplicateUrlSlot(bool enableButtons=false, bool silentMode=true, bool clickedButton=false);
    void                removeDuplicateDomains();
    void                scrapButtonEnabled();
    void                scrapButtonDisabled();
    void                updateCustom1(QString);
    void                updateCustom2(QString);
    void                updateCustomSeCombo(QMap<QString, QPair<QString, int> > SEMap, QString match);
    void                updateHttpStatus(QString url, int httpstatus);
    void                UpdateRelUrls(QString, double);
    void                updateBacklinks(QString url, qlonglong bls);
    void                updateObl(QString url, int obl);
    void                updateIpAddr(QString url, QString addr);
    void                updateLinkAlive(QString url, QString status);
    void                updatePlatform(QString url, QString platform);
    void                updateDomainAvailable(QString url, QVariant value, int columnIndex);
    void                updateCitationFlow(QString url, int cf);
    void                updateTrustFlow(QString url, int tf);
    void                updateTableViewSlot(QString url, QVariant value, int col);
    void                updateTableMappingViewSlot(QPair<QString, QString> firstPair, QPair<QString, QString> secondPair, QVariant value, int col);
    void                updateMultipleCol(QString url, QHash<int, QVariant> values, bool basedOnUrl);
    void                updateMultipleRows(QHash<QString, QHash<int, QVariant> > values, bool basedOnUrl);
   // void                updateCustomCol(QString url, QString value);
    void                updateNbUrl(int nbUrls=-1);
    void                updateCustomRatio(int left);
    void                updateUrl(QString oldUrl, QString newUrl);
    void                insertProxies(QStringList proxies);
    void                markDeadProxy(QString proxy, int error);
    void                markGoodProxy(QString proxy);
    void                timeoutProxies();
    //    void                proxyChanged(QModelIndex,QModelIndex);
    void                populateFootprints(QStringList);
    void                clearFootprints();
    void                appendLog(QString, QString logStyle="default");
    void                autoResolveRedirect();
    void                autoHttpStatus();
    void                autoCheckDofollowNofollow();
    void                autoBacklinks();
    void                autoObl();
    void                autoIp();

    void                setBusyCursorSlot();
    void                restoreCursorSlot();


    void                doFullCopy();
    void                doUrlCopy();
    void                doUrlPrCopy();
    void                doColumnCopy();
    void                doPaste();

//    void                updateAhrefsUnits(QString units);
    void                updateBacklinksBalanceUnits(QString units, QString service);

    void                updateProgressBar(int current, int total, QString label="");
    void                showSystrayMsgSlot(QString msg);

//    void                enableAhrefCreditButton();
//    void                enableMajesticCreditButton();
//    void                enableDecaptcherCreditButton();
//    void                enableDBCCreditButton();

//    void                scrapEndActions();

    void                customContextMenuRequestedSlot(QPoint pos);


//    void                launchPhantomScrap(QString url, QString custom1, QString custom2, QString ua, QString proxy="", QStringList args=QStringList());
    void                launchPhantomScrap(QHash<QString,QString> args);



private:
    Ui::MainWindow      *ui;
    Scraper             *scraper;
    Tools               tools;
    Config              *config;
    Network             *network;
    FootprintMgr        *footprintMgr;
    ScrapEngineManager  *seMgr;
    scrapTable          *scraptable;
//    Backlinks           *backlinks;
    searchReplaceBox    searchreplacebox;
    deleteBox           deletebox;
    AppAbout            aboutWin;
    bool                errorfound;
    bool                use_proxies;
    bool                _customNoDedup;
    QString             proxy_used;
    QString             footprint;
    QString             saveMsg;
    QString             currentVersionTxt;
    QString             lineBreak;
    QString             blApiForScrap; // use api for bls metrics : scrap.
    QString             action; // Scrap or backlinks
    QString             commentHeaderFile;
    QString             previousAction;
    QTranslator         appTranslator;
    QStringList         scrapExtensions;
    QStandardItemModel  *model;
    QBrush              okBrush;
    QBrush              errorBrush;
    QBrush              redirBrush;

    QChar               txtDelimiter;
    QChar               csvDelimiter;
    QChar               _csvDelimiter;
    QChar               _csvSeparator;
    QChar               csvEnclosedBy;

    QHash<QString,QChar> _delimiters;
    QHash<int,QString>  urlHash;

    QTime              proxyTimeStart;
    QTime              lastOperationTimeStart;
    QTimer             *lastOperationTimer;
    QTimer             *proxyTimer;
    QTimer             *updateViewTimer;
    QLabel             *elapsedTimeLabel;

    QStringList         abbrProxyCountries;
    QStringList         proxyCountries;

    quint8             urlCol;
    quint8             anchorCol;
    quint8             statusCol;
    quint8             dofollowCol;
    quint8             backlinksCol;
    quint8             mozRankCol;
    quint8             daCol;
    quint8             paCol;
    quint8             cfCol;
    quint8             tfCol;
    quint8             ahrefsUrlRank;
    quint8             oblCol;
    quint8             ipCol;


    quint8             maxCol;


    QMap<int, QStringList> GetScrapListResult();
    void                updateScrapList(QByteArray,QString type="urlsonly");
    void                clearCustomSeFields();
    void                updateMenuOptions();
    void                buildMenuLanguage();
    void                buildToolButton(QString iconPath, QString title, QString tooltip, QMenu *menu, QString objectName,  bool isEnabled=true, bool showMenu=true);
    void                buildToolbar();

    void                insertColumn(QString columnName, QString columnTooltip, QString stdName, int width=60, int startAt=-1);
    void                buildScrapTabResults(QString action, QString backlinkAPI="", QStringList scrapFileHeader = QStringList());
    void                clearAutoCheckedBacklinksAction();
    void                initProxiesList();
    //QStringList         getProxiesFromList();
    QHash<int,QStringList>  getProxiesFromList();
    void                updateProxiesList(QStringList proxies,QString action="add",QBrush brush = QBrush(), QString errMsg= QString());

    void                loadConfiguration();
    void                loadNewsTabContent();
    void                needNewInstall();

    QStringList         getSeCustomColumns();
    QString             genCsvLine(QStringList input);
    QStringList         getSeCustomColumnsName(QString columns);
    void                populateCustomSeColumn(QStringList columnNames);

    void                addComboParentItem(QStandardItemModel * model, const QString& text);
    void                addComboChildItem(QStandardItemModel * model, const QString& text, const QVariant& data);
    void                addComboSeparator(QStandardItemModel * model);

    void                doCopy(QList<int> columns, bool selectedOnly=true);

    void                requestLauncher(int requestIndex, int timerType);
    void                initScrapClass();
    void                disableAllNetworkButtons();

    int                 getBacklinksMethod; // 0 = URL, 1 = Subdomain, 2 = Domain
    int                 getColumnCount();

    bool                _getDomainOnly; // For updateAllDomain scraptable method
    bool                _configLoaded;

    void                updateDelimiters();
//    int                 _resendReqHTTP;


//    QWebEnginePage     *_webPage;
    //void *webPageCallBack(QString sourcecode);
    QHash<QString, QVariant> buildSEConfigHash();
    int                 idealThreadMultiplicator();
};

#endif // MAINWINDOW_H
