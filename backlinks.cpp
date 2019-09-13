#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "backlinks.h"

#ifndef NETWORK_H
#include "network.h"
#endif


Backlinks::Backlinks(MainWindow *mw, Config *conf, Scraper *parentScraper)
{
    config  = conf;
    mainWin = mw;
    scraper = parentScraper;
    maxBlsPerRequest = 1000;
    majesticMaxItemPerQuery = 10;
    blStartOffset = 0;
    ahrefsBlCounter = 0;
    majesticStartAt = 0;
    urlsLeft        = 0;
    this->blsThreads = 0;
    this->blsCurrentThread = 0;
    this->blGlobalIndex = 0;
    ahrefsMotherFucker = false;
    ahrefsSelectParam = "url_from,url_to,ahrefs_rank,domain_rating,links_external,nofollow,anchor,link_type,ip_from";

    _apisUrls.insert("ahrefs","https://apiv2.ahrefs.com/");
    _apisUrls.insert("majestic","https://enterprise.majesticseo.com/api/json");
    _apisUrls.insert("seobserver","https://api0.seobserver.com/api/json");
    _apisUrls.insert("moz","http://lsapi.seomoz.com/");

    ahrefsBacklinksMode = config->getConfigurationValue("backlinksCheckMode");
    qRegisterMetaType<QMap<int,QList<QVariant> > >("QMap<int,QList<QVariant> >");
    QObject::connect(this, SIGNAL(addFields(QMap<int,QList<QVariant> >,int)),mw, SLOT(addFields(QMap<int,QList<QVariant> >,int)));
    QObject::connect(this, SIGNAL(updateBacklinks(QString,qlonglong)), mw, SLOT(updateBacklinks(QString,qlonglong)));
    QObject::connect(this, SIGNAL(enableAhrefsButton()),mw,SLOT(scrapButtonEnabled()));
    QObject::connect(this, SIGNAL(enableMajesticButton()),mw,SLOT(scrapButtonEnabled()));


}

Backlinks::~Backlinks()
{

}

void Backlinks::blError(QString err)
{
    scraper->appendLog(err, "error");
    scraper->appendLog(tr("Backlinks check finished"));
}

/////////////////////////////////////////////////////////////////
////                        API Balance
/////////////////////////////////////////////////////////////////

QPair<QString,QByteArray> Backlinks::genSeobserverAuthFields()
{
    QPair<QString,QByteArray> authFields;
    qint64 currentTimeStamp = Tools::getCurrentTimestamp();
    authFields.first = QString::number(currentTimeStamp);
    authFields.second = QCryptographicHash::hash(QCryptographicHash::hash(
                                                     (config->readGeneralConfig("ExternalPrivateKeys", "SEObserver").toString() + QString::number(currentTimeStamp)).toLatin1(),
                                                     QCryptographicHash::Md5),QCryptographicHash::Sha1).toHex();

    return authFields;
}

void Backlinks::getAhrefsBalance(QString apiKey)
{
    QUrl blQurl;
    QUrlQuery blQuery;

    blQurl.setUrl(_apisUrls.value("ahrefs"));
    blQuery.addQueryItem("from","subscription_info");
    blQuery.addQueryItem("output","xml");
    blQuery.addQueryItem("token",apiKey);
    blQurl.setQuery(blQuery);

    emit createHttpRequest(blQurl.toString(),"standard",AHREFS_BALANCE);
}

void Backlinks::getMajesticBalance(QString accessToken,QString service)
{
    QUrl blQurl;
    QUrlQuery blQuery;

    if(service == "Majestic")
    {
        blQurl.setUrl(_apisUrls.value("majestic"));
        blQuery.addQueryItem("accesstoken",accessToken);
        blQuery.addQueryItem("privatekey",config->readGeneralConfig("ExternalPrivateKeys", "Majestic").toString());
    }
    else
    {
        QPair<QString,QByteArray> authFields = this->genSeobserverAuthFields();

        blQurl.setUrl(_apisUrls.value("seobserver"));
        blQuery.addQueryItem("app_api_key",accessToken);
        blQuery.addQueryItem("auth_signature",authFields.second);
        blQuery.addQueryItem("auth_ts",authFields.first);
        blQuery.addQueryItem("auth_partner","RDDZ");
    }

    blQuery.addQueryItem("cmd","GetSubscriptionInfo");
    blQurl.setQuery(blQuery);

    emit createHttpRequest(blQurl.toString(),"standard",MAJESTIC_BALANCE);
}

void Backlinks::getMajesticBalanceFinished()
{
    QNetworkReply   *reply;
    QString         majesticCode;
    QJsonDocument   jsonDoc;
    QJsonObject     jsonObj;

    reply = qobject_cast<QNetworkReply *>(sender());
    QByteArray rep(reply->readAll());

    emit scraper->deleteReplyFromQhash(reply->property("realUrl").toString());

    jsonDoc = QJsonDocument::fromJson(rep);
    jsonObj = jsonDoc.object();
    majesticCode = jsonObj.value("Code").toString();

    if(majesticCode == "OK")
    {
        // SEObserver Case
        if(jsonObj.value("data").isObject())
        {
            jsonObj = jsonObj.value("data").toObject();
        }
        emit scraper->updateBacklinksBalanceUnits(QString::number(jsonObj.value("TotalAnalysisResUnits").toInt()), "majesticTotalAnalysisResUnits");
        emit scraper->updateBacklinksBalanceUnits(QString::number(jsonObj.value("TotalIndexItemInfoResUnits").toInt()), "majesticTotalIndexItemInfoResUnits");
        emit scraper->updateBacklinksBalanceUnits(QString::number(jsonObj.value("TotalRetrievalResUnits").toInt()), "majesticTotalRetrievalResUnits");

    }
    else
    {
        QString  errorMsg, fullErrorMsg;

        errorMsg    = jsonObj.value("ErrorMessage").toString();
        fullErrorMsg  = jsonObj.value("FullError").toString();
        scraper->appendLog("[Majestic SEO] "+QString(errorMsg)+"  "+QString(fullErrorMsg));
    }
    if(reply)
    {
        scraper->disconnectReply(reply, MAJESTIC_BALANCE);
        reply->deleteLater();
    }
    emit enableMajesticButton();
}

void Backlinks::getAhrefsBalanceFinished()
{
    QNetworkReply   *reply;
    QString      hasError, ahrefsUnits;

    reply = qobject_cast<QNetworkReply *>(sender());
    QString str(reply->readAll());

    emit scraper->deleteReplyFromQhash(reply->property("realUrl").toString());

    hasError  = scraper->getXpath(str,"//error/string()","").at(0);
    if(hasError.isEmpty())
    {
        ahrefsUnits = scraper->getXpath(str,"//rows_left/string()","").at(0);
        scraper->updateBacklinksBalanceUnits(ahrefsUnits, "ahrefs");
    }
    else
    {
        scraper->appendLog("[Ahrefs] "+hasError, "error");
        scraper->updateBacklinksBalanceUnits(hasError, "ahrefs");
    }

    if(reply)
    {
        scraper->disconnectReply(reply, AHREFS_BALANCE);
        reply->deleteLater();
    }

    emit enableAhrefsButton();
}

/////////////////////////////////////////////////////////////////
////                        API Backlinks
/////////////////////////////////////////////////////////////////

void Backlinks::processBacklinksUrl(QString backlinkService)
{
    int i, nextStep, backlinksInterval;
    if (backlinkService == "Majestic" || backlinkService == "SEObserver")
        backlinksInterval = 2;
    else if (backlinkService == "Ahrefs") // For Ahrefs : mono-thread
        backlinksInterval = 1;
    else
        backlinksInterval = 1;

    if(!this->blsCurrentThread)
    {
        urlsLeft = urlToCheck.size();
        nextStep = (blsCurrentThread+backlinksInterval < urlToCheck.size()) ?  blsCurrentThread+backlinksInterval : urlToCheck.size();
        for(i=blsCurrentThread;i < nextStep;i++)
        {
            mappingStartAt.insert(urlToCheck.at(i),0);
            mappingBacklinkIndexForUrl.insert(urlToCheck.at(i),0);
            mappingMajesticStartAt.insert(urlToCheck.at(i),0);
             QtConcurrent::run(this, &Backlinks::scrapBacklinks, urlToCheck.at(i), backlinkService,true);
        }
        backlinksMutex.lock();
        blsCurrentThread = i - 1;
        backlinksMutex.unlock();
    }
    else if (blsCurrentThread <  urlToCheck.size())
         QtConcurrent::run(this, &Backlinks::scrapBacklinks, urlToCheck.at(blsCurrentThread), backlinkService,true);
}

void Backlinks::scrapBacklinks(QString url, QString backlinkService, bool firstPass)
{
    QString hostname,scheme,tld,passed_url;
    QRegExp hashttp;

    scraper->startAt    = 0;
    scraper->interval   = (config->readGeneralConfig("Global","pthreads").toInt()) ? config->readGeneralConfig("Global","pthreads").toInt() : QThread::idealThreadCount() * 2;

    passed_url = url;
    hashttp = QRegExp("^https?://");
    if(!url.contains(hashttp))
        url.prepend("http://");
    QUrl host(url);
    if(host.isValid())
    {
        tld = host.topLevelDomain();
        scheme = host.scheme();
        hostname = host.toString().remove(tld).section(".",-1);
        hostname.append(tld);
        hostname.remove(scheme);
        hostname.remove("://");
    }
    else
    {
        blError(tr("Backlinks check fail : your domain name is not valid"));
        scraper->scrapButtonEnabled();
        return;
    }

    if(firstPass == true)
    scraper->appendLog(tr("Backlinks check launched for url : %1").arg(url));

    if (backlinkService == "Ahrefs")
    {
        QUrl blQurl(_apisUrls.value("ahrefs"));
        QUrlQuery blQuery;
        blQuery.addQueryItem("target",passed_url);
        blQuery.addQueryItem("mode",ahrefsBacklinksMode);
        blQuery.addQueryItem("output","xml");
        blQuery.addQueryItem("from","metrics");
        blQuery.addQueryItem("select","backlinks");
        blQuery.addQueryItem("token",config->getConfigurationValue("ahrefsApiKey"));
        blQurl.setQuery(blQuery);
        emit createHttpRequest(blQurl.toString(),"standard",AHREFS_BL_COUNT, (QStringList() <<  passed_url));

    }
    else if(backlinkService == "Majestic" || backlinkService == "SEObserver")
    {
        QUrl blQurl;
        QUrlQuery blQuery;

        if(backlinkService == "Majestic")
        {
            blQurl.setUrl(_apisUrls.value("majestic"));
            blQuery.addQueryItem("accesstoken",config->getConfigurationValue("majesticAccessToken"));
            blQuery.addQueryItem("privatekey",config->readGeneralConfig("ExternalPrivateKeys", "Majestic").toString());
        }
        else
        {
            QPair<QString,QByteArray> authFields = this->genSeobserverAuthFields();

            blQurl.setUrl(_apisUrls.value("seobserver"));
            blQuery.addQueryItem("app_api_key",config->getConfigurationValue("seobserverApiKey"));
            blQuery.addQueryItem("auth_signature",authFields.second);
            blQuery.addQueryItem("auth_ts",authFields.first);
            blQuery.addQueryItem("auth_partner","RDDZ");

        }

        blQuery.addQueryItem("datasource",config->getConfigurationValue("majesticIndex"));

        if (!config->getConfigurationValue("majesticRetrievingMethod","0").toInt()) // All backlinks
        {
            blQuery.addQueryItem("cmd","GetBackLinkData");
            blQuery.addQueryItem("Count","50000");
            blQuery.addQueryItem("Mode","1");
            blQuery.addQueryItem("MaxSameSourceURLs","1");
            // Put an option into configuration tab
            blQuery.addQueryItem("MaxSourceURLsPerRefDomain",config->getConfigurationValue("majesticReferring","-1"));
            blQuery.addQueryItem("item",passed_url);
        }
        else if (config->getConfigurationValue("majesticRetrievingMethod","0").toInt()) // New backlinks
        {
            blQuery.addQueryItem("cmd","GetNewLostBackLinks");
            blQuery.addQueryItem("item",passed_url);
            blQuery.addQueryItem("Count","1000");

        }
        else
        {
            blQuery.addQueryItem("cmd","GetRefDomains");
            blQuery.addQueryItem("Count","100000");
            blQuery.addQueryItem("item0",passed_url);


        }
         blQurl.setQuery(blQuery);

        emit createHttpRequest(blQurl.toString(),"standard",MAJESTIC_BACKLINKS, (QStringList() <<  passed_url));
    }
    else if(backlinkService == "Moz")
    {
        uint expireTime;
        QString mozAccessId,mozSecretKey,stringToSign,urlSafeSignature;
        QByteArray binarySignature;

        backlinksMutex.lock();
        QDateTime currentDateTime = QDateTime::currentDateTime();
        maxBlsPerRequest = 50;
        mozAccessId = config->getConfigurationValue("mozAccessId");
        mozSecretKey = config->getConfigurationValue("mozSecretKey");
        expireTime = currentDateTime.toTime_t() + 300;
        stringToSign = mozAccessId+"\n"+QString::number(expireTime);
        binarySignature =  QMessageAuthenticationCode::hash(stringToSign.toUtf8(), mozSecretKey.toUtf8(), QCryptographicHash::Sha1);
        urlSafeSignature = QUrl::toPercentEncoding(binarySignature.toBase64());
        backlinksMutex.unlock();


        QUrl blQurl(_apisUrls.value("moz") + "linkscape/links/"+QUrl::toPercentEncoding(url));
        QUrlQuery blQuery;
        blQuery.addQueryItem("Offset",QString::number(blStartOffset));
//        blQuery.addQueryItem("Limit","50");
        blQuery.addQueryItem("Limit",QString::number(maxBlsPerRequest));
        blQuery.addQueryItem("Scope",config->getConfigurationValue("mozBacklinksMode"));
        blQuery.addQueryItem("Filter","external+equity");
        blQuery.addQueryItem("SourceCols","103616102436");
        blQuery.addQueryItem("TargetCols","4");
        if(config->getConfigurationValue("mozBacklinksMode").startsWith("page_"))
            blQuery.addQueryItem("Sort","page_authority");
        else
             blQuery.addQueryItem("Sort","domain_authority");
        blQuery.addQueryItem("LinkCols","4");
        blQuery.addQueryItem("AccessID",mozAccessId);
        blQuery.addQueryItem("Expires",QString::number(expireTime));
        blQuery.addQueryItem("Signature",urlSafeSignature);
        blQurl.setQuery(blQuery);

        scraper->appendLog("[Moz URL] : "+blQurl.toString());
        emit createHttpRequest(blQurl.toString(),"standard",MOZ_BACKLINKS,(QStringList() <<  passed_url));
        return;
    }
    else
    {
        blError(tr("Invalid backlinks service. Please check one in Advanced Tab or use a custom Search Engine"));
        scraper->scrapButtonEnabled();
    }
}

void Backlinks::getAhrefsBacklinksCountForUrl(QList<QVariant> results)
{
    int i;

    for(i=0;i<results.size();i++)
    {
    QUrl blQurl(_apisUrls.value("ahrefs") + "get_backlinks_count.php");
    QUrlQuery blQuery;
    blQuery.addQueryItem("target",results.at(i).toString());
    blQuery.addQueryItem("mode","exact");
    blQuery.addQueryItem("output","xml");
    blQuery.addQueryItem("AhrefsKey",config->getConfigurationValue("ahrefsApiKey"));
    blQurl.setQuery(blQuery);
    emit createHttpRequest(blQurl.toString(),"standard",AHREFS_BL_NB_FOR_ITEM, (QStringList() <<  results.at(i).toString()));
    }

}

void Backlinks::getAhrefBacklinksNbPerItem()
{
    QNetworkReply   *reply;
    QString         hasError;
    QString         ahrefsBacklinks;

    reply = qobject_cast<QNetworkReply *>(sender());
    QString str(reply->readAll());

    if(reply)
    {
        int httpstatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

        QtConcurrent::run(this, &Backlinks::getAhrefsBacklinksProcessResult, str, reply->property("realUrl").toString(), httpstatus);


        scraper->disconnectReply(reply, AHREFS_BL_NB_FOR_ITEM);
        reply->deleteLater();
    }
}

void Backlinks::getAhrefsBacklinksProcessResult(QString str, QString url, int statusCode)
{
    QMap<int,QList<QVariant> > results;
    QString hasError, ahrefsBacklinks;

    backlinksMutex.lock();
    ahrefsBlCounter++;
    backlinksMutex.unlock();

    if(statusCode != 200)
    {
        blError("[ Ahrefs ] : Ahrefs.com server error");
        scraper->scrapButtonEnabled();
    }
    else
    {
        hasError  = scraper->getXpath(str,"//error/string()","").at(0);
        if(hasError.isEmpty())
        {
            qlonglong backlinksNb;
            ahrefsBacklinks = scraper->getXpath(str, "/AhrefsApiResponse/Result/Backlinks/string()","").at(0);
            backlinksNb = ahrefsBacklinks.trimmed().toInt();
            emit updateBacklinks(url, backlinksNb);
        }
        else
        {
            blError("[ Ahrefs ] "+QString(hasError));
        }
    }

    if(ahrefsBlCounter == blsCount && ahrefsMotherFucker == true)
    {
        results.clear();
        emit addFields(results, 0);
    }
}


void Backlinks::getAhrefsBacklinksCount()
{
    QNetworkReply   *reply;
    QString         mode;
    QString         hasError;
    QString         ahrefsBacklinks;

    reply = qobject_cast<QNetworkReply *>(sender());
    QString str(reply->readAll());
    int httpstatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    blsCount = 0;
    blsTotal = 0;

   emit scraper->deleteReplyFromQhash(reply->property("realUrl").toString());

    if(httpstatus != 200)
    {
        blError("[ Ahrefs ] : Ahrefs.com server error");
        scraper->scrapButtonEnabled();
    }
    else
    {
        hasError  = scraper->getXpath(str,"//error/string()","").at(0);
        if(hasError.isEmpty())
        {
            ahrefsBacklinks = scraper->getXpath(str, "/AhrefsApiResponse/metrics/Backlinks","").at(0);
            blsTotal = ahrefsBacklinks.trimmed().toInt();

            if(blsTotal!=0)
            {
                mode = config->getConfigurationValue("backlinksCheckMode");

                QUrl blQurl(_apisUrls.value("ahrefs"));
                QUrlQuery blQuery;
                blQuery.addQueryItem("target",reply->property("realUrl").toString());
                blQuery.addQueryItem("mode",mode);
                blQuery.addQueryItem("from","backlinks");
                blQuery.addQueryItem("select",ahrefsSelectParam);
                blQuery.addQueryItem("output","json");
                blQuery.addQueryItem("token",config->getConfigurationValue("ahrefsApiKey"));
                blQurl.setQuery(blQuery);

                emit createHttpRequest(blQurl.toString(),"standard",AHREFS_BACKLINKS,(QStringList() <<  reply->property("realUrl").toString()));
            }
            else
            {
                blError("[ Ahrefs ] No backlinks found");
                scraper->scrapButtonEnabled();
            }
        }
        else
        {
            blError("[ Ahrefs ] "+QString(hasError));
            scraper->scrapButtonEnabled();
        }
    }

    if(reply)
    {
        scraper->disconnectReply(reply, AHREFS_BL_COUNT);
        reply->deleteLater();
    }
}

void Backlinks::getMajesticBacklinksProcessResult(QByteArray str, QString apiCallUrl, QString currentUrl, int statusCode, int backlinkIndex)
{
    QMap<int,QList<QVariant> > results;
    QJsonDocument jsonDoc;
    QJsonObject jsonObj;
    QJsonArray dataValue;
    QString majesticCode,errorMsg, fullErrorMsg;
    int blsLeft;
    qlonglong majesticBacklinksNb;
    QString url;
    int currentStartAt,i;

    currentStartAt = this->mappingStartAt.value(currentUrl);
    majesticBacklinksNb = -1;
    blsLeft = 0;
    i = 0;

    if(backlinkIndex == -1)
        return;

    if(statusCode > 200)
    {
        blError("[ Majestic ] : Majesticseo.com server error");
    }
    else
    {
        jsonDoc = QJsonDocument::fromJson(str);
        jsonObj = jsonDoc.object();
        majesticCode = jsonObj.value("ResponseCode").toString();
        dataValue = jsonObj.value("Data").toArray();


        if(majesticCode == "OK")
        {
            if(dataValue.size())
            {
                backlinksMutex.lock();
                blsLeft = mappingBlCount.value(currentUrl);

                for(i=0;i<dataValue.size();i++)
                {
                url = mappingUrlBls.value(currentUrl).at((backlinkIndex*majesticMaxItemPerQuery)+i).toString();
                if(!url.isEmpty())
                {
                if(dataValue.at(i).toObject().value("Status").toString() == "Found")
                    majesticBacklinksNb = dataValue.at(i).toObject().value("ExtBackLinks").toVariant().toLongLong();
                emit updateBacklinks(url, majesticBacklinksNb);
                }
                else
                    qDebug() << "Url is empty :::: " << fullMappingUrl.value(currentUrl).value(backlinkIndex) << backlinkIndex;
                 blsLeft--;
                }

                currentStartAt = (backlinkIndex*this->majesticMaxItemPerQuery) + i;
                mappingBlCount.insert(currentUrl,blsLeft);
                backlinksMutex.unlock();
            }
            else
                qDebug() << "Datavalue is empty >>>>> " << apiCallUrl;
            }
        else
        {
            qDebug() << "Majestic code is not OK" << backlinkIndex ;
            blError("[Majestic SEO] "+QString(errorMsg)+"  "+QString(fullErrorMsg));
        }

    }

    if(blsLeft == 0)
    {
        results.clear();
        this->urlsLeft--;
                emit this->scraper->updateTaskProgressBar(this->urlToCheck.size()-urlsLeft,urlToCheck.size(),tr("Retrieving backlinks ... "));
        this->scraper->appendLog(tr("Backlinks check finished for url : %1").arg(currentUrl) , "success");
        if(urlsLeft)
        {
            if(this->blsCurrentThread < urlToCheck.size())
            {
                this->blGlobalIndex = 0;
                backlinksMutex.lock();
                ++blsCurrentThread;
                backlinksMutex.unlock();
                this->processBacklinksUrl("Majestic");
            }
            emit addFields(results,1);
        }
        else
        emit addFields(results, 0);
        return;
    }
    else if(blsLeft >= 0)
    {

        backlinksMutex.lock();
        ++currentStartAt;
        mappingStartAt.insert(currentUrl,currentStartAt);
        backlinksMutex.unlock();
    }
}


void Backlinks::getMajesticBacklinks()
{
    QNetworkReply   *reply;

    reply = qobject_cast<QNetworkReply *>(sender());

    emit scraper->deleteReplyFromQhash(reply->property("realUrl").toString());

    if(reply)
    {
        QtConcurrent::run(this, &Backlinks::scrapBacklinksProcessResults, reply->readAll(), QString("Majestic"), reply->property("realUrl").toString());
        scraper->disconnectReply(reply, MAJESTIC_BACKLINKS);
        reply->deleteLater();
        reply->manager()->deleteLater();
    }
}

void Backlinks::getAhrefsBacklinks()
{
    QNetworkReply   *reply;

    reply = qobject_cast<QNetworkReply *>(sender());

    if(reply)
    {
        QtConcurrent::run(this, &Backlinks::scrapBacklinksProcessResults, reply->readAll(), QString("Ahrefs"), reply->property("realUrl").toString());

        scraper->disconnectReply(reply, AHREFS_BACKLINKS);
        reply->deleteLater();
        reply->manager()->deleteLater();
    }
}

void Backlinks::getMozBacklinks()
{
    QNetworkReply   *reply;

    reply = qobject_cast<QNetworkReply *>(sender());

    if(reply)
    {
        QtConcurrent::run(this, &Backlinks::scrapBacklinksProcessResults, reply->readAll(), QString("Moz"), reply->property("realUrl").toString());

        scraper->disconnectReply(reply, MOZ_BACKLINKS);
        reply->deleteLater();
        reply->manager()->deleteLater();
    }
}


void Backlinks::scrapBacklinksProcessResults(QByteArray source, QString blService, QString url)
{
    QJsonDocument jsonDoc;
    QJsonObject jsonObj;
    QJsonArray dataValue;
    QList<QVariant> blUrls, blAnchors,blNb, currentWorkingUrl;
    int i;

    QMap<int,QList<QVariant> > results;

    backlinksMutex.lock();
    results.clear();
    blUrls.clear();
    blAnchors.clear();
    blNb.clear();
    backlinksMutex.unlock();

    if(blService == "Majestic")
    {
        QString majesticCode,errorMsg, fullErrorMsg;
        QString urlchecked = url;

        backlinksMutex.lock();
        jsonDoc = QJsonDocument::fromJson(source);
        jsonObj = jsonDoc.object();
        majesticCode = jsonObj.value("Code").toString();
        errorMsg    = jsonObj.value("ErrorMessage").toString();
        fullErrorMsg  = jsonObj.value("FullError").toString();
        jsonObj = jsonObj.value("DataTables").toObject();
        jsonObj = (config->getConfigurationValue("majesticRetrievingMethod","0").toInt() <= 1) ? jsonObj.value("BackLinks").toObject() : jsonObj.value("Results").toObject();
        dataValue = jsonObj.value("Data").toArray();
        backlinksMutex.unlock();

        if(majesticCode == "OK")
        {
            if(!dataValue.size())
            {
                blError(tr("[Majestic] %1 has no backlinks").arg(urlchecked));
                backlinksMutex.lock();
                urlsLeft--;
                backlinksMutex.unlock();
                emit this->scraper->updateTaskProgressBar(this->urlToCheck.size()-urlsLeft,urlToCheck.size(),tr("Retrieving backlinks ... "));
                if(!urlsLeft)
                    scraper->scrapButtonEnabled();
                return;
            }

//            QStringList citationFlow, trustFlow;
            QList<QVariant> citationFlow, trustFlow, topicalTrustFlowTopic, topicalTrustFlowValue, targetURL, linkType, nofollowFlag, deletedFlag, ipAddr, fid, lsd;
            QHash<int,QString> fullLocalMapping;
            blsTotal = dataValue.size();

            if (!config->getConfigurationValue("majesticRetrievingMethod","0").toInt())
            {

                for (i=0;i < dataValue.size();i++)
                {
                    currentWorkingUrl.append(url);
                    fullLocalMapping.insert(i,dataValue.at(i).toObject().value("SourceURL").toString());
                    blUrls.append(dataValue.at(i).toObject().value("SourceURL").toString());
                    targetURL.append(dataValue.at(i).toObject().value("TargetURL").toString());
                    linkType.append(dataValue.at(i).toObject().value("LinkType").toString());
                    blAnchors.append(dataValue.at(i).toObject().value("AnchorText").toString());
                    citationFlow.append(dataValue.at(i).toObject().value("SourceCitationFlow").toInt());
                    trustFlow.append(dataValue.at(i).toObject().value("SourceTrustFlow").toInt());
                    topicalTrustFlowTopic.append(dataValue.at(i).toObject().value("SourceTopicalTrustFlow_Topic_0").toString());
                    topicalTrustFlowValue.append(dataValue.at(i).toObject().value("SourceTopicalTrustFlow_Value_0").toString().toInt());
                    nofollowFlag.append(dataValue.at(i).toObject().value("FlagNoFollow").toInt());
                    deletedFlag.append(dataValue.at(i).toObject().value("FlagDeleted").toInt());
                    fid.append(dataValue.at(i).toObject().value("FirstIndexedDate").toString());
                    lsd.append(dataValue.at(i).toObject().value("LastSeenDate").toString());
                }

                backlinksMutex.lock();
                results.insert(this->mainWin->getColumnIndexByName("blForCol"),currentWorkingUrl);
                results.insert(this->mainWin->getUrlCol(),blUrls);
                results.insert(this->mainWin->getColumnIndexByName("targetUrlCol"),targetURL);
                results.insert(this->mainWin->getColumnIndexByName("anchorCol"),blAnchors);
                results.insert(this->mainWin->getColumnIndexByName("linkTypeCol"),linkType);
                results.insert(this->mainWin->getColumnIndexByName("nofollowCol"),nofollowFlag);
                results.insert(this->mainWin->getColumnIndexByName("cfCol"),citationFlow);
                results.insert(this->mainWin->getColumnIndexByName("tfCol"),trustFlow);
                results.insert(this->mainWin->getColumnIndexByName("ttftCol"),topicalTrustFlowTopic);
                results.insert(this->mainWin->getColumnIndexByName("ttfvCol"),topicalTrustFlowValue);
                results.insert(this->mainWin->getColumnIndexByName("fidCol"),fid);
                results.insert(this->mainWin->getColumnIndexByName("lsdCol"),lsd);

                results.insert(this->mainWin->getColumnIndexByName("linkDeletedCol"),deletedFlag);
                this->urlsLeft--;
                backlinksMutex.unlock();
            }
            else if (config->getConfigurationValue("majesticRetrievingMethod","0").toInt() == 1)
            {

                for (i=0;i < dataValue.size();i++)
                {
                    currentWorkingUrl.append(url);
                    fullLocalMapping.insert(i,dataValue.at(i).toObject().value("SourceURL").toString());
                    blUrls.append(dataValue.at(i).toObject().value("SourceURL").toString());
                    targetURL.append(dataValue.at(i).toObject().value("TargetURL").toString());
                    linkType.append(dataValue.at(i).toObject().value("LinkType").toString());
                    blAnchors.append(dataValue.at(i).toObject().value("AnchorText").toString());
                    citationFlow.append(dataValue.at(i).toObject().value("SourceCitationFlow").toInt());
                    trustFlow.append(dataValue.at(i).toObject().value("SourceTrustFlow").toInt());
                    topicalTrustFlowTopic.append(dataValue.at(i).toObject().value("SourceTopicalTrustFlow_Topic_0").toString());
                    topicalTrustFlowValue.append(dataValue.at(i).toObject().value("SourceTopicalTrustFlow_Value_0").toString().toInt());
                    nofollowFlag.append(dataValue.at(i).toObject().value("FlagNoFollow").toInt());
                    deletedFlag.append(dataValue.at(i).toObject().value("FlagDeleted").toInt());
                    fid.append(dataValue.at(i).toObject().value("FirstIndexedDate").toString());
                    lsd.append(dataValue.at(i).toObject().value("LastSeenDate").toString());
                }


                backlinksMutex.lock();
                results.insert(this->mainWin->getColumnIndexByName("blForCol"),currentWorkingUrl);
                results.insert(this->mainWin->getUrlCol(),blUrls);
                results.insert(this->mainWin->getColumnIndexByName("targetUrlCol"),targetURL);
                results.insert(this->mainWin->getColumnIndexByName("anchorCol"),blAnchors);
                results.insert(this->mainWin->getColumnIndexByName("linkTypeCol"),linkType);
                results.insert(this->mainWin->getColumnIndexByName("nofollowCol"),nofollowFlag);
                results.insert(this->mainWin->getColumnIndexByName("cfCol"),citationFlow);
                results.insert(this->mainWin->getColumnIndexByName("tfCol"),trustFlow);
                results.insert(this->mainWin->getColumnIndexByName("ttftCol"),topicalTrustFlowTopic);
                results.insert(this->mainWin->getColumnIndexByName("ttfvCol"),topicalTrustFlowValue);
                results.insert(this->mainWin->getColumnIndexByName("fidCol"),fid);
                results.insert(this->mainWin->getColumnIndexByName("lsdCol"),lsd);
                results.insert(this->mainWin->getColumnIndexByName("linkDeletedCol"),deletedFlag);
                this->urlsLeft--;
                backlinksMutex.unlock();


            }
            else
            {
                for (i=0;i < dataValue.size();i++)
                {
                    currentWorkingUrl.append(url);
                    if (config->getConfigurationValue("majesticRetrievingMethod","0").toInt() == 2)
                        blUrls.append(dataValue.at(i).toObject().value("Domain").toString().prepend("http://"));
                    else
                        blUrls.append(dataValue.at(i).toObject().value("Domain").toString().prepend("http://www."));
                    citationFlow.append(dataValue.at(i).toObject().value("CitationFlow").toInt());
                    trustFlow.append(dataValue.at(i).toObject().value("TrustFlow").toInt());
                    topicalTrustFlowTopic.append(dataValue.at(i).toObject().value("TopicalTrustFlow_Topic_0").toString());
                    topicalTrustFlowValue.append(dataValue.at(i).toObject().value("TopicalTrustFlow_Value_0").toString().toInt());
                    ipAddr.append(dataValue.at(i).toObject().value("IP").toString());
                }



                backlinksMutex.lock();
                results.insert(this->mainWin->getColumnIndexByName("blForCol"),currentWorkingUrl);
                results.insert(this->mainWin->getUrlCol(),blUrls);
                results.insert(this->mainWin->getColumnIndexByName("cfCol"),citationFlow);
                results.insert(this->mainWin->getColumnIndexByName("tfCol"),trustFlow);
                results.insert(this->mainWin->getColumnIndexByName("ttftCol"),topicalTrustFlowTopic);
                results.insert(this->mainWin->getColumnIndexByName("ttfvCol"),topicalTrustFlowValue);
                results.insert(this->mainWin->getColumnIndexByName("ipCol"),ipAddr);
                this->urlsLeft--;
                backlinksMutex.unlock();

            }



            emit this->scraper->updateTaskProgressBar(this->urlToCheck.size()-urlsLeft,urlToCheck.size(),tr("Retrieving backlinks ... "));
            emit deleteReplyFromQhash(url);
            this->scraper->appendLog(tr("Backlinks check finished for url : %1").arg(url), "success");
            if(urlsLeft)
            {
                if(this->blsCurrentThread < urlToCheck.size())
                {
                    backlinksMutex.lock();
                    ++blsCurrentThread;
                    backlinksMutex.unlock();
                    this->processBacklinksUrl("Majestic");
                }
                emit addFields(results,1);
            }
            else
                emit addFields(results,0);

            return;
        }
        else
            blError("[Majestic SEO] "+QString(errorMsg)+"  "+QString(fullErrorMsg));
    }
    else if (blService == "Ahrefs")
    {
        backlinksMutex.lock();
        jsonDoc = QJsonDocument::fromJson(source);
        jsonObj = jsonDoc.object();
        backlinksMutex.unlock();

        if (jsonObj.value("refpages").isUndefined())
        {
            if (jsonObj.value("error").isUndefined())
                blError("[Ahrefs] Unknown error");
            else
                blError("[Ahrefs] "+jsonObj.value("error").toString());
        }
        else
        {
            backlinksMutex.lock();
            dataValue = jsonObj.value("refpages").toArray();
            backlinksMutex.unlock();

//            QStringList urlRank;
            QList<QVariant> urlRank, domainRating,obl,nofollow,targetUrl,ipFrom,linktype;
            for (i=0;i < dataValue.size();i++)
            {
                currentWorkingUrl.append(url);
                blUrls.append(dataValue.at(i).toObject().value("url_from").toString());
                targetUrl.append(dataValue.at(i).toObject().value("url_to").toString());
                blAnchors.append(dataValue.at(i).toObject().value("anchor").toString());
                linktype.append(dataValue.at(i).toObject().value("link_type").toString());
                nofollow.append(dataValue.at(i).toObject().value("nofollow").toBool());
                urlRank.append(dataValue.at(i).toObject().value("ahrefs_rank").toInt());
                domainRating.append(dataValue.at(i).toObject().value("domain_rating").toInt());
                obl.append(dataValue.at(i).toObject().value("links_external").toInt());
                ipFrom.append(dataValue.at(i).toObject().value("ip_from").toString());
            }

            results.insert(this->mainWin->getColumnIndexByName("blForCol"),currentWorkingUrl);
            results.insert(this->mainWin->getUrlCol(),blUrls);
            results.insert(this->mainWin->getColumnIndexByName("targetUrlCol"), targetUrl);
            results.insert(this->mainWin->getColumnIndexByName("anchorCol"),blAnchors);
            results.insert(this->mainWin->getColumnIndexByName("linkTypeCol"),linktype);
            results.insert(this->mainWin->getColumnIndexByName("nofollowCol"),nofollow);
            results.insert(this->mainWin->getColumnIndexByName("ahrefsUrlRankCol"), urlRank);
            results.insert(this->mainWin->getColumnIndexByName("ahrefsDomainRankCol"), domainRating);
            results.insert(this->mainWin->getColumnIndexByName("oblCol"), obl);
            results.insert(this->mainWin->getColumnIndexByName("ipCol"), ipFrom);



            backlinksMutex.lock();
            blsCount += i;
            backlinksMutex.unlock();


            if(i<maxBlsPerRequest)
                blsCount = blsTotal;

            if(blsTotal > blsCount && i > 0)
            {
                emit addFields(results,1);
                backlinksMutex.lock();

                QUrl blQurl(_apisUrls.value("ahrefs"));
                QUrlQuery blQuery;
                blQuery.addQueryItem("target",url);
                blQuery.addQueryItem("mode",ahrefsBacklinksMode);
                blQuery.addQueryItem("offset",QString::number(blsCount));
                blQuery.addQueryItem("from","backlinks");
                blQuery.addQueryItem("select",ahrefsSelectParam);
                blQuery.addQueryItem("output","json");
                blQuery.addQueryItem("token",config->getConfigurationValue("ahrefsApiKey"));
                blQurl.setQuery(blQuery);

                backlinksMutex.unlock();
                emit createHttpRequest(blQurl.toString(),"standard",AHREFS_BACKLINKS, (QStringList() <<  url));
                return;
            }
            else
            {
                ahrefsMotherFucker = true;
                backlinksMutex.lock();
                urlsLeft--;
                backlinksMutex.unlock();
                emit this->scraper->updateTaskProgressBar(this->urlToCheck.size()-urlsLeft,urlToCheck.size(),tr("Retrieving backlinks ... "));
                this->scraper->appendLog(tr("Backlinks check finished for url : %1").arg(url), "success");
                if(urlsLeft)
                {
                    if(this->blsCurrentThread < urlToCheck.size())
                    {
                        backlinksMutex.lock();
                        ++blsCurrentThread;
                        backlinksMutex.unlock();
                        this->processBacklinksUrl("Ahrefs");
                    }
                    emit addFields(results,1);
                }
                else
                emit addFields(results,0);
                return;
            }
        }
    }
    else if (blService == "Moz")
    {
        QString mozStatus,errorMsg;

        backlinksMutex.lock();
        jsonDoc = QJsonDocument::fromJson(source);
        backlinksMutex.unlock();

        // If Object >> erreur
        if(jsonDoc.isObject())
        {
            backlinksMutex.lock();
            jsonObj = jsonDoc.object();
            mozStatus = jsonObj.value("status").toString();
            errorMsg    = jsonObj.value("error_message").toString();
            backlinksMutex.unlock();

            if(errorMsg.startsWith("This request exceeds the limit allowed by your current plan"))
            {
                scraper->appendLog(tr("[Moz] request limit found. Waiting 12 seconds before retry"),"warning");
#ifdef Q_OS_WIN
                Sleep(12000);
#else
                sleep(12);
#endif
                this->scrapBacklinks(url, "Moz");
                return;
            }
            else
                blError("[Moz] "+errorMsg);
        }
        else if(jsonDoc.isArray()) // Response OK
        {

            backlinksMutex.lock();
            QJsonArray values = jsonDoc.array();
            backlinksMutex.unlock();
            QList<QVariant> mozRank, pa, da, statusCode;

            for (i=0;i<values.size();i++)
            {
                currentWorkingUrl.append(url);
                blUrls.append("http://"+values.at(i).toObject().value("uu").toString());
                blAnchors.append(values.at(i).toObject().value("lt").toString());
                blNb.append(values.at(i).toObject().value("ueid").toVariant());
                mozRank.append(values.at(i).toObject().value("umrp").toDouble());
                pa.append(values.at(i).toObject().value("upa").toDouble());
                da.append(values.at(i).toObject().value("pda").toDouble());
                statusCode.append(values.at(i).toObject().value("us").toInt());
            }
            results.insert(this->mainWin->getColumnIndexByName("blForCol"),currentWorkingUrl);
            results.insert(this->mainWin->getUrlCol(),blUrls);
            results.insert(this->mainWin->getColumnIndexByName("anchorCol"),blAnchors);
            results.insert(this->mainWin->getColumnIndexByName("blCol"),blNb);
            results.insert(this->mainWin->getColumnIndexByName("mozRankCol"),mozRank);
            results.insert(this->mainWin->getColumnIndexByName("paCol"),pa);
            results.insert(this->mainWin->getColumnIndexByName("daCol"),da);
//            if(this->mainWin->getColumnIndexByName("httpCol"))
            results.insert(this->mainWin->getColumnIndexByName("httpCol"),statusCode);


            // If blUrls == maxBlsPerRequest, continue
            if(blUrls.size() == maxBlsPerRequest)
            {
                emit addFields(results,1);
                backlinksMutex.lock();
                blStartOffset += maxBlsPerRequest;
                backlinksMutex.unlock();

                this->scrapBacklinks(url, "Moz");
                return;

            }
            else
            {
                backlinksMutex.lock();
                urlsLeft--;
                // Reset Offset !!
                this->blStartOffset = 0;
                backlinksMutex.unlock();
                emit deleteReplyFromQhash(url);
                emit this->scraper->updateTaskProgressBar(this->urlToCheck.size()-urlsLeft,urlToCheck.size(),tr("Retrieving backlinks ... "));
                this->scraper->appendLog(tr("Backlinks check finished for url : %1").arg(url), "success");
                if(urlsLeft)
                {
                    if(this->blsCurrentThread < urlToCheck.size())
                    {
                        backlinksMutex.lock();
                        ++blsCurrentThread;
                        backlinksMutex.unlock();
                        this->processBacklinksUrl("Moz");
                    }
                    emit addFields(results,1);
                }
                else
                emit addFields(results,0);
                return;

            }

        }
    }
    scraper->scrapButtonEnabled();
}
