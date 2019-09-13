#ifndef TOOLS_H
#define TOOLS_H
#ifndef CONFIG_H
#include "config.h"
#endif
#include <QtNetwork>
#include <QTreeWidget>
#include <QListWidget>

//#include "ressources/tld/tld.h"


class Tools : public QWidget
{
    Q_OBJECT
public :
        Tools();
        ~Tools();
        void        clearUrlProxies();
        int         setProxy(QNetworkAccessManager *m, QStringList l_proxies, QString url, int type);
        QString switchProxy(QStringList l_proxies, QString url);
        QByteArray  openFile(QString filetype="scrap");
        void        writeFile(QString, QMap<int, QStringList> content, QString format,QHash<QString, QChar> delimiters);
        void        saveFile(QMap<int,QStringList> content, QStringList extensions, QHash<QString, QChar> delimiters);
        QByteArray  randomUa(QStringList);
        void        deleteItemsFromQtree(QList<int>, QTreeWidget *);
        // Proxies methods
        QString     randomProxy(QStringList,int uid=0);
        QStringList proxyInfos(QString proxy);
        int         getProxyIndex(QString proxy);
        void        setBannedProxy(QString proxy, bool banned);
        QStringList     allProxies;
        QStringList     proxiesBanned;
        QList<int>      proxiesBannedIndex;
        QHash<QString,QString> proxiesBannedDomain;

        QMap<int, QVariantList> parseInputFile(QByteArray content, QStringList args, int colCount, QStringList scrapFileHeaderList, QHash<int, QString> tableHeaderData);


        static QString getDomain(QString url, bool domainOnly=true, bool scheme=true);
        static QString toCamelCase(QString str);
        static QList<QVariant> stringListToVariantList(QStringList list);
        static QStringList dedupStringList(QStringList list);
        static bool isGoogleUrl(QString url);
        static bool isGoogleCaptchaUrl(QString url);
        static bool isGoogleCaptchaImgUrl(QString url);
        void        removeFilesByExt(QString directory, QString fileExt);

        static QString platformDetector(QString src, QList<QPair<QByteArray, QByteArray> > headers);
        static QString encryptIt(QByteArray string);
        static QString simpleCrypt(QString string);

        QString jsonToXml(QString jsonStr, int currentIndex=0, int subIndex=0, QString outputXml="");

        static bool extensionSupportedByApi(QString tld, QString api);
        static qint64 getCurrentTimestamp();

private :
        char *      myQstrtoStr(QString);
        QHash<QString,QString> urlProxies;
        int         proxiesIndex;
        Config              *config;

        QString     _outputXml;

        QPair<int,QByteArray> sendBlockingRequest(QUrl Url);

        static QString getPlatformCategories(QJsonArray categoriesJson, QJsonObject parentObject);
        static bool plateformHasHeader(QJsonObject fileHeaders, QList<QPair<QByteArray, QByteArray> > serverHeaders);  
        static bool platformHasValue(QJsonValue jsonValue, QString src);
        static bool platformHasScript(QJsonValue jsonValue, QString src);
        static bool platformHasMeta(QJsonObject jsonObj, QString src);


private slots :

};

#endif // TOOLS_H
