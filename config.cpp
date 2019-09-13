#include "config.h"
#include "mainwindow.h"
#include <QMessageBox>

Config::Config(MainWindow *mw, QString forceDefaultSe)
{
    mainWin = mw;
    QString debugConfig;


    QObject::connect(this, SIGNAL(updateSE(QMap<QString,QPair<QString, int> >, QString)),mw, SLOT(updateCustomSeCombo(QMap<QString,QPair<QString, int> >,QString)));
    QObject::connect(this, SIGNAL(updateCustom1(QString)),mw, SLOT(updateCustom1(QString)));
    QObject::connect(this, SIGNAL(updateCustom2(QString)),mw, SLOT(updateCustom2(QString)));
    QObject::connect(this, SIGNAL(populateFootprints(QStringList)),mw, SLOT(populateFootprints(QStringList)));
    QObject::connect(this, SIGNAL(clearFootprints()),mw, SLOT(clearFootprints()));
    connect(this, &Config::sendAppendLog, mw, &MainWindow::appendLog);

    // settings
    this->globalSettings =  new QSettings(QSettings::IniFormat, QSettings::UserScope,"RDDZ_Scraper", "RDDZ_Scraper");
    fullDebug = false;
    _forceDefaultSe = forceDefaultSe;

    debugConfig = this->readGeneralConfig("Global","debug").toString();
    if(debugConfig == "ilovemsglog")
        fullDebug=true;

    this->dbConnect();

    // WE check if the results directory exists
    QDir resultsDir(QApplication::applicationDirPath()+"/results");
    if(!resultsDir.exists())
        resultsDir.mkpath(resultsDir.absolutePath());

    this->loadProxies();
    this->loadFootprints();
    this->initUa();

}

Config::~Config()
{
    this->db.close();
    QSqlDatabase::database().close();
}

bool Config::proxy_enable()
{
    // We check if proxy is Enable && proxies list non empty
    if(!this->get_proxies().isEmpty())
        return this->getAutomationValue("useProxies");
    else
        return false;
}

bool Config::useProxyRotating()
{
   return this->getAutomationValue("useProxyRotating");
}


void Config::dbConnect()
{
    this->db = QSqlDatabase::addDatabase("QSQLITE");
    this->db.setHostName("localhost");
    this->db.setDatabaseName(QDir(QFileInfo(globalSettings->fileName()).absoluteDir()).path()+"/rddzscraper.db");
    this->dbs = this->db.open();

    if(this->db.open())
    {
        this->checkTables();
    }
}

void Config::checkTables()
{
    QStringList tables;
    tables << "configuration" << "automation" << "customs" << "footprints" << "proxies" << "scrapeEngines" << "seGroup" << "userAgent";

    foreach (QString table, tables) {
        QSqlQuery checkTableQuery("SELECT name FROM sqlite_master WHERE type='table' AND name='"+table+"';");
        if(checkTableQuery.last() == false)
        {
            qDebug() << "Table not found : " << table;
            this->createTableStructure(table);
        }
        else {
           this->checkColumnsForTable(table);
        }
        checkTableQuery.finish();
    }
}

void Config::createTableStructure(QString tableName)
{

    qDebug().noquote() << tr("   === Creating table [%1]").arg(tableName);
    if(tableName == "automation")
    {
        this->db.exec("CREATE TABLE IF NOT EXISTS `"+tableName+"` (`id` INTEGER PRIMARY KEY  NOT NULL , `key` VARCHAR NOT NULL , `value` BOOL NOT NULL);");

    }
    if(tableName == "configuration")
    { this->db.exec("CREATE TABLE IF NOT EXISTS `"+tableName+"` (`Id`	INTEGER PRIMARY KEY AUTOINCREMENT,`key` VARCHAR,`value` VARCHAR);");

    }
    else if(tableName == "customs")
    {
    this->db.exec("CREATE TABLE IF NOT EXISTS `"+tableName+"` (`id` INTEGER PRIMARY KEY AUTOINCREMENT,`custom1` TEXT,`custom2` TEXT, `seType` VARCHAR,`seId` INTEGER);");
    }
    else if(tableName == "footprints")
    {
    this->db.exec("CREATE TABLE IF NOT EXISTS `"+tableName+"` (`Id`	INTEGER PRIMARY KEY AUTOINCREMENT,`footprint` TEXT);");
    }
    else if(tableName == "proxies")
    {
    this->db.exec("CREATE TABLE IF NOT EXISTS `"+tableName+"` (`id` INTEGER PRIMARY KEY  NOT NULL , `hostname` VARCHAR NOT NULL , `port` VARCHAR NOT NULL , `username` TEXT , `password` TEXT);");
    }
    else if(tableName == "scrapeEngines")
    {
    this->db.exec("CREATE TABLE IF NOT EXISTS `"+tableName+"` ( `id` INTEGER PRIMARY KEY NOT NULL, `display_name` VARCHAR NOT NULL, `url` VARCHAR NOT NULL, `xpath_content` VARCHAR, `xpath_pagination` VARCHAR, `prefix` VARCHAR, `suffix` VARCHAR, `isDefault` BOOL, `specificUa` CHAR, `specificUaActive` BOOL, `informations` text, `jsEnable` INTEGER, `keepHtmlEncode` INTEGER, `jsonStructure` VARCHAR, `xpathInterpreter` INTEGER, `pause` INTEGER, `pausePagination` INTEGER, `customHeader` TEXT, `resendReqStr` TEXT, `resendReqStrNotContains` INTEGER, `resendHTTP` INTEGER, `resendHTTPCode` INTEGER, `resendHTTPAction` INTEGER, `resendHTTPCondition` INTEGER, `csvDelimiter` CHAR(1), `csvSeparator` CHAR(1), `deduplicate` INTEGER, resendReqStrAction INTEGER, seGroup_id INTEGER, acceptLanguage VARCHAR, acceptEncoding VARCHAR, host VARCHAR, postArray VARCHAR, requestType VARCHAR, contentType VARCHAR, sourceCodeType INTEGER, forceOutputEncoding VARCHAR, referer VARCHAR);");
    }
    else if(tableName == "seGroup")
    {
         QString defaultSEGroupName = tr("My Engines");

         this->db.exec("CREATE TABLE IF NOT EXISTS `"+tableName+"` (`Id`	INTEGER PRIMARY KEY AUTOINCREMENT,`Name` TEXT NOT NULL UNIQUE,`Enable` INTEGER,`Parent` INTEGER);");
         this->db.exec("INSERT INTO `"+tableName+"` (`Name`,`Enable`) VALUES (\""+defaultSEGroupName+"\",1)");
    }
    else if(tableName == "userAgent")
    {
        QSqlQuery query;
        this->db.exec("CREATE TABLE IF NOT EXISTS `"+tableName+"` (`id` INTEGER PRIMARY KEY  NOT NULL , `content` VARCHAR NOT NULL , `type` VARCHAR NOT NULL )");
        this->db.transaction();
        query.exec("INSERT INTO `"+tableName+"` ( `content`, `type`) VALUES ('RDDZScraper OpenSource Version', 'standard');");
        this->db.commit();
    }
    qDebug().noquote() << tr("   === Table [%1] created successfully").arg(tableName);
}

void Config::checkColumnsForTable(QString tableName)
{
    QHash<QString,QString> sqlColumns;
    QDebug deb = qDebug().noquote();

    deb << tr("Checking columns for table %1 ... ").arg(tableName);
    if(tableName == "automation")
    {
        sqlColumns.insert("key","VARCHAR NOT NULL");
        sqlColumns.insert("value","BOOL NOT NULL");
    }
    else if(tableName == "configuration")
    {
        sqlColumns.insert("key","VARCHAR");
        sqlColumns.insert("value","VARCHAR");
    }
    else if(tableName == "customs")
    {
        sqlColumns.insert("custom1","TEXT");
        sqlColumns.insert("custom2","TEXT");
        sqlColumns.insert("seType","VARCHAR");
        sqlColumns.insert("seId","INTEGER");
    }
    else if(tableName == "footprints")
    {
        sqlColumns.insert("footprint","TEXT");
        sqlColumns.insert("test","TEXT");

    }
    else if(tableName == "proxies")
    {
        sqlColumns.insert("hostname","VARCHAR NOT NULL");
        sqlColumns.insert("port","VARCHAR NOT NULL");
        sqlColumns.insert("username","TEXT");
        sqlColumns.insert("password","TEXT");
    }
    else if(tableName == "scrapeEngines")
    {
        sqlColumns.insert("informations","TEXT");
        sqlColumns.insert("jsEnable","INTEGER");
        sqlColumns.insert("keepHtmlEncode","INTEGER");
        sqlColumns.insert("jsonStructure","VARCHAR");
        sqlColumns.insert("xpathInterpreter","INTEGER");
        sqlColumns.insert("pause","INTEGER");
        sqlColumns.insert("pausePagination","INTEGER");
        sqlColumns.insert("customHeader","TEXT");
        sqlColumns.insert("resendReqStr","TEXT");
        sqlColumns.insert("resendReqStrNotContains","INTEGER");
        sqlColumns.insert("resendReqStrAction","INTEGER");
        sqlColumns.insert("resendHTTP","INTEGER");
        sqlColumns.insert("resendHTTPCondition","INTEGER");
        sqlColumns.insert("resendHTTPCode","INTEGER");
        sqlColumns.insert("resendHTTPAction","INTEGER");
        sqlColumns.insert("csvDelimiter","CHAR(1)");
        sqlColumns.insert("csvSeparator","CHAR(1)");
        sqlColumns.insert("deduplicate","INTEGER");
        sqlColumns.insert("seGroup_id","INTEGER");
        sqlColumns.insert("acceptLanguage","VARCHAR");
        sqlColumns.insert("acceptEncoding","VARCHAR");
        sqlColumns.insert("host","VARCHAR");
        sqlColumns.insert("contentType","VARCHAR");
        sqlColumns.insert("requestType","VARCHAR");
        sqlColumns.insert("postArray","VARCHAR");
        sqlColumns.insert("sourceCodeType","INTEGER"); // 0 => XPath, 1 => JSONiq
        sqlColumns.insert("forceOutputEncoding","VARCHAR");
        sqlColumns.insert("referer","VARCHAR");
    }
    else if(tableName == "seGroup")
    {
        sqlColumns.insert("Name","TEXT NOT NULL UNIQUE");
        sqlColumns.insert("Enable","INTEGER");
        sqlColumns.insert("Parent","INTEGER");
    }

    QHashIterator<QString, QString> sqlHashIt(sqlColumns);
    sqlHashIt.toFront();
    while(sqlHashIt.hasNext())
    {
        sqlHashIt.next();

        QSqlQuery checkSeField("SELECT " +  sqlHashIt.key() + " FROM `"+tableName+"`");
        if(checkSeField.lastError().text() != " ")
        {
            deb << tr(" Creating column %1 ... ").arg(sqlHashIt.value());
            this->db.exec("ALTER TABLE `"+tableName+"` ADD COLUMN " +  sqlHashIt.key() + " " + sqlHashIt.value());
        }
        checkSeField.finish();
    }

   deb << tr("done");
}


void Config::setDefaultSe(QString seIndexName)
{
    this->defaultSe = seIndexName;
}

void Config::setLastUsedEngine(QString seName)
{
    QHash<QString,QString> configSetup;
    QString savedName;

    savedName = (seName.isEmpty()) ? this->currentSe.second : seName;

    configSetup.insert("lastUsedSe", savedName);
    this->updateConfiguration(configSetup);
}

void Config::setCurrentSe(QPair<int,QString> se)
{
    this->currentSe = se;
}

int Config::getRealSeId(QString whatsThisRole)
{
    return (whatsThisRole.replace(QRegularExpression("^([^0-9]+)-([0-9]+).*$"),"\\2").toInt());
}


QStringList Config::get_ua(QString type)
{
    if(type == "mobile")
        return this->mobile_ua;
    else
        return this->ua;
}

void Config::checkBadUa(QStringList badUa)
{
    QList<int> badId;
    QString badUaJoin;

    badUaJoin = badUa.join("\",\"");
    badUaJoin.prepend("\"");
    badUaJoin.append("\"");

    QSqlQuery queryUa = db.exec("SELECT `id` FROM userAgent WHERE content IN ("+badUaJoin+")");

    while(queryUa.next())
        badId.append(queryUa.value(0).toInt());

    if(badId.size())
        this->deleteUa(badId);
}


void Config::initUa()
{
    QSqlQuery queryMobile = db.exec("SELECT `content` FROM userAgent WHERE type='mobile'");
    QSqlQuery queryStandard = db.exec("SELECT `content` FROM userAgent WHERE type='standard'");


    while(queryMobile.next())
        this->mobile_ua.append(queryMobile.value(0).toString());

    while(queryStandard.next())
        this->ua.append(queryStandard.value(0).toString());
}


int Config::getSEIdByName(QString SEName)
{
    QSqlQuery query;
    query.prepare("SELECT `id` FROM scrapeEngines WHERE `display_name`=? LIMIT 1");
    query.addBindValue(SEName);
    if(query.exec())
    {
        while(query.next())
        {
            return query.value("id").toInt();
        }
    }
    return(0);

}

QByteArray Config::getSpecificUa()
{
    QSqlQuery query;
    query.prepare("SELECT `specificUa` FROM scrapeEngines WHERE id=?");
    query.addBindValue(this->currentSe.first);
    if(query.exec())
    {
        if(query.last())
            return query.value(0).toByteArray();
    }
    return "";
}

void Config::deleteUa(QList<int> uaId)
{
    int i;
    for(i=0;i<uaId.size();i++)
        this->db.exec("DELETE FROM `userAgent` WHERE `id`="+QString::number(uaId.at(i)));
}

QString Config::getDefaultSe()
{
    return this->defaultSe;
}

void Config::insertFootprint(QString footprint)
{
    QSqlQuery query;
    query.prepare("INSERT INTO footprints('footprint') VALUES (?)");
    query.addBindValue(footprint);
    query.exec();
}

void Config::insertFootprints(QStringList footprints)
{
    int i;
    QString query;

    // We always delete first, in order to treat updates correctly
    db.exec("DELETE FROM footprints");

    for(i=0, query.clear();i<footprints.size();i++)
    {
        if(!footprints.at(0).isEmpty())
        {
            if (i%100 == 0)
            {
                if(i != 0)
                {
                QSqlQuery insertQuery(query);
                insertQuery.exec();
                query.clear();
                }
                query.append("INSERT INTO footprints SELECT '"+QString::number(i)+"' AS `id`, '"+footprints.at(i)+"' AS `footprint`");
            }
            else
                    query.append(" UNION SELECT "+QString::number(i)+", '"+footprints.at(i)+"'");
        }
    }
    QSqlQuery insertQuery(query);
    insertQuery.exec();
    this->loadProxies();

}

void Config::insertSeGroup(QString name)
{
    QSqlQuery query;
    query.prepare("INSERT INTO seGroup(`Name`,`Enable`) VALUES (?,?)");
    query.addBindValue(name);
    query.addBindValue(1);
    query.exec();
}

QStringList Config::getFootprints()
{
        QStringList footprints;

        if(this->dbs)
        {
            QSqlQuery query("SELECT footprint FROM footprints ORDER BY footprint ASC");
            while (query.next())
                footprints.append(query.value(0).toString());
        }
        else
            qFatal( "Failed to connect." );

        return footprints;
}

QStringList Config::getSeGroupNames()
{
    QStringList stack;
    QSqlQuery query("SELECT `Name` FROM seGroup ORDER BY `Name` ");
    while(query.next())
        stack.append(query.value(0).toString());
    return stack;
}

QStringList Config::getSeDisplayNames()
{
    QStringList stack;

    QSqlQuery query("SELECT `display_name` FROM scrapeEngines ORDER BY `display_name` ");
    while(query.next())
        stack.append(query.value(0).toString());
    return stack;
}

QStringList Config::get_proxies()
{
    this->proxies.removeDuplicates();
    return this->proxies;
}

int Config::getNbProxies()
{
    this->proxies.removeDuplicates();
    return this->proxies.count();
}


bool Config::getDisableWarnProxies()
{
    return this->getAutomationValue("disableWarnProxies");
}

bool Config::getTestProxiesOnGoogle()
{
    return this->getAutomationValue("testProxiesOnGoogle");
}

bool Config::getAutoRemoveBadUrl()
{
    return this->getAutomationValue("autoRemoveBadUrl");
}

bool Config::getAutoGetStatus()
{
    return this->getAutomationValue("autoGetStatus");
}

bool Config::getAutoPR()
{
    return this->getAutomationValue("autoPR");
}

bool Config::getAutoResolveRedir()
{
    return this->getAutomationValue("autoResolveRedir");
}

bool Config::getAutoDF()
{
    return this->getAutomationValue("autoDFStatus");
}

bool Config::getAutoBL()
{
    return this->getAutomationValue("autoBacklinks");
}

bool Config::getAutoOBL()
{
    return this->getAutomationValue("autoObl");
}

bool Config::getAutoIP()
{
    return this->getAutomationValue("autoIpAddr");
}


QString Config::getLastFootprint()
{
    return this->getConfigurationValue("lastFootprint");
}

QSettings *Config::getGlobalSettings()
{
    return this->globalSettings;
}

void Config::delete_proxy(QString proxy)
{
    int d = proxies.indexOf(proxy);
    proxies.removeAt(d);
}

void Config::deleteAllProxies()
{
     db.exec("DELETE FROM proxies");
}

void Config::deleteSe(int seId)
{
        QSqlQuery query;
        query.prepare("DELETE FROM scrapeEngines WHERE `id`=?");
        query.addBindValue(seId);
        query.exec();
}


void Config::updateSeList(QHash<QString, QVariant> scrapEngineConfig, QString oldDisplayName,QString type)
{
    QSqlQuery query;
    QString displayName;
    QString generatedQuery;


    displayName = (oldDisplayName.isEmpty()) ? scrapEngineConfig.value("display_name").toString() : oldDisplayName;

    if(type=="newdefault")
    {
        this->db.exec("UPDATE scrapeEngines SET `isDefault`=0 WHERE `isDefault`=1");
        query.prepare("UPDATE scrapeEngines SET `isDefault`=? WHERE `display_name`=? ");
        query.bindValue(0,QVariant(1));
        query.bindValue(1,QVariant(displayName));
        query.exec();
    }
    else if (type == "dontupdatecustom")
    {
        generatedQuery = "UPDATE scrapeEngines SET ";
        QHashIterator<QString, QVariant> hashIterator(scrapEngineConfig);
        hashIterator.toFront();
        while(hashIterator.hasNext())
        {
            hashIterator.next();
            if(!hashIterator.key().startsWith("custom1") || !hashIterator.key().startsWith("custom2"))
                generatedQuery += "`"+hashIterator.key()+"`=:"+hashIterator.key()+", ";
        }

        generatedQuery.remove(generatedQuery.length() -2, 2);
        generatedQuery += " WHERE `display_name`=:displaynametocheck";

        query.prepare(generatedQuery);
        hashIterator.toFront();
        while(hashIterator.hasNext())
        {
            hashIterator.next();
            if(!hashIterator.key().startsWith("custom1") || !hashIterator.key().startsWith("custom2"))
                query.bindValue(":"+hashIterator.key(),scrapEngineConfig.value(hashIterator.key()));
        }

        query.bindValue(":displaynametocheck",QVariant(displayName));
        query.exec();
    }
    else if(type == "customsOnly")
    {
        if(!scrapEngineConfig.value("custom1").toString().isEmpty() || !scrapEngineConfig.value("custom2").toString().isEmpty())
        {
            // We have to check if record exists
            QSqlQuery checkRecord;
            checkRecord.prepare("SELECT id FROM customs WHERE seId=? AND seType=? LIMIT 1");
            checkRecord.addBindValue(this->currentSe.first);
            checkRecord.addBindValue(this->currentSe.second.replace(QRegularExpression("^([^0-9]+)-[0-9]+.*$"),"\\1"));
            checkRecord.exec();
            if(checkRecord.last()) // We Update
            {
                query.prepare("UPDATE customs SET `custom1`=:custom1,`custom2`=:custom2, `seType`=:seType WHERE `id`=:id ");
                query.bindValue(":custom1",scrapEngineConfig.value("custom1"));
                query.bindValue(":custom2",scrapEngineConfig.value("custom2"));
                query.bindValue(":seType",this->currentSe.second.replace(QRegularExpression("^([^0-9]+)-[0-9]+.*$"),"\\1"));
                query.bindValue(":id",checkRecord.value(0).toInt());
                query.exec();
                query.finish();
            }
            else // We Insert
            {
                query.prepare("INSERT INTO customs (`custom1`,`custom2`, `seType`,`seId`) VALUES (:custom1,:custom2,:seType,:seId)");
                query.bindValue(":custom1",scrapEngineConfig.value("custom1"));
                query.bindValue(":custom2",scrapEngineConfig.value("custom2"));
                query.bindValue(":seType",this->currentSe.second.replace(QRegularExpression("^([^0-9]+)-[0-9]+.*$"),"\\1"));
                query.bindValue(":seId",this->currentSe.first);
                query.exec();
                query.finish();
            }
        }
    }
    else
    {
        generatedQuery = "UPDATE scrapeEngines SET ";
        QHashIterator<QString, QVariant> hashIterator(scrapEngineConfig);
        hashIterator.toFront();
        while(hashIterator.hasNext())
        {
            hashIterator.next();
            generatedQuery += "`"+hashIterator.key()+"`=:"+hashIterator.key()+", ";
        }


        generatedQuery.remove(generatedQuery.length() -2, 2);
        generatedQuery += " WHERE `display_name`=:displaynametocheck";

        query.prepare(generatedQuery);
        hashIterator.toFront();
        while(hashIterator.hasNext())
        {
            hashIterator.next();
            query.bindValue(":"+hashIterator.key(),scrapEngineConfig.value(hashIterator.key()));
        }

        query.bindValue(":displaynametocheck",QVariant(displayName));
        query.exec();
    }

    if(query.lastError().type())
        emit sendAppendLog(tr("Unable to update search engine config : %1").arg(query.lastError().text()), "error");
}

int Config::insertSE(QHash<QString, QVariant> scrapEngineConfig)
{
    bool isDefault;
    QSqlQuery insertQuery;
    QString generatedQuery;
    int i;

    // WE check if we already have a default SE, otherwise we set it
    QSqlQuery query = db.exec("SELECT id FROM scrapeEngines WHERE `isDefault`=1");
    isDefault = (query.last()) ? false : true;
    scrapEngineConfig.insert("isDefault",isDefault);

    QHashIterator<QString, QVariant> hashIterator(scrapEngineConfig);
    generatedQuery = "INSERT INTO scrapeEngines (";
    hashIterator.toFront();
    while(hashIterator.hasNext())
    {
        hashIterator.next();
        generatedQuery += "`"+hashIterator.key()+"`,";
    }
    generatedQuery.remove(generatedQuery.length()-1,1);
    generatedQuery += ") VALUES (";
    for(i=0;i<scrapEngineConfig.size();i++)
        generatedQuery += "?,";
    generatedQuery.remove(generatedQuery.length()-1,1);
    generatedQuery += ")";

    insertQuery.prepare(generatedQuery);
    hashIterator.toFront();
    while(hashIterator.hasNext())
    {
        hashIterator.next();
        insertQuery.addBindValue(scrapEngineConfig.value(hashIterator.key()));
    }
    insertQuery.exec();

    // Errors
    if(insertQuery.lastError().type())
    {
        emit sendAppendLog(tr("Unable to save scrap Engine configuration : %1").arg(insertQuery.lastError().text()), "error");
        return 0;
    }
    return insertQuery.lastInsertId().toInt();
}

void Config::updateAutomation(QString key,bool value)
{
    QSqlQuery selectQuery = db.exec("SELECT `id` FROM automation WHERE key='"+key+"'");

    if(selectQuery.last())
    {
        QSqlQuery updateQuery;
        updateQuery.prepare("UPDATE automation SET `key`=?, `value`=? WHERE `key`=?");
        updateQuery.addBindValue(key);
        updateQuery.addBindValue(value);
        updateQuery.addBindValue(key);
        updateQuery.exec();

    }
    else
        insertAutomation(key,value);
}

void Config::insertAutomation(QString key,bool value)
{
    QSqlQuery query;

    query.prepare("INSERT INTO automation (`key`,`value`) VALUES (?,?)");
    query.addBindValue(key);
    query.addBindValue(value);
    query.exec();
}


bool Config::getAutomationValue(QString key)
{
    QSqlQuery query = db.exec("SELECT `value` FROM automation WHERE `key`='"+key+"'");

    if(query.last())
        return query.value(0).toBool();
    else
        return false;
}

void Config::writeGeneralConfig(QString group,QString key, QString value)
{
    this->globalSettings->beginGroup(group);
    this->globalSettings->setValue(key,value);
    this->globalSettings->endGroup();
    this->globalSettings->sync();
}


QVariant Config::readGeneralConfig(QString group,QString key, QVariant defaultValue)
{
    QVariant value;

    this->globalSettings->beginGroup(group);
    value = this->globalSettings->value(key,defaultValue);
    this->globalSettings->endGroup();

    return value;
}

void Config::insertProxies(QHash<int, QStringList> proxiesList,bool deleteAll)
{
    int i, maxId;
    QStringList items;
    QString query;


    // We always delete first, in order to treat updates correctly
    if(deleteAll)
    db.exec("DELETE FROM proxies");

    maxId = 0;
    QSqlQuery getMaxId("SELECT MAX(id) FROM proxies");
    if(getMaxId.last())
    maxId = getMaxId.value(0).toInt()+1;

    for(i=0, query.clear();i<proxiesList.size();i++,maxId++)
    {
        items.clear();
        items = proxiesList[i];

        if(!items.at(0).isEmpty())
        {
            if (i%100 == 0)
            {
                if(i != 0)
                {
                QSqlQuery insertQuery(query);
                insertQuery.exec();
                query.clear();
                }
                query.append("INSERT INTO proxies SELECT '"+QString::number(maxId)+"' AS `id`, '"+items.at(0)+"' AS `hostname`, '"+items.at(1)+"' AS `port`, '"+items.at(2)+"' AS `username`, '"+items.at(3)+"' AS `password` ");
            }
            else
                    query.append(" UNION SELECT "+QString::number(maxId)+", '"+items.at(0)+"','"+items.at(1)+"','"+items.at(2)+"', '"+items.at(3)+"'");
        }
    }
    QSqlQuery insertQuery(query);
    insertQuery.exec();
    this->loadProxies();
}


void Config::setProxies(QStringList proxies)
{
    if(proxies.size())
    {
        this->proxies.clear();
        this->proxies = proxies;
    }
}


void Config::loadProxies()
{
    proxies.clear();

    QString proxy;
    QSqlQuery query = db.exec("SELECT `hostname`,`port`,`username`,`password` FROM proxies");

    while(query.next())
    {
        if(query.value(2).toString().isEmpty() && query.value(3).toString().isEmpty())
            proxy = query.value(0).toString()+":"+query.value(1).toString();
            else
        proxy = query.value(0).toString()+":"+query.value(1).toString()+":"+query.value(2).toString()+":"+query.value(3).toString();
        if(!query.value(0).toString().isEmpty())
            this->proxies.append(proxy);
    }
}

void Config::updateConfiguration(QHash<QString, QString> newHashConfig)
{
    QSqlQuery queryInsert;
    QHashIterator<QString, QString> i(newHashConfig);

    if(newHashConfig.isEmpty())
        return;

    while (i.hasNext())
    {
        i.next();

        if(hashConfiguration.contains(i.key()))
        {
            if(hashConfiguration.value(i.key()) != i.value())
            {
                queryInsert.prepare("UPDATE configuration SET `value`=? WHERE `key`=?");
                queryInsert.addBindValue(i.value());
                queryInsert.addBindValue(i.key());
                queryInsert.exec();
                queryInsert.finish();

                this->hashConfiguration.insert(i.key(),i.value());
            }
        }
        else // New configuration value
        {
            QHash<QString,QString> stackHash;
            stackHash.clear();
            stackHash.insert(i.key(),i.value());

            this->hashConfiguration.insert(i.key(),i.value());
            this->insertConfiguration(stackHash);
        }
    }
}

void Config::insertConfiguration(QHash<QString, QString> newHashConfig)
{
    QSqlQuery queryInsert;
    QHashIterator<QString, QString> i(newHashConfig);
    QList<QString> keys;


    if(newHashConfig.isEmpty())
        return;

    keys = newHashConfig.keys();

    while (i.hasNext())
    {
        i.next();

        queryInsert.prepare("INSERT INTO configuration (`key`,`value`) VALUES (?,?)");
        queryInsert.addBindValue(i.key());
        queryInsert.addBindValue(i.value());
        queryInsert.exec();
        queryInsert.finish();

        this->hashConfiguration.insert(i.key(),i.value());
    }
}

QString Config::getConfigurationValue(QString key, QString defaultValue,bool forceDbCheck)
{
    if(forceDbCheck == false)
    return this->hashConfiguration.value(key,defaultValue);
    else
    {
        QSqlQuery query = db.exec("SELECT `value` FROM configuration WHERE `key`='"+key+"'");

        if(query.last())
            return query.value(0).toString();
        else
            return QString();
    }
}

QHash<QString, QString> Config::getHashConfiguration()
{
    return this->hashConfiguration;
}



QHash<QString, QVariant> Config::getScrapEngineConfigurationHash(int idSE, bool fullExport)
{
    QHash<QString, QVariant> scrapEngineConfig;
    int i;
    QSqlQuery query;

    if(fullExport == true)
        query.prepare("SELECT * FROM scrapeEngines WHERE `id`=? LIMIT 1");
    else
        query.prepare("SELECT `display_name`,`url`,`xpath_content`,`xpath_pagination`,`prefix`,`suffix`,`specificUa`,`specificUaActive`,`informations`,`jsEnable`,`customHeader`,`csvDelimiter`,`csvSeparator`,`deduplicate`,`pause`,`pausePagination` FROM scrapeEngines WHERE `id`=? LIMIT 1");
    query.addBindValue(idSE);
    if(query.exec())
    {
        QSqlRecord qRecord = query.record();
        while(query.next())
        {
            for(i = 0;i < qRecord.count();i++)
                scrapEngineConfig.insert(qRecord.fieldName(i),query.value(qRecord.fieldName(i)));

        }
    }
    return scrapEngineConfig;
}


void Config::loadConfiguration()
{
    QSqlQuery query("SELECT `key`,`value` FROM configuration");

    // Table Configuration
    while(query.next())
        this->hashConfiguration.insert(query.value(0).toString(), query.value(1).toString());
}


void Config::loadCustoms(bool custom1Checked, bool custom2Checked)
{
    QString custom1,custom2;
    QSqlQuery query;

    query.prepare("SELECT customs.`custom1`,customs.`custom2` FROM customs WHERE customs.`seId`=? AND customs.seType=?");
    query.addBindValue(this->currentSe.first);
    query.addBindValue(this->currentSe.second.replace(QRegularExpression("^([^0-9]+)-[0-9]+.*$"),"\\1"));

    if(query.exec())
    {
        while(query.next())
        {
            custom1 = query.value(0).toString();
            custom2 = query.value(1).toString();
        }
    }


    if(!custom1Checked)
        emit updateCustom1(custom1);
    if(!custom2Checked)
        emit updateCustom2(custom2);
}

QMap<QString,QPair<QString, int> > Config::getAllSe()
{
    QMap<QString,QPair<QString, int> > SEMap; // <groupname <id,SEName>>
    QPair<QString, int> SEName;
    if(this->dbs)
    {
        QSqlQuery query("SELECT scrapeEngines.id,`display_name`,`isDefault`,`Name` FROM scrapeEngines LEFT JOIN seGroup ON scrapeEngines.seGroup_id=seGroup.Id ORDER BY seGroup_id ASC,  `display_name` DESC");

        for (int i =0;query.next();i++)
        {
            if((this->getConfigurationValue("lastUsedSe") == "local-" + QString::number(query.value("id").toInt()) && _forceDefaultSe.isEmpty()) || !i)
            {
                 this->defaultSe = this->getConfigurationValue("lastUsedSe","",true);
            }
            else if (!_forceDefaultSe.isEmpty() && _forceDefaultSe == query.value("display_name").toString())
                this->defaultSe=query.value("display_name").toString();


            SEName.first = query.value("display_name").toString();
            SEName.second = query.value("id").toInt();
            SEMap.insertMulti(query.value("Name").toString(),SEName);
        }
        return SEMap;
    }
    else
        qFatal( "Failed to connect." );
}

void Config::setParentGroup(QString seName, int seGroupId)
{
    QSqlQuery queryInsert;

    if(!seGroupId)
        return;

    queryInsert.prepare("UPDATE scrapeEngines SET `seGroup_id`=? WHERE `display_name`=?");
    queryInsert.addBindValue(seGroupId);
    queryInsert.addBindValue(seName);
    queryInsert.exec();
}

void Config::setMultipleParentGroup(QHash<int, int> idsList)
{
    QString query;

    int i;
    QStringList idToUpdate;
    QHashIterator<int,int> hashIt(idsList);

    hashIt.toFront();
    // We build the query
    query = "UPDATE scrapeEngines SET seGroup_id = CASE id";
    for(i=0;hashIt.hasNext();i++)
    {

        hashIt.next();
        query.append(" WHEN "+QString::number(hashIt.key())+" THEN "+QString::number(hashIt.value())+" " );
        idToUpdate.append(QString::number(hashIt.key()));
    }
    query.append(" END WHERE id IN ("+idToUpdate.join(",")+");");

    QSqlQuery insertQuery(query);
    insertQuery.exec();
    insertQuery.lastError();
}

void Config::updateSeGroupName(QString oldName, QString newName)
{
    QSqlQuery query;

    if(oldName.isEmpty() || newName.isEmpty())
        return;

    query.prepare("UPDATE seGroup SET `Name`=? WHERE `Name`=?");
    query.addBindValue(newName);
    query.addBindValue(oldName);
    query.exec();
}

void Config::setSeGroupStatus(QString seGroupName,int newStatus)
{
    QSqlQuery query;

    query.prepare("UPDATE seGroup SET `Enable`=? WHERE `Name`=?");
    query.addBindValue(newStatus);
    query.addBindValue(seGroupName);
    query.exec();
}

void Config::deleteSeGroup(QString SEGroupName)
{
    QSqlQuery query;

    query.prepare("DELETE FROM seGroup WHERE `Id`=?");
    query.addBindValue(this->getSEGroupIdByName(SEGroupName));
    query.exec();
}

QString Config::getSeGroupNameById(int id)
{
    QSqlQuery query;
    query.prepare("SELECT Name FROM seGroup WHERE `Id`=? LIMIT 1");
    query.addBindValue(id);

    if(query.exec())
    {
        while(query.next())
        {
            return query.value("Name").toString();
        }
    }
    return(QString());
}

int Config::getSEGroupIdByName(QString seGroupName)
{
    QSqlQuery query;
    query.prepare("SELECT Id FROM seGroup WHERE `Name`=? LIMIT 1");
    query.addBindValue(seGroupName);

    if(query.exec())
    {
        while(query.next())
        {
            return query.value("Id").toInt();
        }
    }
    return(0);
}

int Config::getSeGroupIdByChildName(QString seName)
{
    QSqlQuery query;
    query.prepare("SELECT `seGroup_id` FROM `scrapeEngines` WHERE `display_name`=? LIMIT 1");
    query.addBindValue(seName);

    if(query.exec())
    {
        while(query.next())
        {
            return query.value("seGroup_id").toInt();
        }
    }
    return(0);
}

int Config::getSeGroupStatus(QString seGroupName)
{
    QSqlQuery query;
    query.prepare("SELECT `Enable` FROM seGroup WHERE `Name`=? LIMIT 1");
    query.addBindValue(seGroupName);

    if(query.exec())
    {
        while(query.next())
        {
            return query.value("Enable").toInt();
        }
    }
    return(1);
}

void Config::loadSE()
{
    QMap<QString,QPair<QString,int> > SEMap = this->getAllSe();
    emit updateSE(SEMap, this->defaultSe);
}

void Config::loadFootprints()
{
    QStringList footprints;

    if(this->dbs)
    {
        emit clearFootprints();
        footprints.append("");
        QSqlQuery query("SELECT footprint FROM footprints ORDER BY footprint ASC");
        while (query.next())
            footprints.append(query.value(0).toString());

        emit populateFootprints(footprints);
    }
    else
        qFatal( "Failed to connect." );
}

QString Config::exportSE(QHash<QString,QVariant> scrapEngineConfiguration, bool exportCustoms)
{
    // First we put the QHash into a QMap in order to always have the same key output order
    QHashIterator<QString, QVariant> hashIterator(scrapEngineConfiguration);
    QMap<QString, QVariant> mapConfiguration;
    hashIterator.toFront();
    while(hashIterator.hasNext())
    {
        hashIterator.next();
        mapConfiguration.insert(hashIterator.key(),hashIterator.value());
    }


    QString dbFields;
    QString valuesToInsert;
    QString generatedQuery;
    QStringList boolFields;
    QStringList ignoreFields;
    QMapIterator<QString,QVariant> mapIterator(mapConfiguration);
    boolFields << "jsEnable" << "keepHtmlEncode" << "pausePagination";
    if(exportCustoms == false)
        ignoreFields << "id" << "custom1" << "custom2";
    else
        ignoreFields << "id" << "seGroup_id";
    mapIterator.toFront();
    while(mapIterator.hasNext())
    {
        mapIterator.next();
        if(ignoreFields.contains(mapIterator.key())) // We don't export ignore fields !!
            continue;
        dbFields += "`"+mapIterator.key()+"`,\n";
        if(boolFields.contains(mapIterator.key()))
            valuesToInsert += ""+QString::number(mapIterator.value().toBool() ? 1 : 0)+",\n";
        else if(mapIterator.key() == "isDefault")
            valuesToInsert += "0,\n";
        else if(mapIterator.value().type() == QVariant::String)
            valuesToInsert += "\""+mapIterator.value().toString().replace('"', "\"\"")+"\",\n";
        else if(mapIterator.value().type() == QVariant::Int || mapIterator.value().type() == QVariant::LongLong)
            valuesToInsert +=  ""+QString::number(mapIterator.value().toInt())+",\n";
    }


    dbFields.remove(dbFields.length()-2,2);
    valuesToInsert.remove(valuesToInsert.length()-2,2);
    generatedQuery = "INSERT INTO scrapeEngines\n (\n" + dbFields + "\n)\n VALUES (\n" + valuesToInsert + "\n);";

    return (generatedQuery);
}

void Config::importSE(QString content)
{
    // TODO : Add error gesture
    db.exec(content);
    loadSE();
}

void Config::updateDefaultEngine(QString content)
{
    QSqlQuery currentQuery;
    if(!content.isEmpty())
    {
        QStringList queries = content.split(QRegExp(";(\\r)?\\n"));
        foreach(QString query, queries)
        {
            if(!query.isEmpty())
            {
                currentQuery = db.exec(query);
                if(currentQuery.lastError().type())
                {
                    mainWin->errorDuringUpdate = true;
                    emit sendAppendLog(tr("Updating default engine file error : %1").arg(currentQuery.lastError().text()), "error");
                }
            }
        }
        loadSE();

    }

}
