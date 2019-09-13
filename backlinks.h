#ifndef BACKLINKS_H
#define BACKLINKS_H
#include "config.h"
#include "tools.h"
#include <QtNetwork>
#include <QWidget>
#include <QUrlQuery>
#include <unistd.h>

#ifdef Q_OS_WIN
#include <windows.h>
#endif


class MainWindow;
class Scraper;

class Backlinks : public QWidget
{
    Q_OBJECT
public:
    Backlinks(MainWindow *mw, Config *conf, Scraper *parentScraper);
    ~Backlinks();
    void            getAhrefsBalance(QString apiKey);
    void            getMajesticBalance(QString accessToken, QString service);
    void            scrapBacklinks(QString url, QString backlinkService, bool firstPass=false);
    void            processBacklinksUrl(QString backlinkService);
    QHash<QString,QString> hashAhrefsBlsMode;
    void            getAhrefsBacklinksCountForUrl(QList<QVariant> results);

    QStringList     urlToCheck;
        int             blsCurrentThread;


signals:
    void            createHttpRequest(QString url, QString ua, int type, QStringList args = QStringList(), QUrl qurl=QUrl());
//    void            addFields(QMap<int, QStringList>,int resultCount=1);
    void            addFields(QMap<int, QList<QVariant> >,int resultCount=1);
    void            enableAhrefsButton();
    void            enableMajesticButton();
    void            updateBacklinks(QString, qlonglong);
    void            deleteReplyFromQhash(QString);


public slots:
    void            getAhrefsBalanceFinished();
    void            getAhrefsBacklinksCount();
    void            getAhrefsBacklinks();
    void            getAhrefBacklinksNbPerItem();
    void            getMajesticBacklinks();
    void            getMozBacklinks();
    void            getMajesticBalanceFinished();

private:
    Config          *config;
    MainWindow      *mainWin;
    Scraper         *scraper;
    QString         ahrefsBacklinksMode;
    int             blsCount;
    int             blsTotal;
    int             blsThreads;
    QHash<QString,QString> _apisUrls; // Key : service name, Value : url

    int             maxBlsPerRequest;
    int             blStartOffset;
    int             ahrefsBlCounter;
    int             majesticStartAt;
    int             majesticMaxItemPerQuery;
    int             urlsLeft;
//    int             blsTreated;
    int             blGlobalIndex ; // Use it for Majestic and ahrefs
    bool            ahrefsMotherFucker;
    QMutex          backlinksMutex;
    void            scrapBacklinksProcessResults(QByteArray source, QString blService, QString url);
    void            blError(QString err);

    void            getMajesticBacklinksProcessResult(QByteArray str, QString apiCallUrl, QString currentUrl, int statusCode, int backlinkIndex);
    void            getAhrefsBacklinksProcessResult(QString str, QString url, int statusCode);

    QString         ahrefsSelectParam;
    QList<QVariant> backlinksUrlList;
    QHash<int, QHash<int,QString> > majesticUrlIndex;
    //QHash<QString, int> mappingUrlBls;
    QHash<QString, int> mappingBlCount; // Map the backlinks left for this url
    QHash<QString, QList<QVariant> > mappingUrlBls; // Map The domain/url/subdomain with their backlinks urls
    QHash<QString, int> mappingStartAt; // Map the current StartAt with the url
    QHash<QString, int> mappingMajesticStartAt; // Map the current MajesticStartAt with the url
    QHash<QString, int> mappingBacklinkIndexForUrl; // Map the current backlink index with the url

    QHash<QString, QHash<int,QString> > fullMappingUrl;

    QPair<QString, QByteArray> genSeobserverAuthFields();
};

#endif // BACKLINKS_H
