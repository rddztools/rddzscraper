#ifndef WORKER_H
#define WORKER_H

#include <QObject>


#include <QObject>
#include <QDebug>
#include <QThread>
#include <QMutex>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QNetworkProxy>
#include <QUrl>
#include <QProcess>
#include <QFile>
#include <QDir>
#include <QTextDocument>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QTimer>
#include <QCoreApplication>
#include <QSslConfiguration>
#include <QUrl>

#include <QXmlQuery>
#include <QXmlResultItems>
#include <errno.h>
#include <stdio.h>

#include "tools.h"
#include "zlib.h"

#include <unistd.h>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

#define NETWORK_SCRAP       0
//#define NETWORK_BL          1
#define NETWORK_HTTPSTATUS  2
#define NETWORK_REDIR       3
//#define NETWORK_PR          4
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
#define NETWORK_REDIR_STATUS 16


class Worker : public QObject
{
    Q_OBJECT
public:
    //explicit Worker(QObject *parent = 0);
    Worker(int timeout, Tools *tools);
    ~Worker();

    void initScrapThread(const QString &url, QHash<QString, QVariant> parameters);


public slots:
    void createRequest();
    void replyFinished();
    void getUrlRespCodeFinished();
    void getXpath(const QString &url, const QByteArray &sourceCode, QHash<QString, QVariant> replyProperties, const QString &xpathFilename="");
    void abortSignalReceived();

private slots:
    void xidelProcessFinished(int exitCode, QProcess::ExitStatus qExitStatus);
    void xidelProcessStarted();
    void xidelProcessError(QProcess::ProcessError);
    void threadTimeoutSlot();


signals:
    void scrapThreadReady();
    void resultReady(const QString &url, const QByteArray &sourceCode);
    void sourceCodeParsed(QHash<QString,QVariant>,QHash<QString,QVariant>);
    void threadTimeout(const QString &url, QHash<QString,QVariant>);
    void finished();
    void sendMsg(const QString &, const QString &);
    void sendRetry(const QString &url, QHash<QString,QVariant>);
    void updateThreadsLaunched();
    void updateItemsFound(int);
    void updateBannedProxies(QString,bool);
    void sendAbortSignal();
    void sendResultsToTable(QStringList,QHash<QString, QVariant>, QString);

    void sendHttpStatusCode(int, const QString &);
    void sendObl(double,const QString &);
    void sendLinkAlive(const QString &, const QString &);
    void sendDofollow(double, const QString &);
    void sendCheckPlatform(const QString &, const QString &);
    void sendRedirection(int, const QString &, const QString &);
    void sendWhoisResponse(const QString &, const QString &);
    void sendBacklinks(QHash<QString,QVariant>, const QString &);

private :
    Tools   *_tools;
    QString _url;
    QTimer *timer;
    QMutex _threadMutex;
    QString xpathPagination;
    QByteArray currentSourceCode;
    QHash<QString,QVariant> results;
    QHash<QString, QVariant> _replyProperties;
    QList<int> _redirectCodes;
    bool htmlEncoded;
    bool _working;
    bool _abort;
    bool _paginationTreated;
    bool _minimalLogs;
    bool _useProxyProviderRotation;
    bool _retryOnTimeout;
    QString _tmpStackFile;
    int _threadTimeout;
    QNetworkAccessManager *_qnam;
    QProcess *xidelProcess;

    void        sendEmptyResults();
    bool        checkForAbort();
    bool        checkIfHTTPStatusFound(int httpstatus, QString redirectUrl);
    void        httpCheckRetryMsg(QString msg);
    void        httpCheckSkipMsg(QString msg);
    QString     prettyFormatUrl(const QString &url);
    QString     preJsonTreatment(QString input, QString jsonStruct);
    void        getBacklinks(const QByteArray &sourceCode, const QString &url);
    QByteArray  gUncompress(const QByteArray &data);
    void        disconnectProcessSignals();
    void        writeContentToLogFile(QString logContent);
};


#endif // WORKER_H
