#include "worker.h"

Worker::Worker(int timeout, Tools *tools)
{
    _working = false;
    _abort = false;
    _paginationTreated = false;
    _retryOnTimeout = true;
    _threadTimeout = timeout;
    _tools = tools;
    _redirectCodes << 301 << 302 << 307 << 308;
    _tmpStackFile = QApplication::applicationDirPath()+"/tmp";


    this->_url.clear();
    this->_replyProperties.clear();
    this->results.clear();
    _qnam = nullptr;
    xidelProcess = nullptr;


    this->timer = new QTimer(this);
    connect(timer, SIGNAL(timeout()), this, SLOT(threadTimeoutSlot()));
    this->timer->start(timeout*1000);

    qRegisterMetaType< QPair<QByteArray,QByteArray> >("QPair<QByteArray,QByteArray>");
    qRegisterMetaType< QList<QPair<QByteArray,QByteArray> > >("QList<QPair<QByteArray,QByteArray>>");
    qRegisterMetaType< QHash<QString,QVariant> >("QHash<QString,QVariant>");
}

Worker::~Worker()
{
    if(xidelProcess != nullptr)
    {
        QObject::disconnect(xidelProcess, SIGNAL(error(QProcess::ProcessError)), this, SLOT(xidelProcessError(QProcess::ProcessError)));
        QObject::disconnect(this->xidelProcess, SIGNAL(finished(int,QProcess::ExitStatus)),this, SLOT(xidelProcessFinished(int,QProcess::ExitStatus)));
        QObject::disconnect(this->xidelProcess, SIGNAL(started()),this, SLOT(xidelProcessStarted()));

        xidelProcess->close();
        delete xidelProcess;
    }
}

void Worker::initScrapThread(const QString &url, QHash<QString, QVariant> parameters)
{
    _threadMutex.lock();
    _working = true;
    _abort = false;

    QString cleanUrl;
    QString newUrl;

    // We split url if we encounter the get_first function
    if(url.startsWith("get_first("))
    {
        newUrl = url;
        newUrl.replace(QRegularExpression(QRegularExpression::escape("get_first(") + "([^\\)]+)" + QRegularExpression::escape(")")),"\\1");
        cleanUrl = newUrl.section("~~",0,0);

        _url = cleanUrl;
    }
    else if(url.contains("get_first("))
    {
        QString firstMotif;
        QString passedUrl;
        QString extractedParams;
        passedUrl = url;

        QRegularExpression regexGetFirst(QRegularExpression::escape("get_first(") + "([^\\)]+)" + QRegularExpression::escape(")"));
        QRegularExpressionMatch match = regexGetFirst.match(passedUrl);
        if (match.hasMatch()) {
            extractedParams = match.captured(1);
            firstMotif = extractedParams.section("~~",0,0);
        }

        passedUrl.replace(QRegularExpression(QRegularExpression::escape("get_first(") + "([^\\)]+)" + QRegularExpression::escape(")")),firstMotif);

        _url = passedUrl;
    }
    else
        _url = url;
    _replyProperties = parameters;
    _threadMutex.unlock();

    if(url.isEmpty())
        qDebug() << "Couille au init, l'url est vide !!!";


    emit scrapThreadReady();
}




void Worker::threadTimeoutSlot()
{
    if(this->timer->isActive())
        this->timer->stop();

    if(checkForAbort())
    {
        emit finished();
        return;
    }


    if(this->_url.isEmpty())
    {
        emit threadTimeout(this->_url, this->_replyProperties);
        emit finished();
        return;
    }

    if(!_minimalLogs)
    {
        // We set specific debug message for backlinks metrics
        if(_replyProperties.value("operationType").toInt() == NETWORK_URL_BL)
            emit sendMsg("Thread timeout for backlinks metrics" ,"warning");
        else
        {
            if(_replyProperties.value("operationType").toInt() == NETWORK_URL_BL)
                this->_url = _replyProperties.value("initialUrl").toString();
            emit sendMsg("Thread timeout for  "+this->_url+" with proxy : " + _replyProperties.value("currentProxyStr").toString() ,"warning");
        }
    }
    disconnect(timer, SIGNAL(timeout()), this, SLOT(threadTimeoutSlot()));

    // retry on timeout and for scrap only !!
    if(_retryOnTimeout && _replyProperties.value("operationType").toInt() == NETWORK_SCRAP)
    {
        emit sendMsg(tr("Retry on timeout activated. Re-sending request for %1").arg(_url),"warning");
        emit sendRetry(_url, _replyProperties);
        emit finished();
        return;
    }

    emit threadTimeout(this->_url, this->_replyProperties);
    emit finished();
    return;
}

void Worker::createRequest()
{
    int operationType;
    QString strProxy;
    QStringList pickedProxy;

    _threadMutex.lock();
    QString url = _url;
    QHash<QString,QVariant> parameters = this->_replyProperties;
    _minimalLogs = _replyProperties.value("minimalLogs").toBool();
    _useProxyProviderRotation = _replyProperties.value("useProxyProviderRotation").toBool();
    if(parameters.value("proxyType").toString() == "none")
        _useProxyProviderRotation = false;
    htmlEncoded = _replyProperties.value("keepHtmlEncode").toBool();
    _retryOnTimeout = _replyProperties.value("retryOnTimeout").toBool();
    _working = true;
    // For rotating proxies
    if(_useProxyProviderRotation)
        _qnam = new QNetworkAccessManager(this);
    else
        _qnam = (_qnam != nullptr) ? _qnam : new QNetworkAccessManager(this);
    strProxy = _tools->randomProxy(_tools->allProxies,parameters.value("threadUID").toInt());
    pickedProxy = _tools->proxyInfos(strProxy);
    if(this->timer->isActive())
        this->timer->stop();
    this->timer->start(_threadTimeout*1000);
    QUrl myqurl(url);
    _threadMutex.unlock();


    if(checkForAbort())
    {
        emit finished();
        return;
    }

    operationType = parameters.value("operationType").toInt();

    // We update the number of threads already launched, necessary for scrap process
    emit updateThreadsLaunched();

    // If url not valid, return
    if(!myqurl.isValid() || myqurl.isRelative())
    {
        emit sendMsg(tr("Url %1 is not valid").arg(_url),"error");
        this->sendEmptyResults();
        emit finished();
        return;
    }

    QNetworkReply           *reply = nullptr;
    QNetworkRequest         request;
    QNetworkProxy           proxy;

    // We gen proxies here !!!
    if(parameters.value("proxyType").toString() != "none")
    {
        QString proxyString;
        QString proxyUser;
        QString proxyPwd;
        QString proxyAddr;
        int proxyPort;
        // We set proxy only if it wasn't already set
        if(parameters.value("proxyAddr").toString().isEmpty() && operationType != NETWORK_WHOIS)
        {
            if(strProxy.isEmpty() && parameters.value("proxyType").toString() == "default")
            {
                emit sendMsg(tr("All proxies banned !! Aborting current scrap"),"error");
                emit sendAbortSignal();
                emit finished();
                return;
            }

            proxyAddr   = pickedProxy.at(0);
            proxyPort   = pickedProxy.at(1).toInt();
            proxyUser   = pickedProxy.at(2);
            proxyPwd    = pickedProxy.at(3);
        }
        else
        {
            proxyAddr   = _replyProperties.value("proxyAddr").toString();
            proxyPort   = _replyProperties.value("proxyPort").toInt();
            proxyUser   = _replyProperties.value("proxyUser").toString();
            proxyPwd    = _replyProperties.value("proxyPassword").toString();
        }

        // We set parameters
        _replyProperties.insert("proxyAddr",proxyAddr);
        _replyProperties.insert("proxyPort",proxyPort);
        _replyProperties.insert("proxyUser",proxyUser);
        _replyProperties.insert("proxyPassword",proxyPwd);

        proxy.setType(QNetworkProxy::HttpProxy);
        proxy.setHostName(proxyAddr);
        proxy.setPort(static_cast<unsigned short>(proxyPort));
        proxy.setUser(proxyUser);
        proxy.setPassword(proxyPwd);
        proxyString = _replyProperties.value("proxyAddr").toString()+":"+_replyProperties.value ("proxyPort").toString();

        if(operationType == NETWORK_SCRAP)
        {
            if(!_minimalLogs)
                emit sendMsg("[Proxy : "+proxyString+"][User-Agent : "+parameters.value("userAgent").toString()+"] "+this->_url,"default");
        }

        _replyProperties.insert("currentProxyStr",strProxy);

    }
    else
    {
        proxy.setType(QNetworkProxy::NoProxy);
        if(operationType == NETWORK_SCRAP)
        {
            if(!_minimalLogs)
                emit sendMsg("[No proxy][User-Agent : "+parameters.value("userAgent").toString()+"] "+this->_url,"default");
        }
    }

    _qnam->setProxy(proxy);
    _qnam->connectToHost(_url);


    QSslConfiguration conf = request.sslConfiguration();
    conf.setPeerVerifyMode(QSslSocket::VerifyNone);

    if (operationType == NETWORK_SCRAP && !Tools::isGoogleCaptchaUrl(url)) // scrap
    {
        QByteArray encodedUrl = myqurl.toEncoded();
        request = QNetworkRequest(QUrl::fromEncoded(encodedUrl));
    }
    else
        request = QNetworkRequest(myqurl);

    if(url.startsWith("https"))
        request.setSslConfiguration(conf);

    if(operationType == NETWORK_WHOIS)
        request.setRawHeader("X-Mashape-Key",parameters.value("apiKey").toByteArray());

    // We set the headers
    if(!parameters.value("userAgent").toString().isEmpty())
        request.setRawHeader("User-Agent", parameters.value("userAgent").toByteArray());
    if(!parameters.value("host").toString().isEmpty())
        request.setRawHeader("host", parameters.value("host").toByteArray());
    if(!parameters.value("acceptLanguage").toString().isEmpty())
        request.setRawHeader("Accept-Language", parameters.value("acceptLanguage").toByteArray());
    if(!parameters.value("acceptEncoding").toString().isEmpty())
        request.setRawHeader("Accept-Encoding", parameters.value("acceptEncoding").toByteArray());
    if(!parameters.value("contentType").toString().isEmpty())
        request.setRawHeader("Content-Type", parameters.value("contentType").toByteArray());
    if(!parameters.value("referer").toString().isEmpty())
        request.setRawHeader("referer", parameters.value("referer").toByteArray());
    request.setAttribute(QNetworkRequest::CacheSaveControlAttribute, false);

    // TODO, set parameters below in the conf
    request.setRawHeader("Accept","text/html,application/xhtml+xml,application/xml;q=0.9,image/webp,image/apng,*/*;q=0.8");
    request.setRawHeader("Pragma","no-cache");
    request.setRawHeader("DNT","1");
    request.setRawHeader("Upgrade-Insecure-Requests","1");
    request.setRawHeader("Cache-Control","no-cache");


    // For JSON, we set raw-header. Hard-coded now, but it will be great to add a dropdown on "Expert mode" tab
    if(!_replyProperties.value("jsonStructure").toString().isEmpty())
        request.setRawHeader("X-Requested-With", "XMLHttpRequest");

    // Special case for Moz, we use POST request in order to do bulk check
    if(operationType == NETWORK_URL_BL && parameters.value("currentAPI") == "Moz")
    {
        QStringList urlsWithoutSchemes,urlsList;
        int i;

        urlsList = parameters.value("urlsList").toStringList();
        if(!parameters.value("backlinksMethod").toInt())
        {
            for(i=0;i<urlsList.size();i++)
            {
                QUrl currentQurl(urlsList.at(i));
                QString scheme = currentQurl.scheme();
                urlsWithoutSchemes.append(urlsList.at(i).mid(scheme.size()+3));
            }
            urlsList = urlsWithoutSchemes;
        }

        QJsonDocument mozJsonDoc;
        mozJsonDoc.setArray(QJsonArray::fromStringList(urlsList));
        request.setRawHeader("Content-Type", "application/json");

        reply = _qnam->post(request,mozJsonDoc.toJson());
    }
    else if(_replyProperties.value("requestType").toString() == "POST" && !_replyProperties.value("postArray").toString().isEmpty())
    {
        QUrlQuery postData;
        QString postArrayStr = _replyProperties.value("postArray").toString();

        // First we replace customs if they are in the postArray
        postArrayStr.replace("{%custom1%}", parameters.value("custom1").toString());
        postArrayStr.replace("{%custom2%}", parameters.value("custom2").toString());

        QStringList postArray = postArrayStr.split("&");
        foreach(QString postFields, postArray)
        {

            QStringList postField = postFields.split("=");
            if(postField.size() == 2)
            {
                postData.addQueryItem(postField.at(0), postField.at(1));
            }

        }

        if(postData.queryItems().size())
        {
            reply = _qnam->post(request, postData.toString(QUrl::FullyEncoded).toUtf8());
        }
    }
    else
        reply = _qnam->get(request);

    reply->setProperty("operationType",parameters.value("operationType"));
    reply->setProperty("url",QVariant(url));
    reply->setProperty("userAgent",parameters.value("userAgent"));
    reply->setProperty("realUrl",parameters.value("realUrl"));
    reply->setProperty("custom1",parameters.value("custom1"));
    reply->setProperty("custom2",parameters.value("custom2"));
    // We set the XPath queries
    reply->setProperty("xpathContent",parameters.value("xpathContent"));
    reply->setProperty("xpathPagination",parameters.value("xpathPagination"));
    // Proxy property
    reply->setProperty("proxyType",parameters.value("proxyType"));


    switch(operationType){
    case NETWORK_SCRAP:
        QObject::connect(reply, SIGNAL(finished()),this, SLOT(replyFinished()));
        break;
    case NETWORK_HTTPSTATUS:
        QObject::connect(reply, SIGNAL(metaDataChanged()),this, SLOT(getUrlRespCodeFinished()));
        break;
    case NETWORK_REDIR:
        QObject::connect(reply, SIGNAL(metaDataChanged()),this, SLOT(getUrlRespCodeFinished()));
        break;
    case NETWORK_REDIR_STATUS:
        QObject::connect(reply, SIGNAL(metaDataChanged()),this, SLOT(getUrlRespCodeFinished()));
        break;
    default:
        QObject::connect(reply, SIGNAL(finished()),this, SLOT(replyFinished()));
        break;

    }

}

void Worker::getUrlRespCodeFinished()
{
    int             httpstatus;
    int             operationType;
    QNetworkReply   *reply;
    QUrl            newurl;
    QString         newUrlStr;


    this->_threadMutex.lock();
    reply = qobject_cast<QNetworkReply *>(sender());
    httpstatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    operationType = _replyProperties.value("operationType").toInt();
    newurl = reply->url().resolved(reply->attribute(QNetworkRequest::RedirectionTargetAttribute).toUrl());
    this->_threadMutex.unlock();

    reply->deleteLater();

    newUrlStr = "";
    if (operationType == NETWORK_REDIR && (httpstatus >= 300 && httpstatus < 400))
    {
        newUrlStr = newurl.toString().simplified();
        _replyProperties.insert("redirectedUrl",newUrlStr);
        _replyProperties.insert("initialUrl",_url);
        _replyProperties.insert("operationType",NETWORK_REDIR_STATUS);
        this->_threadMutex.lock();
        _url = newUrlStr;
        this->_threadMutex.unlock();
        this->createRequest();
        return;

    }
    else if (operationType == NETWORK_REDIR_STATUS)
        emit sendRedirection(httpstatus,_replyProperties.value("initialUrl").toString(),_url);
    else if (operationType == NETWORK_REDIR)
        emit sendRedirection(httpstatus,_url,_url);
    else // On fait du status code
        emit sendHttpStatusCode(httpstatus, _url);


    emit finished();
    return;
}

void Worker::httpCheckSkipMsg(QString msg)
{
    emit sendMsg(msg, "information");
    this->sendEmptyResults();
    _qnam->deleteLater();
    emit finished();
}

void Worker::httpCheckRetryMsg(QString msg)
{
    emit sendMsg(msg, "information");
    emit sendRetry(_url, _replyProperties);
    emit finished();
}

void Worker::writeContentToLogFile(QString logContent)
{
    QFile file(_replyProperties.value("httpLogFile").toString());
    QTextStream stream( &file );
    _threadMutex.lock();
    if (file.open(QIODevice::Append))
    {
        // We write the file into UTF8 format
        stream.setCodec("UTF-8");
        stream.setGenerateByteOrderMark(true);
        stream << logContent << endl;
        stream.flush();

    }
    file.close();
    _threadMutex.unlock();
}


bool Worker::checkIfHTTPStatusFound(int httpstatus, QString redirectUrl)
{
    bool validateCondition;
    QString logMsg;

    validateCondition = false;

    if(_replyProperties.value("resendHTTP") == 1) // IS
    {
        if(_replyProperties.value("resendHTTPCode").toInt() == httpstatus && _replyProperties.value("resendHTTPCondition").toInt() == 0) // Status code equal
        {
            validateCondition = true;
            if(!_replyProperties.value("resendHTTPAction").toInt())
                logMsg = tr("HTTP Status code %1 found (Server reply with a %2 status code). Re-sending request %3").arg(QString::number(_replyProperties.value("resendHTTPCode").toInt()), QString::number(httpstatus), _url);
            else if(_replyProperties.value("resendHTTPAction").toInt() == 1)
                logMsg = tr("HTTP Status code %1 found (Server reply with a %2 status code). Skip request  %3").arg(QString::number(_replyProperties.value("resendHTTPCode").toInt()), QString::number(httpstatus), _url);
            else if(_replyProperties.value("resendHTTPAction").toInt() == 2)
                logMsg = tr("HTTP Status code %1 found (Server reply with a %2 status code). Logging request %3 into file %4").arg(QString::number(_replyProperties.value("resendHTTPCode").toInt()),QString::number(httpstatus),_url, _replyProperties.value("httpLogFile").toString());
        }
        else  if(_replyProperties.value("resendHTTPCode").toInt() != httpstatus && _replyProperties.value("resendHTTPCondition").toInt() == 1) // Status code different than
        {
            validateCondition = true;
            if(!_replyProperties.value("resendHTTPAction").toInt())
                logMsg = tr("HTTP Status code %1 not found (Server reply with a %2 status code). Re-sending request %3").arg(QString::number(_replyProperties.value("resendHTTPCode").toInt()), QString::number(httpstatus), _url);
            else if(_replyProperties.value("resendHTTPAction").toInt() == 1)
                logMsg = tr("HTTP Status code %1 not found (Server reply with a %2 status code). Skip request  %3").arg(QString::number(_replyProperties.value("resendHTTPCode").toInt()), QString::number(httpstatus), _url);
            else if(_replyProperties.value("resendHTTPAction").toInt() == 2)
                logMsg = tr("HTTP Status code %1 not found (Server reply with a %2 status code). Logging request %3 into file %4").arg(QString::number(_replyProperties.value("resendHTTPCode").toInt()),QString::number(httpstatus),_url, _replyProperties.value("httpLogFile").toString());
        }
        else if(_replyProperties.value("resendHTTPCode").toInt() < httpstatus && _replyProperties.value("resendHTTPCondition").toInt() == 2) // Status code higher
        {
            validateCondition = true;
            if(!_replyProperties.value("resendHTTPAction").toInt())
                logMsg = tr("HTTP Status code %1 is higher than %2. Re-sending request %3").arg(QString::number(httpstatus), QString::number(_replyProperties.value("resendHTTPCode").toInt()), _url);
            else if(_replyProperties.value("resendHTTPAction").toInt() == 1)
                logMsg = tr("HTTP Status code %1 is higher than %2. Skip request %3").arg(QString::number(httpstatus), QString::number(_replyProperties.value("resendHTTPCode").toInt()), _url);
            else if(_replyProperties.value("resendHTTPAction").toInt() == 2)
                logMsg = tr("HTTP Status code %1 is higher than %2. Logging request %3 into file %4").arg(QString::number(httpstatus),QString::number(_replyProperties.value("resendHTTPCode").toInt()),_url,_replyProperties.value("httpLogFile").toString());
        }
        else if(_replyProperties.value("resendHTTPCode").toInt() > httpstatus && _replyProperties.value("resendHTTPCondition").toInt() == 3) // Status code lower
        {
            validateCondition = true;
            if(!_replyProperties.value("resendHTTPAction").toInt())
                logMsg = tr("HTTP Status code %1 is lower than %2. Re-sending request %3").arg(QString::number(_replyProperties.value("resendHTTPCode").toInt()), QString::number(httpstatus), _url);
            else if(_replyProperties.value("resendHTTPAction").toInt() == 1)
                logMsg = tr("HTTP Status code %1 is lower than %2. Skip request %3").arg(QString::number(_replyProperties.value("resendHTTPCode").toInt()), QString::number(httpstatus), _url);
            else if(_replyProperties.value("resendHTTPAction").toInt() == 2)
                logMsg = tr("HTTP Status code %1 is lower than %2. Logging request %3 into file %4").arg(QString::number(_replyProperties.value("resendHTTPCode").toInt()),QString::number(httpstatus),_url,_replyProperties.value("httpLogFile").toString());
        }
    }

    // we check here if a condition is valid
    if(validateCondition == true)
    {
        switch(_replyProperties.value("resendHTTPAction").toInt())
        {
        case 0: // Action is retry
            this->httpCheckRetryMsg(logMsg);
            return true;
        case 1: // Action is continue
            this->httpCheckSkipMsg(logMsg);
            return true;
        case 2: // Action is log
            QString logContent;
            this->httpCheckSkipMsg(logMsg);
            if(_redirectCodes.contains(httpstatus))
                logContent = _url + "\t" + QString::number(httpstatus) + "\t" + redirectUrl;
            else
                logContent = _url + "\t" + QString::number(httpstatus);
            this->writeContentToLogFile(logContent);
            return true;
        }
    }

    return false;
}

void Worker::replyFinished()
{
    QNetworkReply *reply;
    QByteArray replyContent;
    int httpstatus;
    int operationType;
    QNetworkReply::NetworkError replyError;
    QString replyErrorString;
    QUrl newurl;

    this->timer->stop();
    reply = qobject_cast<QNetworkReply*>(sender());
    httpstatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    replyContent = reply->readAll();
    // We check the gzip format ;)
    if(replyContent.mid(0,2).toHex() == "1f8b")
    {
        emit sendMsg(tr("Gzip content received for %1").arg(_url),"information");
        replyContent = gUncompress(replyContent);
    }

    replyError = reply->error();
    replyErrorString = reply->errorString();
    operationType = _replyProperties.value("operationType").toInt();
    newurl = reply->url().resolved(reply->attribute(QNetworkRequest::RedirectionTargetAttribute).toUrl());


    // New from 1.7.5 : we check the http status code value
    if(this->checkIfHTTPStatusFound(httpstatus, newurl.toString()))
    {
        reply->deleteLater();
        return;
    }

    // If source code is empty
    if(replyContent.isEmpty() && replyError)
    {
        // If we use proxy provider with auto-rotating IP and have an error : Error communicating with HTTP proxy
        if(_useProxyProviderRotation && (replyError == 99 || replyError == 2))
        {
            emit sendMsg(replyErrorString + " but autoRotate IP activated. Re-sending : " + _url + " " + _replyProperties.value("initialUrl").toString(), "information");
            emit sendRetry(_url, _replyProperties);
            emit finished();
            return;
        }

        // For debug purpose
        emit sendMsg(replyErrorString + " [HTTP Status code : " + QString::number(httpstatus)+" with error "+QString::number(replyError)+"] " +tr("for url : ")+_url, "error");

        if(operationType == NETWORK_SCRAP)
        {
            this->sendEmptyResults();
            reply->deleteLater();
            _qnam->deleteLater();
            emit finished();
            return;
        }
    }


    // Redirections (all except API !!)

    if(httpstatus >= 300 && httpstatus < 400)
    {
        _replyProperties.insert("redirFound",true);

        if(Tools::isGoogleUrl(this->_url) && Tools::isGoogleCaptchaUrl(newurl.toString()))
        {
            if(!_minimalLogs)
                emit sendMsg(tr("Google Captcha found for query : %1").arg(this->_url), "success");

            // If we use a proxy provider with auto-rotating IP, we retry
            if(_useProxyProviderRotation)
            {
                this->createRequest();
                return;
            }

            // Here we check if Ici on devra check si il y a une api de captcha define, sinon on switch de proxies si on peut
            // Pas d'API pour les captchas, on switch de proxy si on le peut
            _tools->setBannedProxy(_replyProperties.value("currentProxyStr","").toString(),true);
            _replyProperties.insert("proxyAddr","");
            this->createRequest();

            return;
        }
        else // Redirection (not Google captcha), we retry with the new url
        {
            if(!_minimalLogs)
                emit sendMsg(tr("Redirection found for url : %1 => %2").arg(this->_url,newurl.toString()), "information");

            // We delete reply and qnam
            reply->deleteLater();

            // Protection against redirections infinite loop, we check URL and http status code (for example 301 then 302 then 200)
            QHash<QString, QVariant> previousRedirections = _replyProperties.value("previousRedirections","").toHash();
            if(previousRedirections.contains(newurl.toString()) && previousRedirections.value(newurl.toString(),-1).toInt() == httpstatus)
            {
                emit sendMsg(tr("Inifinite redirection loop detect for url : %1").arg(this->_url), "warning");
                // If we scrape, we send an empty result
                if(operationType == NETWORK_SCRAP)
                {
                    this->sendEmptyResults();
                    emit finished();
                    return;
                }
            }
            else
            {
                previousRedirections.insert(newurl.toString(), httpstatus);
                _replyProperties.insert("previousRedirections",previousRedirections);
                // We use the same qnam, for 301/302 on the same page
                _url = newurl.toString();
                this->createRequest();
                return;
            }

        }

    }

    // For scrap only, we retry if we found string in source code << be careful !!!
    if(_replyProperties.value("resendReqStrNotContains").toInt() && _useProxyProviderRotation)
    {
        if(replyContent.contains(_replyProperties.value("resendReqStr").toByteArray()) && _replyProperties.value("resendReqStrNotContains").toInt() == 1)
        {
            switch(_replyProperties.value("resendReqStrAction").toInt())
            {
            case 0: // Action is retry
                this->httpCheckRetryMsg(tr("String \"%1\" found in source code. Re-sending request %2").arg(_replyProperties.value("resendReqStr").toString(),_url));
                break;
            case 1: // Action is continue
                this->httpCheckSkipMsg(tr("String \"%1\" found in source code. Skip request %2").arg(_replyProperties.value("resendReqStr").toString(),_url));
                break;
            }
            return;
        }
        else if (!replyContent.contains(_replyProperties.value("resendReqStr").toByteArray()) && _replyProperties.value("resendReqStrNotContains").toInt() == 2)
        {
            switch(_replyProperties.value("resendReqStrAction").toInt())
            {
            case 0: // Action is retry
                this->httpCheckRetryMsg(tr("String \"%1\" not found in source code. Re-sending request %2").arg(_replyProperties.value("resendReqStr").toString(),_url));
                break;
            case 1: // Action is continue
                this->httpCheckSkipMsg(tr("String \"%1\" not found in source code. Skip request %2").arg(_replyProperties.value("resendReqStr").toString(),_url));
                break;
            }
            return;
        }
    }

    // Here we check for operationType in order to do correct actions
    if(operationType == NETWORK_OBL)
    {
        _replyProperties.insert("xpathContent","count(//a[not(starts-with(@href,\""+Tools::getDomain(_url,false)+"\")) and (starts-with(@href,\"http://\") or starts-with(@href,\"https://\"))])");
        _replyProperties.insert("xpathPagination","");
        this->getXpath(_url,replyContent,_replyProperties);
        return;
    }
    else if(operationType == NETWORK_LINK_ALIVE)
    {
        if(replyContent.isEmpty())
        {
            emit sendLinkAlive("error : " + QString::number(httpstatus),_url);
            emit finished();
            return;
        }

        _replyProperties.insert("xpathContent","//a/@href[contains(.,\""+_replyProperties.value("searchForUrl").toString()+"\")]");
        _replyProperties.insert("xpathPagination","");
        this->getXpath(_url,replyContent,_replyProperties);
        return;
    }
    else if(operationType == NETWORK_DOFOLLOW)
    {
        if(replyContent.isEmpty())
        {
            emit sendDofollow(-2, _replyProperties.value("initialUrl").toString());
            emit finished();
            return;
        }

        // Check for a meta nofollow on sourceCode
        _replyProperties.insert("xpathContent","concat(count(//a[not(starts-with(@href,\""+Tools::getDomain(_url,false)+"\")) and (starts-with(@href,\"http://\") or starts-with(@href,\"https://\"))]),\",\",count(//a[not(starts-with(@href,\""+Tools::getDomain(_url,false)+"\")) and @rel=\"nofollow\" and (starts-with(@href,\"http://\") or starts-with(@href,\"https://\"))]))");
        _replyProperties.insert("xpathPagination","");
        this->getXpath(_url,replyContent,_replyProperties);
        return;
    }
    else if(operationType == NETWORK_PLATFORM)
    {
        QList<QPair<QByteArray, QByteArray> > headers = reply->rawHeaderPairs();
        QString platformString;
        if(!replyContent.size() || httpstatus > 400)
        {
            platformString.clear();
            platformString = "error : HTTP Status [" + QString::number(httpstatus) + "]";
        }
        else
            platformString = Tools::platformDetector(QString(replyContent),headers);
        emit sendCheckPlatform(platformString, _replyProperties.value("initialUrl").toString());
        emit finished();
        return;
    }
    else if(operationType == NETWORK_WHOIS)
    {
        QJsonDocument jsonDoc;
        QJsonObject jsonObj;
        QString isAvailable;

        if(httpstatus != 200)
            isAvailable = "API Server error "+QString::number(httpstatus);

        if(_replyProperties.value("currentAPI").toString() == "Dynadot")
        {
            QString replyContentStr(replyContent);
            QStringList whoisResponses = replyContentStr.split("\n");
            if(!replyContentStr.isEmpty())
            {
                if(whoisResponses.at(0) == "ok,")
                {
                    int whoisLoop;
                    for (whoisLoop = 0; whoisLoop < whoisResponses.size() - 1;whoisLoop++)
                    {
                        if(!whoisResponses.at(whoisLoop).isEmpty())
                        {
                            isAvailable = (whoisResponses.at(whoisLoop).section(",",3).right(1) == ",") ? whoisResponses.at(whoisLoop).section(",",3,3) : whoisResponses.at(whoisLoop).section(",",3);
                            emit sendWhoisResponse(isAvailable,  whoisResponses.at(whoisLoop).section(",",1,1));
                        }
                    }

                }
                else // error, account banned, ...
                {
                    emit sendMsg(whoisResponses.at(0),"error");
                    // Check for the 10min ban, do we wait or not ... ?
                    emit sendAbortSignal();
                }
            }
            emit finished();
            return;
        }
        else
        {
            if(!replyContent.isEmpty() && httpstatus == 200)
            {
                jsonDoc = QJsonDocument::fromJson(replyContent);
                jsonObj = jsonDoc.object();

                if(jsonObj.value("status") == QJsonValue::Undefined)
                    isAvailable = "API error";
                else
                {
                    /* For Mashape service */
                    if(jsonObj.value("status").toArray().size())
                    {
                        isAvailable = (jsonObj.value("status").toArray().at(0).toObject().value("summary").toString() == "inactive") ? "true" : "false";
                    }
                }
            }

            if(replyError == QNetworkReply::ContentNotFoundError)
            {
                emit sendMsg(tr("Error while retrieving domain availability for domain : %1").arg(_replyProperties.value("initialUrl").toString()),"error");
                emit sendWhoisResponse("Error", _replyProperties.value("initialUrl").toString());
            }
            else
                emit sendWhoisResponse(isAvailable, _replyProperties.value("initialUrl").toString());
            emit finished();

            return;
        }
    }
    else if(operationType == NETWORK_URL_BL)
    {
        // If Moz and error 299
        this->getBacklinks(replyContent,_replyProperties.value("initialUrl").toString());
        return;
    }

    reply->deleteLater();

    // We quit if user abort
    if(checkForAbort())
    {
        emit finished();
        return;
    }


    if(replyError)
    {
        if(!_minimalLogs)
            emit sendMsg(tr("HTTP Error [proxy : %1] : %2").arg(_replyProperties.value("currentProxyStr","").toString(),replyErrorString) ,"error");
        this->sendEmptyResults();
        emit finished();
        return;
    }
    else
    {
        _replyProperties.insert("httpStatusCode",httpstatus);
        _replyProperties.insert("replyError",replyError);

        this->getXpath(this->_url, replyContent, _replyProperties);
        return;
    }
}


void Worker::xidelProcessFinished(int exitCode, QProcess::ExitStatus qExitStatus)
{
    QStringList xpathResult;
    QJsonDocument jsonDoc, errJsonDoc;
    QTextDocument txtDoc;
    QJsonArray values;
    QStringList processArgs;

    int i, j;
    processArgs = this->xidelProcess->arguments();

    if(qExitStatus == QProcess::CrashExit)
    {
        emit sendMsg(tr("XPath treatment process has crashed "),"error");
        qDebug() << qExitStatus;
        this->disconnectProcessSignals();
        sendEmptyResults();
        emit finished();
        return;
    }

    // For processes with a large amount of threads, we re-launch thread (TODO : remove this partial patch)
    if(exitCode > 4)
    {
        qDebug() << xidelProcess->errorString() << _url;
        this->disconnectProcessSignals();
        emit sendRetry(_url,_replyProperties);
        emit finished();
        return;
    }

    this->_threadMutex.lock();
    QByteArray results = this->xidelProcess->readAllStandardOutput();
    QString xpathError = this->xidelProcess->readAllStandardError();
    this->xidelProcess->close();
    this->_threadMutex.unlock();

    this->disconnectProcessSignals();

    xpathError.replace(QRegExp("([\\*]{4}[^\\*]+[\\*]{4}|^$)"),"");

    QJsonParseError *jsonParseError = new QJsonParseError;

    QString strResult(results);
    jsonDoc = QJsonDocument::fromJson(strResult.toUtf8(),jsonParseError);
    errJsonDoc = QJsonDocument::fromJson(xpathError.toUtf8());
    values = jsonDoc.array();

    this->_threadMutex.lock();
    _working = false;
    this->timer->stop();
    this->_threadMutex.unlock();


    // We delete temporary file
    if(_replyProperties.value("operationType").toInt() != NETWORK_SCRAP || (_replyProperties.value("operationType").toInt() == NETWORK_SCRAP && ((!this->xpathPagination.isEmpty() && _paginationTreated) || this->xpathPagination.isEmpty())))
    {
        if(processArgs.size() > 2 && _replyProperties.value("keepStackFiles").toBool() == false)
        {
            QFile tmpFile(processArgs.at(1));
            tmpFile.remove();
        }
    }


    if (errJsonDoc.isObject())
    {
        QJsonObject jsonObj;
        jsonObj = errJsonDoc.object();
        jsonObj = jsonObj.value("_error").toObject();


        emit sendMsg(tr("[XPath Error] Unable to evaluate XPath query : %1 for %2 ").arg(processArgs[3],this->_url), "error");
        emit sendMsg(tr("[XPath Error]  %1").arg(jsonObj.value("_message").toString()), "error");
        this->sendEmptyResults();
        emit finished();
        return;
    }

    if(!values.size())
    {
        emit sendMsg(tr("[JSON Parse Error] : %1 for file %2").arg(jsonParseError->errorString(),processArgs.at(1)), "error");
    }


    for (i=0;i<values.size();i++)
    {
        if(values.at(i).isArray())
        {
            QJsonArray arrayValues = values.at(i).toArray();
            for(j=0;j<arrayValues.size();j++)
            {
                QString strValue;
                if(arrayValues.at(j).type() == QJsonValue::Double)
                    strValue = QString::number(arrayValues.at(j).toDouble());
                else
                 strValue = arrayValues.at(j).toString();
                if(this->htmlEncoded == false && _paginationTreated == false) // Pas de pretty url pour la pagination
                {
                    txtDoc.setHtml(strValue);
                    strValue = txtDoc.toPlainText();
                    strValue = this->prettyFormatUrl(strValue);
                }
                xpathResult.append(strValue);
            }
        }
        else
        {
            if(values.at(i).isString() || values.at(i).isBool())
            {
                QString strValue = values.at(i).toString();
                if(this->htmlEncoded == false && _paginationTreated == false) // Pas de pretty url pour la pagination
                {
                    txtDoc.setHtml(strValue);
                    strValue = txtDoc.toPlainText();
                    strValue = this->prettyFormatUrl(strValue);
                }
                xpathResult.append(strValue);
            }
            else if (values.at(i).isDouble())
                xpathResult.append(QString::number(values.at(i).toDouble()));
        }
    }

    if(_replyProperties.value("operationType").toInt() == NETWORK_OBL)
    {
        double result = (xpathResult.isEmpty()) ? -1 : xpathResult.at(0).toDouble();
        emit sendObl(result,_replyProperties.value("initialUrl").toString());
        emit finished();
        return;
    }
    else if(_replyProperties.value("operationType").toInt() == NETWORK_LINK_ALIVE)
    {
        QString status = (xpathResult.size() && !xpathResult.at(0).isEmpty()) ? "yes" : "no";
        emit sendLinkAlive(status,_replyProperties.value("initialUrl").toString());
        emit finished();
        return;
    }
    else if(_replyProperties.value("operationType").toInt() == NETWORK_DOFOLLOW)
    {
        QStringList dfStringList;
        double result, nofollow, countLinks;
        nofollow = countLinks = 0;
        result = (xpathResult.isEmpty()) ? -1 : 0;

        if(!static_cast<int>(result))
        {
            dfStringList = xpathResult.at(0).split(",");
            if(dfStringList.size() == 2)
            {
                countLinks = dfStringList.at(0).toDouble();
                nofollow = dfStringList.at(1).toDouble();
                result = (!static_cast<int>(countLinks)) ? 0 : ((countLinks-nofollow) * 100) / countLinks;
            }
        }

        emit sendDofollow(result, _replyProperties.value("initialUrl").toString());
        emit finished();
        return;
    }


    if(this->_paginationTreated && xpathResult.size() > 0)
    {
        QString nextPageUrl = xpathResult.at(0);
        QUrl url, nextUrl;
        this->results.insert("xpathPaginationRes",nextPageUrl);

        if(!this->results.value("url","").toString().isEmpty())
            nextUrl = QUrl(this->results.value("url").toString());

        // Here we set the url for the new pagination
        if(!nextPageUrl.isEmpty())
        {
            nextPageUrl.replace("&amp;","&");
            url = QUrl(nextPageUrl);
            if (url.isRelative())
            {
                QString urlPath;

                urlPath = (nextPageUrl.startsWith('/')) ? "" : nextUrl.path().section('/',0,-2)+"/";
                if(nextPageUrl.startsWith('?') || nextPageUrl.startsWith('#'))
                    nextPageUrl = nextUrl.toString()+nextPageUrl;
                else
                    nextPageUrl = QUrl(nextUrl.scheme()+"://"+nextUrl.host().trimmed()).resolved(QUrl(urlPath+nextPageUrl)).toString();
            }
            _url = nextPageUrl;
        }
    }
    // We store the result on first pass
    if(_paginationTreated == false)
    {
        _threadMutex.lock();
        this->results.insert("url",this->_url);
        this->results.insert("xpathContentRes",xpathResult);
        _threadMutex.unlock();


        if(!_minimalLogs)
            emit sendMsg(tr("Found %1 item(s) for url %2").arg(QString::number(xpathResult.size()),this->_url), "default");
        emit updateItemsFound(xpathResult.size());
    }
    // And if we have an expression for pagination, we launch again the getXoath method (the file already exists)
    if(!this->xpathPagination.isEmpty() && _paginationTreated == false)
    {
        this->_paginationTreated = true;

        if(processArgs.size() > 2)
            this->getXpath(this->_url, this->currentSourceCode, this->_replyProperties, processArgs.at(1));
        else
            this->getXpath(this->_url, this->currentSourceCode, this->_replyProperties);

        return;
    }


    // We do a pause here if : pause != 0, doing a scrap, and pause is not only set for pagination
    if(_replyProperties.value("scrapPause",0).toInt() && _replyProperties.value("scrapPausePagination").toBool() == false  && _replyProperties.value("operationType").toInt() == NETWORK_SCRAP)
    {
        QTime dieTime= QTime::currentTime().addSecs(_replyProperties.value("scrapPause").toInt());
        while( QTime::currentTime() < dieTime )
            QCoreApplication::processEvents(QEventLoop::AllEvents, 100);
    }

    if(xpathResult.isEmpty())
    {
        if(this->xpathPagination.isEmpty())
        {
            if(!_minimalLogs)
                emit sendMsg(tr("[XPath] No match for query : %1 on URL %2").arg(processArgs[3],this->_url),"warning");
            this->sendEmptyResults();
            emit finished();
            return;
        }
        else
        {
            if(!_minimalLogs)
                emit sendMsg(tr("Last page found. No XPath found for next page on url %1").arg(this->_url),"success");
            // On remove la pagination
            this->results.remove("xpathPaginationRes");
            emit sourceCodeParsed(this->results, this->_replyProperties);
            emit finished();
            return;
        }
    }


    // If we have pagination, we keep the current thread
    // And we set the flag _paginationTreated to false
    if(!this->xpathPagination.isEmpty())
    {
        this->_paginationTreated = false;
        emit sendResultsToTable(this->results.value("xpathContentRes").toStringList(),_replyProperties, _url);

        // We set the pause here, if defined
        if(_replyProperties.value("scrapPause").toInt() && _replyProperties.value("scrapPausePagination").toBool()  && _replyProperties.value("operationType").toInt() == NETWORK_SCRAP)
        {
            QTime dieTime= QTime::currentTime().addSecs(_replyProperties.value("scrapPause").toInt());
            while( QTime::currentTime() < dieTime )
                QCoreApplication::processEvents(QEventLoop::AllEvents, 100);
        }

        this->createRequest();
    }
    else
    {
        emit sourceCodeParsed(this->results, this->_replyProperties);
        emit finished();
    }


}

void Worker::disconnectProcessSignals()
{
    disconnect(xidelProcess, SIGNAL(error(QProcess::ProcessError)), this, SLOT(xidelProcessError(QProcess::ProcessError)));
    disconnect(this->xidelProcess, SIGNAL(finished(int,QProcess::ExitStatus)),this, SLOT(xidelProcessFinished(int,QProcess::ExitStatus)));
    disconnect(this->xidelProcess, SIGNAL(started()),this, SLOT(xidelProcessStarted()));

    xidelProcess->close();

    delete xidelProcess;
    xidelProcess = nullptr;
}

void Worker::xidelProcessStarted()
{

}

void Worker::xidelProcessError(QProcess::ProcessError processError)
{
    QStringList processArgs;

    processArgs = this->xidelProcess->arguments();
    if(processArgs.size() > 2)
    {
        QFile tmpFile(processArgs.at(1));
        tmpFile.remove();
    }

    emit sendMsg("XPath process error : " + QString::number(processError), "error");

    this->disconnectProcessSignals();

    this->sendEmptyResults();
    emit finished();
}


void Worker::getXpath(const QString &url, const QByteArray &sourceCode, QHash<QString, QVariant> replyProperties, const QString &xpathFilename)
{
    QStringList xidelArguments;
    QString xidelCommand, xidelBinaryName;
    QString escapedQuery;
    QString tmpFileName;
    QString xpathQuery;
    QString outputEncoding;
    QDateTime timeFileName;
    QString detectedCharset;




    if(checkForAbort())
    {
        emit finished();
        return;
    }

    if(sourceCode.isEmpty())
    {
        emit sendMsg(tr("Source code for : %1 is empty !").arg(url), "error");
        this->sendEmptyResults();
        emit finished();
        return;
    }

    this->xidelProcess = new QProcess(this);

    QObject::connect(this->xidelProcess, SIGNAL(finished(int,QProcess::ExitStatus)),this, SLOT(xidelProcessFinished(int,QProcess::ExitStatus)));
    QObject::connect(this->xidelProcess, SIGNAL(started()),this, SLOT(xidelProcessStarted()));
    connect(xidelProcess, SIGNAL(error(QProcess::ProcessError)), this, SLOT(xidelProcessError(QProcess::ProcessError)));

    _threadMutex.lock();
    // We start the timer ?
    this->timer->start(_threadTimeout*1000);
    this->_url = url;
    this->xpathPagination = replyProperties.value("xpathPagination").toString();
    this->currentSourceCode = sourceCode;
    this->_replyProperties = replyProperties;
    xpathQuery = (this->_paginationTreated) ? this->xpathPagination : replyProperties.value("xpathContent").toString();

    detectedCharset = "";
    outputEncoding = "input";
    _threadMutex.unlock();

    // We replace RDDZ specific variables
    xpathQuery.replace("{%scrapurl%}", _url);
    QString encodedUrl = _url;
    xpathQuery.replace("{%scrapurl::encoded%}", encodedUrl.replace(QRegExp("&(?!amp;)"),"&amp;"));

    xpathQuery.replace("{%custom1%}", replyProperties.value("custom1").toString());
    xpathQuery.replace("{%custom2%}", replyProperties.value("custom2").toString());

    escapedQuery =  xpathQuery;

//#ifdef Q_OS_WIN32
//    // On win 32, args are auto enclose by ' if no spaces are found, otherwise they are enclosed by "
//    if(!escapedQuery.contains(" "))
//        escapedQuery.replace("'","\\\'");
//#endif

//#ifdef Q_OS_WIN64
//    // On win 64, args are auto enclose by '
//    escapedQuery.replace("'","\\\'");
//#endif

#ifdef Q_OS_WIN
    xidelBinaryName = "xidel.exe";
    //outputEncoding = "utf8";
#else
    xidelBinaryName = "xidel";
#endif

    _threadMutex.lock();
    timeFileName = QDateTime::currentDateTime();
    tmpFileName = (xpathFilename.isEmpty()) ? _tmpStackFile+"/rddzStack"+timeFileName.toString("yyyyMMddhhmmsszzz") : xpathFilename;
    QFile file( tmpFileName );
    QTextStream stream( &file );
    _threadMutex.unlock();


    // For encoding purpose
    // We get the content-type if present
    QRegularExpression charsetRegExp("<meta.*?charset=[\"']?([^\"']+)[\"']?",QRegularExpression::CaseInsensitiveOption);
    QRegularExpressionMatch match = charsetRegExp.match(sourceCode);
    if (match.hasMatch())
        detectedCharset = match.captured(1);
    if(_replyProperties.value("forceOutputEncoding").toString() == "latin1")
            detectedCharset = "ISO-8859-15";

    QTextCodec *codec = (!detectedCharset.isEmpty() && QTextCodec::codecForName(detectedCharset.toUtf8())) ? QTextCodec::codecForName(detectedCharset.toUtf8()) : QTextCodec::codecForName("UTF-8");

    if(!_minimalLogs && _replyProperties.value("operationType").toInt() == NETWORK_SCRAP)
        emit sendMsg("Charset : " + detectedCharset,"information");
    QString output = codec->toUnicode(sourceCode);

    // Start pre-treatment for JSON (if present)
    /********
    TODO : put field on expert mode for traitment
    Field must contains , 1,1 for array (correspond to [1][1])
    or indexOne,indexTwo for object values if only one field for object => value, else object before and value for the last
    ********/

    if(!_replyProperties.value("jsonStructure").toString().isEmpty())
    {
        QString jsonOutput;
        QStringList jsonDataz = _replyProperties.value("jsonStructure").toString().split(" ");

        foreach (QString jsonExp, jsonDataz) {
            jsonOutput += this->preJsonTreatment(output,jsonExp) + "\",\"";
        }
        jsonOutput.remove(jsonOutput.size() - 3, 3);
        output = jsonOutput;
    }
    // End of JSON pre-treatment

    if(this->_paginationTreated == false)
    {
        if(!_minimalLogs)
        {
            if(_replyProperties.value("displayStackFile").toBool())
                emit sendMsg(tr("Stack filename : %1 for %2").arg(tmpFileName,url), "information");
        }
    }

    // We don't write the file if it already exists
    if(!file.exists())
    {
        _threadMutex.lock();
        if ( file.open(QIODevice::ReadWrite) )
        {
            // We write the file into UTF8 format
            if(_replyProperties.value("sourceCodeType").toString() != "json")
            {
                stream.setCodec("UTF-8");
                // Next line cause some interpretations errors with xidel
//                stream.setGenerateByteOrderMark(true);
            }
            stream << output << endl;
            stream.flush();

        }
        file.close();
        _threadMutex.unlock();
    }


    if(!_replyProperties.value("forceOutputEncoding").toString().isEmpty())
        outputEncoding = _replyProperties.value("forceOutputEncoding").toString();

    if(_replyProperties.value("sourceCodeType").toString() == "json")
        xidelArguments << "--data" << tmpFileName << "-e" << escapedQuery << "--output-format" << "json-wrapped" << "--output-encoding" << outputEncoding << "--input-format" << "json" << "--printed-json-format" << "compact";
    else
        // A voir par la suite si on change aussi le outputEncoding ici
        xidelArguments << "--data" << tmpFileName << "-e" << escapedQuery << "--output-format" << "json-wrapped" << "--output-encoding" << "input" << "--printed-json-format" << "compact";


    xidelCommand = QCoreApplication::applicationDirPath()+"/dist/"+xidelBinaryName;
    xidelProcess->start(xidelCommand, xidelArguments);
}

QString Worker::preJsonTreatment(QString input, QString jsonStruct)
{
    QJsonParseError *parseError = new QJsonParseError;
    QJsonDocument jsonDoc = QJsonDocument::fromJson(input.toUtf8(),parseError);


    QStringList jsonStructList = jsonStruct.split(",");
    QJsonArray jsonArray;
    QJsonObject jsonObj;

    QVariant formatedOutput;
    int i = 0;

    formatedOutput.clear();


    int previousJsonType = 0;
    if(jsonDoc.isArray())
    {
        jsonArray = jsonDoc.array();
    }
    else if(jsonDoc.isObject())
    {
        jsonObj = jsonDoc.object();
        previousJsonType = 1;
    }
    else
    {
        emit sendMsg(tr("[JSON pre-treatment] Unknown JSON type"),"error");
        return formatedOutput.toString(); //return empty or original sourceCode ?
    }


    // Feature from 1.7.0 : retrieve values from entire json Exp
    for(i=0;i<jsonStructList.size();i++)
    {
        if(i+1 != jsonStructList.size())
        {
            if(!previousJsonType) // Array
            {
                // We check if index exists
                if(jsonArray.contains(jsonArray.at(jsonStructList.at(i).toInt())))
                {
                    if(jsonArray.at(jsonStructList.at(i).toInt()).type() == QJsonValue::Array)
                    {
                        previousJsonType = 0;
                        jsonArray = jsonArray.at(jsonStructList.at(i).toInt()).toArray();
                    }
                    else if(jsonArray.at(jsonStructList.at(i).toInt()).type() == QJsonValue::Object)
                    {
                        previousJsonType = 1;
                        jsonObj = jsonArray.at(jsonStructList.at(i).toInt()).toObject();
                    }
                    else // Error
                    {
                        previousJsonType = -1;
                    }
                }
                else
                {
                    emit sendMsg(tr("[JSON pre-treatment] Invalid array index : ") + jsonStructList.at(i),"error");
                    return QString("");
                }
            }
            else if(previousJsonType == 1) // Obj
            {
                // We check if index exists
                if(jsonObj.contains(jsonStructList.at(i)))
                {
                    if(jsonObj.value(jsonStructList.at(i)).type() == QJsonValue::Array)
                    {
                        previousJsonType = 0;
                        jsonArray = jsonObj.value(jsonStructList.at(i)).toArray();

                    }
                    else if(jsonObj.value(jsonStructList.at(i)).type() == QJsonValue::Object)
                    {
                        previousJsonType = 1;
                        jsonObj = jsonObj.value(jsonStructList.at(i)).toObject();
                    }
                    else // Erreur
                    {
                        previousJsonType = -1;
                    }
                }
                else
                {
                    emit sendMsg(tr("[JSON pre-treatment] Invalid object name : ") + jsonStructList.at(i),"error");
                    return QString("");
                }
            }
            else // -1 => error, just for security, normaly it will return before
            {
                emit sendMsg(tr("[JSON pre-treatment] Unknown json type"),"error");
                return formatedOutput.toString(); //return empty or original sourceCode ?
            }
        }
        else
        {
            if(!previousJsonType)
            {
                if(jsonArray.contains(jsonArray.at(jsonStructList.at(i).toInt())))
                    formatedOutput = jsonArray.at(jsonStructList.at(i).toInt()).toVariant();
                else
                {
                    emit sendMsg(tr("[JSON pre-treatment] Invalid value name : ") + jsonStructList.at(i),"error");
                    return QString("");
                }
            }
            else if (previousJsonType == 1) // If object, multiple values are possible
            {
                if(jsonObj.contains(jsonStructList.at(i)))
                    formatedOutput = jsonObj.value(jsonStructList.at(i)).toVariant();
                else
                {
                    emit sendMsg(tr("[JSON pre-treatment] Invalid value name : ") + jsonStructList.at(i),"error");
                    return QString("");
                }
            }
        }
    }


    if(parseError->error)
        emit sendMsg(tr("[JSON pre-treatment] JSON parse error : ") + parseError->errorString() + " at offset " + QString::number(parseError->offset), "error");

// WIP: Issue 16
//    not sure about the expected behaviour of this one
//    if((formatedOutput.type() == QVariant::Double) && ((formatedOutput.toDouble() - floor(formatedOutput.toDouble())) != 0))
      if((formatedOutput.type() == QVariant::Double) && ((formatedOutput.toDouble() - floor(formatedOutput.toDouble())) > 0))
        return QString::number(formatedOutput.toDouble(),'f',2);
    else
        return formatedOutput.toString();
}

void Worker::sendEmptyResults()
{
    QHash<QString,QVariant> emptyRes;
    emptyRes.insert("url",this->_url);
    emptyRes.insert("xpathContentRes",QVariant(QStringList()));
    emptyRes.insert("xpathPaginationRes","");


    emit sourceCodeParsed(emptyRes,this->_replyProperties);
    emit finished();
}

bool Worker::checkForAbort()
{
    _threadMutex.lock();
    bool abort = _abort;
    _threadMutex.unlock();

    return abort;
}

void Worker::abortSignalReceived()
{
    _abort = true;
}

// From here, we put all utility functions

QString Worker::prettyFormatUrl(const QString &url)
{
    QUrl testQurl(url);

    if(testQurl.isValid() && !testQurl.isRelative())
        return QUrl::fromPercentEncoding(url.toUtf8());
    else
        return url;
}

void Worker::getBacklinks(const QByteArray &sourceCode, const QString &url)
{
    int i;
    qulonglong bls;
    QJsonDocument jsonDoc;
    QJsonObject jsonObj;
    QJsonArray dataValue;
    QString majesticCode;
    QHash<QString,QVariant> result;
    QHash<QString,QVariant> blsValues;

    QList<QVariant> urlsList;
    QList<QVariant> blsList;
    // For Majestic
    QList<QVariant> referringDomainsList;
    QList<QVariant> trustFlowList;
    QList<QVariant> citationFlowList;
    QList<QVariant> ttftList;
    QList<QVariant> ttfvList;
    QList<QVariant> statusList;
    QList<QVariant> lcdList; // Last Crawl Date
    // For Moz
    QList<QVariant> paList;
    QList<QVariant> daList;
    QList<QVariant> mozRankList;

    jsonDoc = QJsonDocument::fromJson(sourceCode);
    jsonObj = jsonDoc.object();

    _threadMutex.lock();
    bls = 0;
    _threadMutex.unlock();


    if(_replyProperties.value("currentAPI","Free").toString() == "Majestic" || _replyProperties.value("currentAPI","Free").toString() == "SEObserver")
    {
        QUrl majesticQurl(url);
        QUrlQuery majesticQurlQuery(majesticQurl);
        majesticCode = jsonObj.value("Code").toString();
        jsonObj = jsonObj.value("DataTables").toObject();
        jsonObj = jsonObj.value("Results").toObject();
        dataValue = jsonObj.value("Data").toArray();

        if(majesticCode == "OK")
        {
            if(dataValue.size())
            {
                for(i=0;i < dataValue.size();i++)
                {
                    // We do the check on the items passed as parameter
                    urlsList.append(majesticQurlQuery.queryItemValue("item"+dataValue.at(i).toObject().value("ItemNum").toVariant().toString()));

                    if (dataValue.at(i).toObject().value("Status").toString() != "Found")
                    {
                        blsList.append(0);
                        referringDomainsList.append(0);
                        citationFlowList.append(-1);
                        trustFlowList.append(-1);
                        ttftList.append("");
                        ttfvList.append(-1);
                        statusList.append(dataValue.at(i).toObject().value("Status").toVariant().toString());
                        lcdList.append("-");
                    }
                    else
                    {
                        blsList.append(dataValue.at(i).toObject().value("ExtBackLinks").toVariant().toULongLong());
                        referringDomainsList.append(dataValue.at(i).toObject().value("RefDomains").toVariant().toULongLong());
                        citationFlowList.append(dataValue.at(i).toObject().value("CitationFlow").toVariant().toInt());
                        trustFlowList.append(dataValue.at(i).toObject().value("TrustFlow").toVariant().toInt());
                        ttftList.append(dataValue.at(i).toObject().value("TopicalTrustFlow_Topic_0").toVariant().toString());
                        ttfvList.append(dataValue.at(i).toObject().value("TopicalTrustFlow_Value_0").toVariant().toInt());
                        statusList.append(dataValue.at(i).toObject().value("Status").toVariant().toString());
                        if(dataValue.at(i).toObject().value("CrawledFlag").toVariant().toBool())
                            lcdList.append(dataValue.at(i).toObject().value("LastCrawlDate").toVariant().toString());
                        else
                            lcdList.append(dataValue.at(i).toObject().value("LastCrawlResult").toVariant().toString());
                    }

                }
            }
        }
        else
        {
            qDebug() << "Majestic reply not OK for query : " << url << majesticCode;
        }

        blsValues.insert("Urls",urlsList);
        blsValues.insert("ExtBackLinks",blsList);
        blsValues.insert("RefDomains",referringDomainsList);
        blsValues.insert("CitationFlow",citationFlowList);
        blsValues.insert("TrustFlow",trustFlowList);
        blsValues.insert("TopicalTrustFlow_Topic_0",ttftList);
        blsValues.insert("TopicalTrustFlow_Value_0",ttfvList);
        blsValues.insert("Status",statusList);
        blsValues.insert("LastCrawlDate",lcdList);

        result.insert("API","Majestic");
        result.insert("Results",blsValues);
        emit sendBacklinks(result,url);
        emit finished();
        return;
    }
    else if(_replyProperties.value("currentAPI","Free").toString() == "Moz")
    {
        // Datas are in the QStringList order
        QStringList passedUrlsList = _replyProperties.value("urlsList").toStringList();
        QString errorMsg;

        dataValue = jsonDoc.array();
        errorMsg = jsonObj.value("error_message").toString();

        if(!errorMsg.isEmpty())
        {
            QString mozStatus = jsonObj.value("status").toString();
            // We have to wait before sending another request
            if(mozStatus.toInt() == 429)
            {
                QTime dieTime= QTime::currentTime().addSecs(12);
                while( QTime::currentTime() < dieTime )
                    QCoreApplication::processEvents(QEventLoop::AllEvents, 100);

                emit sendRetry(_url, _replyProperties);
                emit finished();
                return;
            }
            else
            {
                emit sendMsg(errorMsg,"error");
                emit sendAbortSignal();
                emit finished();
                return;
            }
        }
        else
        {
            for(i=0;i<passedUrlsList.size();i++)
            {
                jsonObj = dataValue.at(i).toObject();
                urlsList.append(passedUrlsList.at(i));
                blsList.append(jsonObj.value("ueid").toVariant().toULongLong());
                paList.append(qRound(jsonObj.value("upa").toDouble()));
                daList.append(qRound(jsonObj.value("pda").toDouble()));
                mozRankList.append(jsonObj.value("umrp").toDouble());
            }
        }

        blsValues.insert("Urls",urlsList);
        blsValues.insert("ueid",blsList);
        blsValues.insert("upa",paList);
        blsValues.insert("pda",daList);
        blsValues.insert("umrp",mozRankList);


        result.insert("API","Moz");
        result.insert("Results",blsValues);
        emit sendBacklinks(result,url);
        emit finished();
        return;
    }
    else if(_replyProperties.value("currentAPI","Free").toString() == "Ahrefs")
    {

        jsonObj = jsonObj.value("metrics").toObject();
        bls = static_cast<unsigned long long>(jsonObj.value("backlinks").toDouble());

        result.insert("API","Ahrefs");
        result.insert("backlinks",bls);
        emit sendBacklinks(result,url);
        emit finished();
        return;
    }
}

/**
 * @brief Worker::gUncompress uncompress gzipped data return from server
 * @param data : the compressed data
 * @return QByteArray : uncompressed data
 */
QByteArray Worker::gUncompress(const QByteArray &data)
{
    if (data.size() <= 4) {
        qWarning("gUncompress: Input data is truncated");
        return QByteArray();
    }

    QByteArray result;

    int ret;
    z_stream strm;
    static const int CHUNK_SIZE = 1024;
    char out[CHUNK_SIZE];

    /* allocate inflate state */
    strm.zalloc = Z_NULL;
    strm.zfree = Z_NULL;
    strm.opaque = Z_NULL;
    strm.avail_in = static_cast<unsigned int>(data.size());
    // WIP: Issue 16
    strm.next_in = reinterpret_cast<Bytef*>(const_cast<char *>(data.data()));

    ret = inflateInit2(&strm, 15 +  32); // gzip decoding
    if (ret != Z_OK)
        return QByteArray();

    // run inflate()
    do {
        strm.avail_out = CHUNK_SIZE;
        strm.next_out = reinterpret_cast<Bytef*>(out);

        ret = inflate(&strm, Z_NO_FLUSH);
        Q_ASSERT(ret != Z_STREAM_ERROR);  // state not clobbered

        switch (ret) {
        case Z_NEED_DICT:
            ret = Z_DATA_ERROR;     // and fall through
            [[clang::fallthrough]];
        case Z_DATA_ERROR:
        case Z_MEM_ERROR:
            (void)inflateEnd(&strm);
            return QByteArray();
        }

        result.append(out, static_cast<int>(CHUNK_SIZE - strm.avail_out));
    } while (strm.avail_out == 0);

    // clean up and return
    inflateEnd(&strm);
    return result;
}

