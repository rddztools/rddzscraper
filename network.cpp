#include "mainwindow.h"
#include "network.h"
#include <QMessageBox>


Network::Network(Config *globalConfig, Scraper *globalScraper, MainWindow *globalMw)
{
    this->scraper = globalScraper;
    this->config = globalConfig;
    this->mw = globalMw;
    this->proxyError = 0;
    this->aborted = 0;
    this->countRequest = 0;
    this->networkAccessManager = new QNetworkAccessManager;
    this->replies.clear();
    QObject::connect(this, SIGNAL(markDeadProxy(QString,int)), mw, SLOT(markDeadProxy(QString, int)));
    QObject::connect(this, SIGNAL(markGoodProxy(QString)), mw, SLOT(markGoodProxy(QString)));
    QObject::connect(this, SIGNAL(appendLog(QString)), mw, SLOT(appendLog(QString)));
    QObject::connect(this->scraper, SIGNAL(createHttpRequest(QString,QString,int,QStringList,QUrl,QNetworkAccessManager*)),
                     this, SLOT(createHttpRequest(QString,QString,int,QStringList,QUrl,QNetworkAccessManager*)));
    QObject::connect(this->scraper->backlinks,
                     SIGNAL(createHttpRequest(QString,QString,int,QStringList,QUrl)),
                     this,
                     SLOT(createHttpRequest(QString,QString,int,QStringList,QUrl)));
    QObject::connect(scraper, SIGNAL(abortNetwork()), this, SLOT(abortNetwork()));
    QObject::connect(this, SIGNAL(scrapButtonEnabled()), mw, SLOT(scrapButtonEnabled()));
    QObject::connect(this->scraper,
                     SIGNAL(deleteReplyFromQhash(QString)),
                     this,
                     SLOT(deleteReplyFromQhashSlot(QString)));
    QObject::connect(this->scraper->backlinks,
                     SIGNAL(deleteReplyFromQhash(QString)),
                     this,
                     SLOT(deleteReplyFromQhashSlot(QString)));

}

void Network::replyDestroyed()
{
    countRequest--;

    if (countRequest == 0 && this->aborted == 1)
    {
        emit appendLog(tr("Network operations aborted"));
        emit scrapButtonEnabled();
    }
}

void Network::abortNetwork()
{
    this->aborted = 1;
    this->countRequest = 0;

    if(replies.size())
    {
        QMutex locker;
        locker.lock();
        QHash<QString, QNetworkReply *> repliesHash = this->replies;
        QHashIterator<QString, QNetworkReply *> replyHashIterator(repliesHash);

        if(config->getConfigurationValue("minimalLog") == "false")
        emit appendLog(tr("Abort clicked. %1 urls in queue").arg(QString::number(this->replies.size())));

        replyHashIterator.toFront();
        while(replyHashIterator.hasNext())
        {
            replyHashIterator.next();
            if(!replyHashIterator.key().isEmpty())
            {
                if(replyHashIterator.value()->isRunning())
                {
                    QNetworkSession session(replyHashIterator.value()->manager()->configuration());
                    session.stop();
                }
            }

        }
        this->replies.clear();
        locker.unlock();
    }

    emit this->scraper->updateScrapTableLayout();
    emit scrapButtonEnabled();
}

void Network::disconnectScraper()
{
    QObject::disconnect(this->scraper, SIGNAL(createHttpRequest(QString,QString,int,QStringList,QUrl)),
                     this, SLOT(createHttpRequest(QString,QString,int,QStringList,QUrl)));
    QObject::disconnect(this->scraper->backlinks, SIGNAL(createHttpRequest(QString,QString,int,QStringList,QUrl)),
                     this, SLOT(createHttpRequest(QString,QString,int,QStringList,QUrl)));

}

void Network::deleteReplyFromQhashSlot(QString url)
{
    QMutex locker;
    locker.lock();
    this->replies.remove(url);
    locker.unlock();
}

QNetworkAccessManager *Network::getCurrentManager()
{
    return this->networkAccessManager;
}

QNetworkReply *Network::changeProxy(QNetworkReply *r_cur)
{
    QNetworkReply           *r_new;
    QNetworkAccessManager   *m;
    QString logString;


    m = r_cur->manager();
    if (tools.setProxy(m, config->get_proxies(), r_cur->url().toString(),r_cur->property("type").toInt()) != 0) {
        if (this->proxyError == 0)
        {
            this->proxyError = 1;
            logString = tr("Unable to use proxy... [Aborting]");
            emit appendLog(logString);
            scraper->disconnectReply(r_cur, r_cur->property("type").toInt());
            scraper->sendAbortNetwork();
        }
        scraper->disconnectReply(r_cur, r_cur->property("type").toInt());
        if(r_cur)
        r_cur->deleteLater();
        return nullptr;
    }
    logString = tr(">> Proxy changed to [%1:%2]").arg(m->proxy().hostName(),QString::number(m->proxy().port()));
    emit appendLog(logString);
    r_new = m->get(r_cur->request());
    r_new->setProperty("type",QVariant(r_cur->property("type").toInt()));
    r_new->setProperty("url",QVariant(r_cur->property("url").toString()));
    r_new->setProperty("realUrl",QVariant(r_cur->property("realUrl").toString()));
    r_new->setProperty("custom1",QVariant(r_cur->property("custom1").toString()));
    r_new->setProperty("custom2",QVariant(r_cur->property("custom2").toString()));
    r_new->setProperty("captchaId",QVariant(r_cur->property("captchaId").toString()));
    r_new->setProperty("captchaHost",QVariant(r_cur->property("captchaHost").toString()));
    return r_new;
}

void Network::createHttpRequest(QString url, QString ua, int type, QStringList args, QUrl qurl, QNetworkAccessManager *qnam)
{

    QString realUrl, custom1, custom2, captchaId, captchaHost, proxyUsed, captchaImg;
    QNetworkReply           *r;
    QNetworkRequest         request;
    QNetworkAccessManager   *manager;
    if(qnam != nullptr)
        manager = qnam;
    else
        manager = new QNetworkAccessManager(this);


    QByteArray              ua_used = (config->getSpecificUa()!="") ? config->getSpecificUa() : tools.randomUa(config->get_ua(ua));
    QString                 logString;
    QStringList             proxies;
    int                     currentBacklinkIndex, currentGoogleQnamIndex;

    if (aborted == 1)
        return;
    countRequest++;

    realUrl = url;
    // Replace consecutive spaces by only ONE
    url = url.trimmed().simplified();
    // construct non-mandatory arg list
    realUrl = args.size() > 0 ? args[0] : realUrl;
    custom1 = args.size() > 1 ? args[1] : "";
    custom2 = args.size() > 2 ? args[2] : "";
    captchaId = args.size() > 3 ? args[3] : "";
    captchaHost = args.size() > 4 ? args[4] : "";
    proxyUsed = args.size() > 5 ? args[5] : "";
    captchaImg = args.size() > 6 ? args[6] : "";
    currentBacklinkIndex = args.size() > 7 ? args[7].toInt() : -1;
    currentGoogleQnamIndex = args.size() > 8 ? args[8].toInt() : -1;

    //qDebug() << "[Network] Passed proxy_used" << proxyUsed;

    if(type != NETWORK_PROXY)
    {
        proxies = (config->proxy_enable()) ? config->get_proxies() : QStringList();


        if (!proxyUsed.isEmpty())
        {
            proxies.clear();
            proxies.append(proxyUsed);
        }
    }
    else if (type == NETWORK_PROXY)
    {
        tools.clearUrlProxies();
        proxies.clear();
        proxies.append(url);
        if(config->getTestProxiesOnGoogle())
        {
            ua_used = "Mozilla/5.0 (Macintosh; Intel Mac OS X 10_7_3) AppleWebKit/534.53.11 (KHTML, like Gecko) Version/5.1.3 Safari/534.53.10";
            url     = "https://www.google.com/search?q=pouetpouettagada&gws_rd=cr";
        }
        else
            url     = "http://www.bing.com";
    }

           if(type != AHREFS_BACKLINKS && type != AHREFS_BALANCE && type != AHREFS_BL_COUNT && type != AHREFS_BL_NB_FOR_ITEM  && type != MOZ_BACKLINKS && type != MAJESTIC_BACKLINKS && type != MAJESTIC_BALANCE && type != NETWORK_PLATFORM && type != NETWORK_WHOIS)
    {
        if(config->useProxyRotating())
            tools.clearUrlProxies();

        if (tools.setProxy(manager, proxies, url,type) != 0) {
            if (this->proxyError == 0)
            {
                this->proxyError = 1;
                logString = tr("Unable to use proxies ... [Aborting]");
                emit appendLog(logString);
                scraper->sendAbortNetwork();
            }
            return;
        }
    }


    QUrl myqurl(url);

    if (!type) // scrap
    {
        QByteArray encodedUrl = myqurl.toEncoded();
        if (Tools::isGoogleUrl(url))
        {
            if (url.contains(QRegExp("/sorry/")))
                encodedUrl = url.toLatin1();
            else
            {
                encodedUrl = encodedUrl.mid(myqurl.scheme().length()+3);
                encodedUrl.replace("%20%2B","%2B");
                encodedUrl.replace("+","%2B");
                encodedUrl.replace(":","%3A");
                encodedUrl.prepend(myqurl.scheme().toLatin1()+"://");
            }
        }
        request = QNetworkRequest(QUrl::fromEncoded(encodedUrl));
    }
     else if(type == NETWORK_URL_BL || type == NETWORK_PR)
      request = QNetworkRequest(qurl);
    else
        request = QNetworkRequest(myqurl);

    if (!request.url().isValid() && type == NETWORK_HTTPSTATUS)
    {
        QtConcurrent::run(this->scraper, &Scraper::getUrlRespCodeProcessResults, 400, url);
        return;
    }

    // Special case for Chrome and Majestic
    if(ua == "chrome")
     ua_used = "Mozilla/5.0 (X11; Linux i686) AppleWebKit/537.11 (KHTML, like Gecko) Chrome/23.0.1271.97 Safari/537.11";
    request.setRawHeader("User-Agent", ua_used);
    // Only useful for Google Scrap.
    // New headers
    if (type == NETWORK_PROXY)
    {
        request.setRawHeader("Host","www.google.com");
        request.setRawHeader("Accept-Encoding","gzip, deflate");
        request.setRawHeader("Accept-Language","fr,fr-fr;q=0.8,en-us;q=0.5,en;q=0.3");
        request.setRawHeader("Connection","keep-alive");
    }

    // We have to pass the Mashap Key
    if(type == NETWORK_WHOIS)
        request.setRawHeader("X-Mashape-Key",config->getConfigurationValue("mashapApiKey").toUtf8());

    // Testing new QT function
    manager->connectToHost(url);

    r = manager->get(request);
    r->setProperty("type",QVariant(type));
    r->setProperty("url",QVariant(url));
    r->setProperty("userAgent",QVariant(ua_used));
    r->setProperty("realUrl",QVariant(realUrl));
    r->setProperty("custom1",QVariant(custom1));
    r->setProperty("custom2",QVariant(custom2));
    r->setProperty("captchaId",QVariant(captchaId));
    r->setProperty("captchaHost",QVariant(captchaHost));
    r->setProperty("proxy_used",QVariant(proxyUsed));
    r->setProperty("captchaImg",QVariant(captchaImg));
    r->setProperty("backlinkIndex",QVariant(currentBacklinkIndex));
    if(currentGoogleQnamIndex != -1)
    r->setProperty("googleQnamIndex",QVariant(currentGoogleQnamIndex));

    if(!realUrl.isEmpty() && myqurl.isValid())
    {
        this->replies.insert(realUrl,r);
    }

    if(type == NETWORK_SCRAP)
    {
        if(config->getConfigurationValue("minimalLog") == "false")
        {
            if(manager->proxy().type() != QNetworkProxy::NoProxy)
            {
                emit scraper->appendLog("[Proxy : "+manager->proxy().hostName()+":"+QString::number(manager->proxy().port())+"][User-Agent : "+ua_used+"] "+realUrl);
            }
            else {
                emit scraper->appendLog("[No proxy][User-Agent : "+ua_used+"] "+realUrl);
            }
        }
    }

    QObject::connect(scraper, SIGNAL(abortNetwork()), r, SLOT(deleteLater()));
    QObject::connect(r, SIGNAL(error(QNetworkReply::NetworkError)), this, SLOT(networkErrorMsg(QNetworkReply::NetworkError)));

    r->ignoreSslErrors();
    switch (type){
    case NETWORK_SCRAP:
        QObject::connect(r, SIGNAL(finished()),scraper, SLOT(getScrapFinished()));
        new QReplyTimeout(r, config->getConfigurationValue("scraperTimeout").toInt()*1000, scraper);
        break;
    case NETWORK_URL_BL: // Backlinks / url
        QObject::connect(r, SIGNAL(finished()),scraper, SLOT(getBacklinksFinished()));
        new QReplyTimeout(r, config->getConfigurationValue("backlinksTimeout").toInt()*1000, scraper);
        break;
    case NETWORK_OBL: // Outbound Links
        QObject::connect(r, SIGNAL(finished()),scraper, SLOT(getOblFinished()));
        new QReplyTimeout(r, config->getConfigurationValue("oblTimeout").toInt()*1000, scraper);
        break;
    case NETWORK_HTTPSTATUS: // getUrlRespCode
        QObject::connect(r, SIGNAL(metaDataChanged()),scraper, SLOT(getUrlRespCodeFinished()));
        new QReplyTimeout(r, config->getConfigurationValue("statuscodeTimeout").toInt()*1000, scraper);
        break;
    case NETWORK_REDIR: // resolveRedir
        QObject::connect(r, SIGNAL(metaDataChanged()),scraper, SLOT(resolveRedirectionsFinished()));
        new QReplyTimeout(r, config->getConfigurationValue("statuscodeTimeout").toInt()*1000, scraper);
        break;
    case NETWORK_PR: // Pagerank
        QObject::connect(r, SIGNAL(finished()),scraper, SLOT(getPagerankFinished()));
        new QReplyTimeout(r, config->getConfigurationValue("pagerankTimeout").toInt()*1000, scraper);
        break;
    case NETWORK_DOFOLLOW: // checkDofollowNofollow
        QObject::connect(r, SIGNAL(finished()),scraper, SLOT(checkDofollowNofollowFinished()));
        new QReplyTimeout(r, config->getConfigurationValue("dofollowTimeout").toInt()*1000, scraper);
        break;
    case NETWORK_PROXY: // test proxy
        QObject::connect(r, SIGNAL(metaDataChanged()),this, SLOT(networkProxyOk()));
        new QReplyTimeout(r, config->getConfigurationValue("proxiesTestTimeout").toInt()*1000, scraper);
        break;
    case NETWORK_LINK_ALIVE: // test link alive
        QObject::connect(r, SIGNAL(finished()),scraper, SLOT(getLinkAliveFinished()));
        new QReplyTimeout(r, config->getConfigurationValue("dofollowTimeout").toInt()*1000, scraper);
        break;
    case NETWORK_PLATFORM: // test link alive
        QObject::connect(r, SIGNAL(finished()),scraper, SLOT(getPlatformFinished()));
        new QReplyTimeout(r, config->getConfigurationValue("dofollowTimeout").toInt()*1000, scraper);
        break;
    case NETWORK_WHOIS: // test domain available
        QObject::connect(r, SIGNAL(finished()),scraper, SLOT(getWhoisFinished()));
        new QReplyTimeout(r, config->getConfigurationValue("dofollowTimeout").toInt()*1000, scraper);
        break;
    case AHREFS_BALANCE: // ahrefs balance
        QObject::connect(r, SIGNAL(finished()),scraper->backlinks, SLOT(getAhrefsBalanceFinished()));
        break;
    case AHREFS_BL_COUNT: // ahrefs backlinks count
        QObject::connect(r, SIGNAL(finished()),scraper->backlinks, SLOT(getAhrefsBacklinksCount()));
        break;
    case AHREFS_BACKLINKS: // ahrefs backlinks
        QObject::connect(r, SIGNAL(finished()),scraper->backlinks, SLOT(getAhrefsBacklinks()));
        break;
    case AHREFS_BL_NB_FOR_ITEM: // ahrefs backlinks
        QObject::connect(r, SIGNAL(finished()),scraper->backlinks, SLOT(getAhrefBacklinksNbPerItem()));
        break;
    case MAJESTIC_BALANCE: // majestic balance
        QObject::connect(r, SIGNAL(finished()),scraper->backlinks, SLOT(getMajesticBalanceFinished()));
        break;
    case MAJESTIC_BACKLINKS: // majestic backlinks
        QObject::connect(r, SIGNAL(finished()),scraper->backlinks, SLOT(getMajesticBacklinks()));
        break;
    case MOZ_BACKLINKS: // moz backlinks
         QObject::connect(r, SIGNAL(finished()),scraper->backlinks, SLOT(getMozBacklinks()));
         break;

    default:
        break;
    }
}

void Network::networkProxyOk()
{
    QNetworkReply *reply;
    QNetworkProxy proxy;
    int httpstatus;
    QUrl newurl;

    reply = qobject_cast<QNetworkReply *>(sender());
    httpstatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    proxy = reply->manager()->proxy();

    emit deleteReplyFromQhashSlot(reply->property("realUrl").toString());

//    qDebug() << "Entering slot proxy << " << proxy.hostName() << " : " << proxy.port() << " >> "  << httpstatus << reply->url();

    // Proxy return HTTP Status code
    if(httpstatus == 200)
        emit markGoodProxy(proxy.hostName()+":"+QString::number(proxy.port()));
    else if ((httpstatus == 301 || httpstatus == 301) && config->useProxyRotating())
         emit markGoodProxy(proxy.hostName()+":"+QString::number(proxy.port()));
    else if (httpstatus == 403 && !config->getTestProxiesOnGoogle())
        emit markDeadProxy(proxy.hostName()+":"+QString::number(proxy.port()),0);
    else if (httpstatus == 403)
        emit markDeadProxy(proxy.hostName()+":"+QString::number(proxy.port()),504);
    else if (httpstatus == 407)
        emit markDeadProxy(proxy.hostName()+":"+QString::number(proxy.port()),105);
    else if(httpstatus == 302)
    {
        newurl = reply->url().resolved(reply->attribute(QNetworkRequest::RedirectionTargetAttribute).toUrl());
        if(config->getTestProxiesOnGoogle() && Tools::isGoogleCaptchaUrl(newurl.toString()))
        emit markDeadProxy(proxy.hostName()+":"+QString::number(proxy.port()),503);
        else
            emit markGoodProxy(proxy.hostName()+":"+QString::number(proxy.port()));
    }
    else if (httpstatus == 500)
        emit markDeadProxy(proxy.hostName()+":"+QString::number(proxy.port()),500);

    QObject::disconnect(reply, SIGNAL(error(QNetworkReply::NetworkError)), this, SLOT(networkErrorMsg(QNetworkReply::NetworkError)));
    if(reply)
    {
        reply->deleteLater();
        reply->manager()->deleteLater();
    }
}

void Network::networkErrorMsg(QNetworkReply::NetworkError err)
{
    QNetworkReply           *r_cur;
    QNetworkReply           *r_new;
    int type, httpStatusCode;
    QNetworkProxy           proxy;
    QString errorLog, originalUrl, currentProxy, replyUa, currentProxyStrDisplay;



    r_cur           = qobject_cast<QNetworkReply*>(sender());
    type            = r_cur->property("type").toInt();
    proxy           = r_cur->manager()->proxy();
    originalUrl     = r_cur->property("realUrl").toString();
    currentProxyStrDisplay = proxy.hostName()+":"+QString::number(proxy.port());
    currentProxy    = (proxy.user().isEmpty() && proxy.password().isEmpty()) ? currentProxyStrDisplay : currentProxyStrDisplay+":"+proxy.user()+":"+proxy.password();
    replyUa         = r_cur->property("userAgent").toString();
    httpStatusCode  = r_cur->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

    this->deleteReplyFromQhashSlot(r_cur->property("realUrl").toString());

    if (aborted == 1)
    {
        if(r_cur)
        {
            scraper->disconnectReply(r_cur, type);
            r_cur->deleteLater();
            r_cur->manager()->deleteLater();
        }
        return;
    }

    if(type == NETWORK_SCRAP)
    {
        QStringList replyList, proxiesAlreadyBanned;
        replyList.append(r_cur->property("realUrl").toString());
        replyList.append(r_cur->property("custom1").toString());
        replyList.append(r_cur->property("custom2").toString());

        // Check if proxies is not already in the QHash for this url
        if(config->proxy_enable() && (httpStatusCode >= 500 || err == QNetworkReply::OperationCanceledError))
        {
                proxiesAlreadyBanned = tools.proxiesBannedDomain.values(originalUrl);
                if(proxiesAlreadyBanned.contains(currentProxy) == false)
                    tools.proxiesBannedDomain.insertMulti(originalUrl,currentProxy);
        }

        if((tools.proxiesBannedDomain.count(originalUrl) == config->get_proxies().size()) && config->proxy_enable() && config->useProxyRotating() == false)
        {
            if(Tools::isGoogleUrl(originalUrl))
            {
                emit scraper->appendLog(tr("All proxies banned :( "),"error");
                scraper->disconnectReply(r_cur,type);
                r_cur->deleteLater();
                r_cur->manager()->deleteLater();

                this->scraper->sendAbortNetwork();
                return;
            }

            QNetworkAccessManager *nullQnam = nullptr;
            QHash<QString, QString> scrapHash;
            scrapHash.insert("output", "NoAppendLog");
            QtConcurrent::run(this->scraper, &Scraper::scrapProcessResults, scrapHash, nullQnam);
             emit scraper->appendLog(tr("All proxies used for url : %1").arg(originalUrl),"error");

        }
        else if(((err == QNetworkReply::OperationCanceledError || config->useProxyRotating()) && config->proxy_enable()))
        {
            // Put this error msg on appendLog ?
            qDebug() << "Resend request for url : " << originalUrl;

            scraper->disconnectReply(r_cur,type);
            r_cur->deleteLater();
            r_cur->manager()->deleteLater();

            this->createHttpRequest(originalUrl,replyUa,type,replyList);
            return;
        }
        else
        {
            QNetworkAccessManager *nullQnam = nullptr;
            QHash<QString, QString> scrapHash;
            scrapHash.insert("url",originalUrl);
            scrapHash.insert("proxy",currentProxy);

             // Proxies banned by Google
            if(Tools::isGoogleUrl(originalUrl) && httpStatusCode == 403)
                scrapHash.insert("output", QString(r_cur->readAll()));
            else
            scrapHash.insert("output", "Error [" + QString::number(r_cur->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt())+"] : "+ originalUrl + tr(" with proxy : ")+ currentProxyStrDisplay);
            QtConcurrent::run(this->scraper, &Scraper::scrapProcessResults, scrapHash, nullQnam);
        }

        if(r_cur)
        {
            scraper->disconnectReply(r_cur,type);
            r_cur->deleteLater();
            r_cur->manager()->deleteLater();
        }

        return;
    }

    if(type == NETWORK_HTTPSTATUS)
    {
        if(r_cur)
        {
            scraper->disconnectReply(r_cur,type);
            r_cur->deleteLater();
            r_cur->manager()->deleteLater();
        }

        httpStatusCode = 503;
        QtConcurrent::run(scraper, &Scraper::getUrlRespCodeProcessResults, httpStatusCode, originalUrl);
        return;
    }

    if(type == NETWORK_REDIR)
    {
        httpStatusCode = 503;
        if(r_cur)
        {
            scraper->disconnectReply(r_cur,type);
            r_cur->deleteLater();
            r_cur->manager()->deleteLater();
        }
        return;
    }

    if(type == NETWORK_OBL)
    {
        if(r_cur)
        {
            scraper->disconnectReply(r_cur,type);
            r_cur->deleteLater();
            r_cur->manager()->deleteLater();
        }
        return;
    }

    if(type == NETWORK_DOFOLLOW)
    {
        if(r_cur)
        {
            scraper->disconnectReply(r_cur,type);
            r_cur->deleteLater();
            r_cur->manager()->deleteLater();
        }
        return;
    }
    if (type == NETWORK_PROXY)
    {
        emit markDeadProxy(proxy.hostName()+":"+QString::number(proxy.port()),104);
        QNetworkSession session(r_cur->manager()->configuration());
        session.stop();
        if(r_cur)
        {
            scraper->disconnectReply(r_cur,type);
            r_cur->deleteLater();
            r_cur->manager()->deleteLater();
        }
        return ;
    }

    if (type == NETWORK_PR)
    {
        if(err == 202)
             emit scraper->appendLog(tr("Unable to retrieve pagerank for : %1 with proxy %2").arg(originalUrl,currentProxyStrDisplay), "error");

        if(r_cur)
        {
            scraper->disconnectReply(r_cur,type);
            r_cur->deleteLater();
            r_cur->manager()->deleteLater();
        }
        return ;
    }

    if (type == NETWORK_URL_BL)
    {
        if(r_cur)
        {
            scraper->disconnectReply(r_cur,type);
            r_cur->deleteLater();
            r_cur->manager()->deleteLater();
        }
        return ;
    }

    if (type == NETWORK_LINK_ALIVE)
    {
        QString status;
        if(httpStatusCode == 200 || !httpStatusCode)
            status = tr("error : timeout");
        else
            status = tr("error : %1").arg(QString::number(httpStatusCode));
        QtConcurrent::run(scraper, &Scraper::getLinkAliveProcessResults, status, originalUrl);
        if(r_cur)
        {
            scraper->disconnectReply(r_cur,type);
            r_cur->deleteLater();
            r_cur->manager()->deleteLater();
        }
        return ;
    }
    if(type == NETWORK_PLATFORM)
    {
        QString status;
            status = tr("error : %1").arg(QString::number(httpStatusCode));
        if(r_cur)
        {
            scraper->disconnectReply(r_cur,type);
            r_cur->deleteLater();
            r_cur->manager()->deleteLater();
        }
        return ;
    }
    if(type == NETWORK_WHOIS)
    {
        QString src;
            src = "";
        QtConcurrent::run(scraper, &Scraper::getWhoisProcessResults, src, originalUrl);
        if(r_cur)
        {
            scraper->disconnectReply(r_cur,type);
            r_cur->deleteLater();
            r_cur->manager()->deleteLater();
        }
        return ;
    }
    else if ( config->proxy_enable() && (type != AHREFS_BACKLINKS && type != AHREFS_BALANCE && type != AHREFS_BL_COUNT && type != AHREFS_BL_NB_FOR_ITEM && type != MOZ_BACKLINKS && type != MAJESTIC_BACKLINKS && type != MAJESTIC_BALANCE && type != NETWORK_WHOIS) )
    {
        // Google Captcha !!
        if (Tools::isGoogleUrl(r_cur->url().toString()) &&
                r_cur->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() == 503)
        {
            r_cur->deleteLater();
            return;
        }

         errorLog = tr("Error %1 : %2 with proxy (%3) : [%4 | %5]").arg(QString::number(type),r_cur->request().url().toString().simplified(),proxy.hostName(),QString::number(err), r_cur->errorString());

        if(err == 99 || err == 4)
        {
            if(r_cur)
            {
                scraper->disconnectReply(r_cur,type);
                r_cur->deleteLater();
                r_cur->manager()->deleteLater();
            }
            scraper->urlList.append(r_cur->request().url().toString());
            return;
        }


        if(type!=NETWORK_PR)
            emit appendLog(errorLog);

        // Don't switch proxy if error is in the range below
        if ((err > 199 && err < 299) || err == 301)
        {
            if(r_cur)
            {
                scraper->disconnectReply(r_cur,type);
                r_cur->deleteLater();
                r_cur->manager()->deleteLater();
            }
            return;
        }

            if ((r_new = changeProxy(r_cur) ) == nullptr)
            {
                if(r_cur)
                {
                    scraper->disconnectReply(r_cur,type);
                    r_cur->deleteLater();
                    r_cur->manager()->deleteLater();
                }
                return;
            }


        if(r_cur)
        {
            scraper->disconnectReply(r_cur,type);
            r_cur->deleteLater();
            r_cur->manager()->deleteLater();
        }
    }
    else
        r_new = r_cur;

    QObject::connect(scraper, SIGNAL(abortNetwork()), this, SLOT(abortNetwork()));
    QObject::connect(scraper, SIGNAL(abortNetwork()), r_new, SLOT(deleteLater()));
    QObject::connect(r_new, SIGNAL(error(QNetworkReply::NetworkError)), this, SLOT(networkErrorMsg(QNetworkReply::NetworkError)));
    switch(type){
    case NETWORK_HTTPSTATUS: // getUrlRespCode
        QObject::connect(r_new, SIGNAL(metaDataChanged()),scraper, SLOT(getUrlRespCodeFinished()));
        new QReplyTimeout(r_new, config->getConfigurationValue("statuscodeTimeout").toInt()*1000, scraper);
        break;
    case NETWORK_URL_BL: // Backlinks per url
        QObject::connect(r_new, SIGNAL(finished()),scraper, SLOT(getBacklinksFinished()));
        break;
    case NETWORK_OBL: // Outbound Links
        QObject::connect(r_new, SIGNAL(finished()),scraper, SLOT(getOblFinished()));
        break;
    case NETWORK_REDIR: // resolveRedir
        QObject::connect(r_new, SIGNAL(metaDataChanged()),scraper, SLOT(resolveRedirectionsFinished()));
        new QReplyTimeout(r_new, config->getConfigurationValue("statuscodeTimeout").toInt()*1000, scraper);
        break;
    case NETWORK_PR: // Pagerank
        QObject::connect(r_new, SIGNAL(finished()),scraper, SLOT(getPagerankFinished()));
        break;
    case NETWORK_DOFOLLOW: // checkDofollowNofollow
        QObject::connect(r_new, SIGNAL(finished()),scraper, SLOT(checkDofollowNofollowFinished()));
        break;
    case AHREFS_BALANCE: // ahrefs balance
        QObject::connect(r_new, SIGNAL(finished()),scraper->backlinks, SLOT(getAhrefsBalanceFinished()));
        break;
    case AHREFS_BL_COUNT: // ahrefs backlinks count
        QObject::connect(r_new, SIGNAL(finished()),scraper->backlinks, SLOT(getAhrefsBacklinksCount()));
        break;
    case AHREFS_BACKLINKS: // ahrefs backlinks
        QObject::connect(r_new, SIGNAL(finished()),scraper->backlinks, SLOT(getAhrefsBacklinks()));
        break;
    case AHREFS_BL_NB_FOR_ITEM: // ahrefs backlinks
        QObject::connect(r_new, SIGNAL(finished()),scraper->backlinks, SLOT(getAhrefBacklinksNbPerItem()));
        break;
    case MAJESTIC_BALANCE: // majestic balance
         QObject::connect(r_new, SIGNAL(finished()),scraper->backlinks, SLOT(getMajesticBalanceFinished()));
        break;
    case MAJESTIC_BACKLINKS: // majestic backlinks
        QObject::connect(r_new, SIGNAL(finished()),scraper->backlinks, SLOT(getMajesticBacklinks()));
        break;
    case MOZ_BACKLINKS: // moz backlinks
         QObject::connect(r_new, SIGNAL(finished()),scraper->backlinks, SLOT(getMozBacklinks()));
         break;
    default:
        break;
    }
}
