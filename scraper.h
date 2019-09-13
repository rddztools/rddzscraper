#ifndef SCRAPER_H
#define SCRAPER_H
#include "config.h"
#include "tools.h"
#include "backlinks.h"
#include "scraptable.h"
//#include "controller.h"
#include "worker.h"
#include <QtNetwork>
#include <QWidget>
#include <stdio.h>
#include <errno.h>
#include <QDateTime>
#include <QDir>
#include <QTextStream>
#include <QUrlQuery>
#include <QMutex>
#include <QMutexLocker>
#include <QSystemTrayIcon>
#include <QNetworkReply>

#define TIMER_HTTPSTATUS    0
//#define TIMER_PAGERANK      1
#define TIMER_DOFOLLOW      2
#define TIMER_SCRAPER       3
#define TIMER_BACKLINKS     4
#define TIMER_OBL           5
#define TIMER_IP            6
#define TIMER_LINKALIVE     7
#define TIMER_PLATFORM      8
#define TIMER_WHOIS         9

#ifndef NETWORK_H
#define NETWORK_SCRAP       0
//#define NETWORK_BL          1
#define NETWORK_HTTPSTATUS  2
#define NETWORK_REDIR       3
//#define NETWORK_PR          4
#define NETWORK_DOFOLLOW    5
#define NETWORK_PROXY       6
#define NETWORK_URL_BL      9
#define NETWORK_OBL         10
#define NETWORK_LINK_ALIVE  13
#define NETWORK_PLATFORM    14
#define NETWORK_WHOIS       15

#define AHREFS_BALANCE      20
#define AHREFS_BL_COUNT     21
#define AHREFS_BACKLINKS    22

#define MAJESTIC_BALANCE    30
#define MAJESTIC_BACKLINKS  31

#define MOZ_BACKLINKS       40
#endif


class MainWindow;

class Scraper : public QWidget
{
    Q_OBJECT
public:
    Scraper(MainWindow *mw,Config *conf,scrapTable *scraptable);
    ~Scraper();
    void            disconnectReply(QNetworkReply *r, int type);
    void            setFootprint(QString);
    void            setCustomLoop(QString loop1="",QString loop2="");
    void            setSeSelected(QHash<QString, QVariant> scrapEngineConfig);
    void            setWhoisApiService(QString apiService);
    void            setWhoisApiKey(QString apiKey);
    void            clearFootprint();
    void            sendAbortNetwork();
    void            launchScrap();
    void            scrapBacklinks(QString url, QString backlinkService);
    void            checkDofollowNofollow(QString url);
    void            getBacklinks(QString url);
    void            getBulkBacklinks(QStringList urls);
    void            getObl(QString url);
    void            getUrlRespCode(QString);
    void            getIpAddr(QString);
    void            getLinkAlive(QString);
    void            getLinkAliveMapper(QString url, QString blFor);
    void            getPlatform(QString);
    void            getSingleWhois(QString domain);
    void            getMultipleWhois(QUrlQuery apiQurlQuery);
    void            resolveRedirections(QString url);
    void            setGetBacklinksMethod(int method);
    void            clearCustomList();
    void            sendRequest(const QString &url, QHash<QString, QVariant> params);
    int             getScrapUrlsSize();
    int             countScraper;
    int             countHeader;
    int             countRel;
    int             countBls;
    int             countObl;
    int             countIp;
    int             countLinkAlive;
    int             countPlatform;
    int             countWhois;
    int             interval;
    int             startAt;
    int             mwNbUrls;
    double          resultsFound;
    QFile           autoSaveScrapResultsFile;
    QTextStream     scrapDataStream;
    QStringList     urlList;
    Backlinks       *backlinks;
    Config          *config;
    QHash<int,QStringList> urlMapping; // Map Url with another field
    //int             getScrapUrlSize();
    void            setCustomLoopList(QStringList loop1=QStringList(), QStringList loop2=QStringList());
    void            startIpTimer();
    void            getAhrefsBalance(QString apiKey);
    QStringList     getXpath(QString str, QString queryXpath, QString custom1="", QString custom2="", QString xpathUrl="", bool displayLog=false);
    void            scrapProcessResults(QHash<QString, QString> args, QNetworkAccessManager *qnam = NULL);
    bool            isScrapRunning();
    bool            jsEnable;
    bool            htmlEncoded;
    bool            forceDeleteStack;
    bool            customScrapColumn;

    void disconnectScraper();

    void            getUrlRespCodeProcessResults(int statusCode, QString url);
    void            checkDofollowNofollowProcessResults(double rel, QString url);
    void            getOblProcessResults(double obl, QString url);
    void            resolveRedirectionsProcessResults(int statusCode, QString url, QString redirectedUrl="");
    void            getBacklinksProcessResults(QHash<QString, QVariant> results, QString url);
    void            getLinkAliveProcessResults(QString status, QString url);
    void            getPlatformProcessResults(QString platformString, QString url);
    void            getWhoisProcessResults(QString isAvailable, QString url);
    void            initScrapVars();


    QStringList results;
    int urlsChecked;
    int maxThreads;
    int nextUrlIndex;
    int threadTimeout;
    bool keepStackFiles;

    QString         currentAction;

private slots:
    void            checkDofollowNofollowFinished();
    void            getBacklinksFinished();
    void            getOblFinished();
    void            getScrapFinished();
    void            getUrlRespCodeFinished();
    void            resolveRedirectionsFinished();
    void            getLinkAliveFinished();
    void            getPlatformFinished();
    void            getWhoisFinished();
    void            ipAddrTimeout();
    void            stopIpTimer();

    void            launchRequestSlot(int type);



public slots:
    void            requestLauncher(int type);
    void            sendAppendLog(QString msg,QString logStyle="default");
    void            phantomRenderFinished(int exitCode,QProcess::ExitStatus);
    void            phantomRenderError(QProcess::ProcessError error);
    void            phantomReadyReadStandardOutput();
    void            phantomReadyReadStandardError();
    void            donwloadProgress(qint64 bytesReceived, qint64 bytesTotal);

    void            interpretScrapResults(QHash<QString, QVariant> results, QHash<QString, QVariant> replyProperties);
    void            threadTimeoutSlot(const QString &url, QHash<QString, QVariant> replyProperties);
    void            retryRequestSlot(const QString &url, QHash<QString, QVariant> replyProperties);
    void            updateNbOfThreadsLaunched();
    void            updatePairProxyCookies(const QString &proxy, const QVariant &cookie);
    void            updateBannedProxies(QString proxy, bool banned);
    void            updateTableResults(QStringList list, QHash<QString, QVariant> replyProperties, QString url);
    void            updateItemsFound(int itemsFound);


signals:
    void            updateUrl(QString, QString);
    void            updateHttpStatus(QString, int);
    void            UpdateRelUrls(QString, double);
    void            updateBacklinks(QString, qlonglong);
    void            updateObl(QString, int);
    void            updateIpAddr(QString, QString);
    void            updateLinkAlive(QString,QString);
    void            updateDomainAvailable(QString,QVariant,int);
    void            updatePlatform(QString,QString);
    void            updateCitationFlow(QString, int);
    void            updateTrustFlow(QString, int);
    void            updateTableViewSignal(QString,QVariant,int);
    void            updateTableViewMappingSignal(QPair<QString,QString>, QPair<QString,QString>, QVariant, int);
    void            updateMultipleCol(QString url, QHash<int, QVariant> values,bool);
    void            updateMultipleRows(QHash<QString, QHash<int, QVariant> > values,bool);
    void            updateCustomRatio(int);
    void            updateNbItems(int nbUrls=-1);
    void            insertProxies(QStringList);
    void            removeDuplicateUrl();
    void            removeBadUrl();
    void            scrapButtonEnabled();
    void            abortNetwork();
    void            createHttpRequest(QString url, QString ua, int type, QStringList args = QStringList(), QUrl qurl=QUrl(), QNetworkAccessManager *qnam = NULL);
    void            appendLog(QString, QString logStyle="default");
    void            autoResolveRedirect();
    void            autoHttpStatus();
    void            autoCheckDofollowNofollow();
    void            autoBacklinks();
    void            autoObl();
    void            autoIp();
    QNetworkReply *changeProxy(QNetworkReply *reply);
    void            updateBacklinksBalanceUnits(QString,QString);
    void            runPhantomScrap(QHash<QString,QString> args);

    void            updateScrapTableLayout();
    void            updateTaskProgressBar(int current, int total, QString label="");
    void            showSystrayMsg(QString msg);

    void            launchRequest(int type);
    void            deleteReplyFromQhash(QString);


private :
    //Controller *controller;
    Worker *worker;
    QThread *thread;


    //bool _abort;
    bool _working;


    QString         buildXPathExpression(QString xpathQuery, QString custom1="", QString custom2="");

    void            scrapBacklinksProcessResults(QString str, QString url);
    void            getIpAddrProcessResults(QString url);
    void            doAutoTasks(int type, bool silent=false);
    QStringList     getNextScrapUrl();
    void            scrapFinished(QStringList list, QString ua, QNetworkAccessManager *qnam=NULL);
    double          getExternalLinksCount(QString src, QString url, bool nofollowOnly=false);
    QString         replaceCustom(QString custom, int encoded);
    qint64         scrapedBytes;



    MainWindow      *mainWin;
    Tools           *tools;
    scrapTable      *scraptable;

    QString         footprint;
    QStringList     custom1List;
    QStringList     custom2List;
    int             customIndex;
    QHash<QString, QVariant> seSelected;
    QTimer          *ipAddrTimer;
    int             ipAddrTimerCount;
    QList<QStringList>     scrapurls; // The full url stack
    int             _scrapUrlIndex; // The current index for scrapurls
    QList<QStringList>     _subScrapUrls; // The substack of n urls
    int             _subScrapNb; // How many url we put in the substack
    int             _subScrapIndex; // Current index of the substack list
    int             scrapurlsSize;
    int             initScrapUrlSize;
    QString         proxyUsed;
    bool            _scrapRunning;
    bool            scrapUrlHasCustoms;
    QMutex          scraperMutex;
    int             getBacklinksMethod;

    int             _threadsTimedOut;
    int             _threadsAlreadyLaunched;
    int             _parallelThreads;

    int             _totalItemsFound;

    // Scrap config, in order to not call config class for each request
    QByteArray      _uaUsed;
    QStringList     _uaList;
    QStringList     _proxies;
    QString         _proxyType; // RDDZ, default or none
    int             _scrapPause;
    bool            _scrapPausePagination;
    bool            _retryOnTimeout;
    QString         _whoisApiService;
    QString         _whoisApiKey;
    QString         _tldNotSupportedMsg;
    QString         _blApiForScrap;
    QString         _seJsonStructure; // Json Pre treatment
    QString         _tmpStackDirectory;
    QString         _resendReqStr;
    int             _resendReqStrNotContains;
    int             _resendReqStrAction;
    int             _resendHTTP;
    int             _resendHTTPCondition;
    int             _resendHTTPCode;
    int             _resendHTTPAction;
    QString         _httpLogFilename; // If resend httpaction is log
    bool            _useProxyProviderRotation; // Genre proxyrack ou trusted, une ip qui switch toute seule.
    bool            _minimalLogs;
    bool            _googleUrlOnCustom; // To determine if we have google url into customs
    int             _dynadotResponsesOk;
    int             _xpathInterpreterIndex;
    QString         _forceOutputEncoding;
    bool            _abortCliked;

    QHash<QString,QVariant> _cookiesForProxy;

    QHash<QString, QHash<int, QVariant> > _resultsStack;

    int testingCounter;
};


#endif // SCRAPER_H
