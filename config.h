#ifndef CONFIG_H
#define CONFIG_H
#include <QWidget>
#include <QSettings>
#include <QtSql>
#include <QApplication>
#include <QHashIterator>
#include <QProgressDialog>


class MainWindow;

class Config : public QWidget
{
    Q_OBJECT
public :
    Config(MainWindow *mw=nullptr,QString forceDefaultSe="");
    ~Config();


    QHash<QString, QVariant> scrapEngineConfig;

    bool            fullDebug;

    void            loadProxies();
    void            loadFootprints();
    void            loadSE();
//    void            loadDb();
    void            loadMenuOptions();
    void            loadConfiguration();
    bool            proxy_enable();
    bool            useProxyRotating();

    QStringList     get_proxies();
    QStringList     get_ua(QString);
    QStringList     getFootprints();

    int             getNbProxies();

    void            setCurrentSe(QPair<int, QString> se);
    void            updateDefaultEngine(QString content);

    // Getting settings
    QSettings       *getGlobalSettings();
    QString         getConfigurationValue(QString key, QString defaultValue="", bool forceDbCheck=false);
    QString         getLastFootprint();
    QHash<QString, QString> getHashConfiguration();
    QHash<QString, QVariant> getScrapEngineConfigurationHash(int idSE, bool fullExport=true);
    QMap<QString, QPair<QString, int> > getAllSe();
    QString         getSeGroupNameById(int id);
    int             getRealSeId(QString whatsThisRole);
    int             getSEGroupIdByName(QString seGroupName);
    int             getSeGroupStatus(QString seGroupName);
    int             getSeGroupIdByChildName(QString seName);





    // SE Groups
    void            setParentGroup(QString seName, int seGroupId);
    void            updateSeGroupName(QString oldName, QString newName);
    void            setMultipleParentGroup(QHash<int,int>);
    void            setSeGroupStatus(QString seGroupName, int newStatus);
    void            deleteSeGroup(QString SEGroupName);

    // Getting custom SE
    QStringList     getSeDisplayNames();
    QStringList     getSeGroupNames();
    QString         getDefaultSe();
    int             getSEIdByName(QString SEName);

    // Getting Menu Options
    bool            getDisableWarnProxies();
    bool            getTestProxiesOnGoogle();
    bool            getAutoRemoveBadUrl();
    bool            getAutoResolveRedir();
    bool            getAutoGetStatus();
    bool            getAutoPR();
    bool            getAutoDF();
    bool            getAutoBL();
    bool            getAutoOBL();
    bool            getAutoIP();

    void            updateAutomation(QString key, bool value);
    void            updateConfiguration(QHash<QString,QString> newHashConfig);
    void            insertConfiguration(QHash<QString, QString> newHashConfig);


    void            insertFootprint(QString footprint);
    void            insertFootprints(QStringList footprints);


    void            writeGeneralConfig(QString group,QString key, QString value);
    QVariant        readGeneralConfig(QString group, QString key, QVariant defaultValue="");


    void            updateSeList(QHash<QString, QVariant> scrapEngineConfig, QString oldDisplay="", QString type="default");
    void            deleteSe(int seId);
    int             insertSE(QHash<QString, QVariant> scrapEngineConfig);
    void            insertSeGroup(QString name);


    void            insertProxies(QHash<int,QStringList> proxiesList, bool deleteAll=false);
    void            delete_proxy(QString proxy);
    void            deleteAllProxies();
    void            regenProxiesList();

    void            loadCustoms(bool custom1Checked=true, bool custom2Checked=true);


    QByteArray      getSpecificUa();
    void            checkBadUa(QStringList badUa);

    QString         exportSE(QHash<QString, QVariant> scrapEngineConfiguration, bool exportCustoms=false);

    void            importSE(QString content);

    // Set variable values
    void            setProxies(QStringList proxies);
    void            setLastUsedEngine(QString seName="");
    void            setDefaultSe(QString seIndexName);

signals:
    void            updateSE(QMap<QString,QPair<QString, int> >, QString);
    void            updateCustom1(QString);
    void            updateCustom2(QString);
    void            populateFootprints(QStringList);
    void            clearFootprints();
    void            sendAppendLog(QString, QString);

private:
    MainWindow      *mainWin;
    QString         defaultSe;
    QStringList     ua;
    QStringList     mobile_ua;
    QStringList     proxies;
    QString         current_tld;
    bool            use_proxies;
//    QString         currentSe;
    QPair<int,QString>  currentSe;
    QString         _forceDefaultSe; // If set in commandLine
    QJsonArray     _remoteScrapEngines;

    QHash<QString, QString> hashConfiguration;
    QSettings       *globalSettings;

    QSqlDatabase    db;
    bool            dbs;



    void            dbConnect();
    void            checkTables();
    void            createTableStructure(QString tableName);
    void            checkColumnsForTable(QString tableName);

    void            initUa();
    void            deleteUa(QList<int> uaId);
    void            insertAutomation(QString key, bool value);
   // void            insertConfiguration(QString key, QString value);
    bool            getAutomationValue(QString key);



};

#endif // CONFIG_H
