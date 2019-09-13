#ifndef NETWORK_H
#define NETWORK_H
#include "tools.h"
#include "config.h"
#include "scraper.h"
//#include "captcha.h"
//#include "backlinks.h"
#include "qreplytimeout.h"
#include <QtNetwork>
#include <QWidget>

#include <QNetworkCookieJar>

#define NETWORK_SCRAP       0
//#define NETWORK_BL          1
#define NETWORK_HTTPSTATUS  2
#define NETWORK_REDIR       3
#define NETWORK_PR          4
#define NETWORK_DOFOLLOW    5
#define NETWORK_PROXY       6
//#define NETWORK_MCAPTCHA    7
//#define NETWORK_ACAPTCHA    8
#define NETWORK_URL_BL      9
#define NETWORK_OBL         10
//#define NETWORK_GETCAPTCHAIMG_NOJS 11
//#define NETWORK_RESOLVECAPTCHA_NOJS 12
#define NETWORK_LINK_ALIVE  13
#define NETWORK_PLATFORM    14
#define NETWORK_WHOIS       15

#define AHREFS_BALANCE      20
#define AHREFS_BL_COUNT     21
#define AHREFS_BACKLINKS    22
#define AHREFS_BL_NB_FOR_ITEM 23


#define MAJESTIC_BALANCE    30
#define MAJESTIC_BACKLINKS  31

#define MOZ_BACKLINKS       40

class Network : public QWidget
{
    Q_OBJECT
public:
    Network(Config *globalConfig, Scraper *globalScraper, MainWindow *globalMw);
    QNetworkAccessManager *getCurrentManager();
    void disconnectScraper();
     int aborted;


public slots:
    void abortNetwork();
    void createHttpRequest(QString url, QString ua, int type, QStringList args, QUrl qurl=QUrl(), QNetworkAccessManager *qnam = NULL);
    void deleteReplyFromQhashSlot(QString url);

private slots:
    void networkErrorMsg(QNetworkReply::NetworkError err);
    QNetworkReply * changeProxy(QNetworkReply *r_cur);
    void networkProxyOk();
    //    void isAborted(qint64 i = 0,qint64 j = 0);
    void replyDestroyed();

signals:
    void markDeadProxy(QString,int);
    void markGoodProxy(QString);
    void appendLog(QString);
        void            scrapButtonEnabled();

private:
    Config *config;
    Scraper *scraper;
    MainWindow *mw;
    Tools tools;
    int proxyError;

    int countRequest;
    QNetworkAccessManager *networkAccessManager;
    QHash<QString, QNetworkReply *> replies;
};

#endif // NETWORK_H
