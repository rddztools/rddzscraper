#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "scraper.h"
#ifndef NETWORK_H
#include "network.h"
#endif
#ifdef Q_OS_WIN
#include <windows.h>
#endif
#ifdef Q_OS_LINUX
#include <unistd.h>
#endif

Scraper::Scraper(MainWindow *mw, Config *conf, scrapTable *scraptable)
{
    config  = conf;
    mainWin = mw;
    tools = new Tools();
    this->scraptable = scraptable;
    backlinks = new Backlinks(mw, conf, this);

    QSettings *settings = config->getGlobalSettings();
    if(!settings->childGroups().contains("ExternalPrivateKeys"))
    {
        settings->beginGroup("ExternalPrivateKeys");
        settings->setValue("Majestic","");
        settings->setValue("SEObserver","");
        settings->endGroup();
        settings->sync();
    }

    countScraper    = this->mainWin->getNbUrl(this->scraptable->columnIndexes.value("urlCol"));
    countHeader     = this->mainWin->getNbUrl(this->scraptable->columnIndexes.value("httpCol"));
    countRel        = this->mainWin->getNbUrl(this->scraptable->columnIndexes.value("dfCol"));
    countBls        = this->mainWin->getNbUrl(this->scraptable->columnIndexes.value("blCol"));
    countObl        = this->mainWin->getNbUrl(this->scraptable->columnIndexes.value("oblCol"));
    countIp         = this->mainWin->getNbUrl(this->scraptable->columnIndexes.value("ipCol"));
    countLinkAlive  = this->mainWin->getNbUrl(this->scraptable->columnIndexes.value("linkAliveCol"));
    countPlatform   = this->mainWin->getNbUrl(this->scraptable->columnIndexes.value("platformCol"));
    countWhois      = this->mainWin->getNbUrl(this->scraptable->columnIndexes.value("domainAvailableCol"));


    scrapUrlHasCustoms = false;
    initScrapUrlSize = 0;
    this->scrapedBytes = 0;
    this->_threadsTimedOut = 0;
    jsEnable = false;
    htmlEncoded = false;
    forceDeleteStack = false;
    this->currentAction = "scrap";
    this->keepStackFiles = false;


    this->maxThreads = 100;
    this->urlsChecked = 0;
    this->nextUrlIndex = 0;
    this->threadTimeout = 10; // in sec
    resultsFound = 0;
    this->testingCounter = 0;

    _abortCliked = false;
    _working = false;
    _totalItemsFound = 0;
    _dynadotResponsesOk = 0;
    _scrapPause = 0;
    _scrapPausePagination = 0;
    _uaList = (this->seSelected.value("specificUa").toString().isEmpty()) ? config->get_ua("standard") : this->seSelected.value("specificUa").toStringList();
    _googleUrlOnCustom = false;
    _cookiesForProxy.clear();
    _whoisApiService = "";
    _whoisApiKey = "";
    _seJsonStructure = "";
    _tldNotSupportedMsg = "tld not supported by API";
    _blApiForScrap = config->getConfigurationValue("blApiForScrap","Free");
    _minimalLogs = false;
    _scrapUrlIndex = 0;
    _tmpStackDirectory = QApplication::applicationDirPath()+"/tmp";
//    _subScrapNb = 10;
    _subScrapNb = 10000;
    _subScrapIndex = 0;
    _resultsStack.clear();
    _httpLogFilename = "";
    scrapurls.clear();


    ipAddrTimer        = new QTimer(this);
    QObject::connect(ipAddrTimer, SIGNAL(timeout()), this, SLOT(ipAddrTimeout()));

    qRegisterMetaType< QHash<int,QVariant> >("QHash<int,QVariant>");
    qRegisterMetaType< QPair<QString,QString> >("QPair<QString,QString>");
    qRegisterMetaType< QHash<QString,QString> >("QHash<QString,QString>");
    qRegisterMetaType< QHash<QString,QHash<int,QVariant> > >("QHash<QString,QHash<int,QVariant>>");

    QObject::connect(ipAddrTimer, SIGNAL(timeout()), this, SLOT(ipAddrTimeout()));
    QObject::connect(this, SIGNAL(removeDuplicateUrl()),            mw, SLOT(removeDuplicateUrlSlot()));
    QObject::connect(this,SIGNAL(updateTableViewSignal(QString,QVariant,int)), mw, SLOT(updateTableViewSlot(QString,QVariant,int)));
    QObject::connect(this, SIGNAL(updateTableViewMappingSignal(QPair<QString,QString>,QPair<QString,QString>,QVariant,int)), mw, SLOT(updateTableMappingViewSlot(QPair<QString,QString>,QPair<QString,QString>,QVariant,int)));
    // All Signal/Slot after can be replace by a unique signal/slot
    QObject::connect(this, SIGNAL(updateUrl(QString, QString)),     mw, SLOT(updateUrl(QString, QString)));
    QObject::connect(this, SIGNAL(updateHttpStatus(QString, int)),  mw, SLOT(updateHttpStatus(QString, int)));
   // QObject::connect(this, SIGNAL(UpdateRelUrls(QString, QString)), mw, SLOT(UpdateRelUrls(QString, QString)));
    connect(this, &Scraper::UpdateRelUrls, mw, &MainWindow::UpdateRelUrls);


    QObject::connect(this, SIGNAL(updateBacklinks(QString,qlonglong)),   mw, SLOT(updateBacklinks(QString,qlonglong)));
    QObject::connect(this, SIGNAL(updateObl(QString,int)),        mw, SLOT(updateObl(QString,int)));
    QObject::connect(this, SIGNAL(updateIpAddr(QString,QString)),        mw, SLOT(updateIpAddr(QString,QString)));
    QObject::connect(this, SIGNAL(updateLinkAlive(QString,QString)),        mw, SLOT(updateLinkAlive(QString,QString)));
    QObject::connect(this, SIGNAL(updatePlatform(QString,QString)),        mw, SLOT(updatePlatform(QString,QString)));
    //QObject::connect(this, SIGNAL(updateDomainAvailable(QString,QString)),        mw, SLOT(updateDomainAvailable(QString,QString)));
    connect(this, &Scraper::updateDomainAvailable,mw, &MainWindow::updateDomainAvailable);
    QObject::connect(this, SIGNAL(updateCitationFlow(QString,int)),        mw, SLOT(updateCitationFlow(QString,int)));
    QObject::connect(this, SIGNAL(updateTrustFlow(QString,int)),        mw, SLOT(updateTrustFlow(QString,int)));
    QObject::connect(this, SIGNAL(updateMultipleCol(QString, QHash<int, QVariant>,bool)),  mw, SLOT(updateMultipleCol(QString, QHash<int, QVariant>,bool)));
    QObject::connect(this, SIGNAL(updateMultipleRows(QHash<QString,QHash<int,QVariant> >,bool)),  mw, SLOT(updateMultipleRows(QHash<QString,QHash<int,QVariant> >,bool)));
    QObject::connect(this, SIGNAL(removeBadUrl()),                  mw, SLOT(removeBadUrl()));
    QObject::connect(this, SIGNAL(scrapButtonEnabled()),            mw, SLOT(scrapButtonEnabled()));
    QObject::connect(this, SIGNAL(appendLog(QString,QString)),              mw, SLOT(appendLog(QString, QString)));
    QObject::connect(this, SIGNAL(autoHttpStatus()),                mw, SLOT(autoHttpStatus()));
    QObject::connect(this, SIGNAL(autoResolveRedirect()),           mw, SLOT(autoResolveRedirect()));
    QObject::connect(this, SIGNAL(autoCheckDofollowNofollow()),     mw, SLOT(autoCheckDofollowNofollow()));
    QObject::connect(this, SIGNAL(autoBacklinks()),                 mw, SLOT(autoBacklinks()));
    QObject::connect(this, SIGNAL(autoObl()),                      mw, SLOT(autoObl()));
    QObject::connect(this, SIGNAL(autoIp()),                      mw, SLOT(autoIp()));
    QObject::connect(this, SIGNAL(updateTaskProgressBar(int,int,QString)), mw, SLOT(updateProgressBar(int,int,QString)));
    QObject::connect(this, SIGNAL(updateBacklinksBalanceUnits(QString,QString)),mw,SLOT(updateBacklinksBalanceUnits(QString,QString)));

    QObject::connect(this, SIGNAL(showSystrayMsg(QString)), mw, SLOT(showSystrayMsgSlot(QString)));

//    QObject::connect(this, SIGNAL(stopTimer(int)),              this, SLOT(stopSpecificTimer(int)));
    QObject::connect(this,SIGNAL(insertProxies(QStringList)),       mw, SLOT(insertProxies(QStringList)));
    QObject::connect(this,SIGNAL(updateCustomRatio(int)),       mw,SLOT(updateCustomRatio(int)));
    QObject::connect(this,SIGNAL(updateNbItems(int)),       mw,SLOT(updateNbUrl(int)));
    QObject::connect(this, SIGNAL(launchRequest(int)), this,SLOT(launchRequestSlot(int)));


    QObject::connect(this,SIGNAL(runPhantomScrap(QHash<QString,QString>)),mw, SLOT(launchPhantomScrap(QHash<QString,QString>)));

}

Scraper::~Scraper()
{
}

void Scraper::disconnectScraper()
{
    QObject::disconnect(ipAddrTimer, SIGNAL(timeout()), this, SLOT(ipAddrTimeout()));
    QObject::disconnect(this, SIGNAL(removeDuplicateUrl()),            mainWin, SLOT(removeDuplicateUrlSlot()));
    QObject::disconnect(this, SIGNAL(updateUrl(QString, QString)),     mainWin, SLOT(updateUrl(QString, QString)));
    QObject::disconnect(this, SIGNAL(updateHttpStatus(QString, int)),  mainWin, SLOT(updateHttpStatus(QString, int)));
    //QObject::disconnect(this, SIGNAL(UpdateRelUrls(QString, QString)), mainWin, SLOT(UpdateRelUrls(QString, QString)));
    QObject::disconnect(this, SIGNAL(updateBacklinks(QString,qlonglong)),   mainWin, SLOT(updateBacklinks(QString,qlonglong)));
    QObject::disconnect(this, SIGNAL(updateObl(QString,int)),        mainWin, SLOT(updateObl(QString,int)));
    QObject::disconnect(this, SIGNAL(updatePlatform(QString,QString)),        mainWin, SLOT(updatePlatform(QString,QString)));
    QObject::disconnect(this, SIGNAL(updateCitationFlow(QString,int)),        mainWin, SLOT(updateCitationFlow(QString,int)));
    QObject::disconnect(this, SIGNAL(updateTrustFlow(QString,int)),        mainWin, SLOT(updateTrustFlow(QString,int)));
    QObject::disconnect(this, SIGNAL(removeBadUrl()),                  mainWin, SLOT(removeBadUrl()));
    QObject::disconnect(this, SIGNAL(scrapButtonEnabled()),            mainWin, SLOT(scrapButtonEnabled()));
    QObject::disconnect(this, SIGNAL(appendLog(QString)),              mainWin, SLOT(appendLog(QString)));
    QObject::disconnect(this, SIGNAL(autoHttpStatus()),                mainWin, SLOT(autoHttpStatus()));
    QObject::disconnect(this, SIGNAL(autoResolveRedirect()),           mainWin, SLOT(autoResolveRedirect()));
    QObject::disconnect(this, SIGNAL(autoCheckDofollowNofollow()),     mainWin, SLOT(autoCheckDofollowNofollow()));
    QObject::disconnect(this, SIGNAL(autoBacklinks()),                  mainWin, SLOT(autoBacklinks()));
    QObject::disconnect(this, SIGNAL(autoObl()),                        mainWin, SLOT(autoObl()));
    QObject::disconnect(this, SIGNAL(autoIp()),                         mainWin, SLOT(autoIp()));
    QObject::disconnect(this, SIGNAL(updateBacklinksBalanceUnits(QString,QString)),mainWin,SLOT(updateBacklinksBalanceUnits(QString,QString)));
    QObject::disconnect(this,SIGNAL(updateCustomRatio(int)),       mainWin,SLOT(updateCustomRatio(int)));

//    QObject::disconnect(this, SIGNAL(stopTimer(int)),              this, SLOT(stopSpecificTimer(int)));
    QObject::disconnect(this,SIGNAL(insertProxies(QStringList)),       mainWin, SLOT(insertProxies(QStringList)));

    qRegisterMetaType< QHash<QString,QString> >("QHash<QString,QString>");
    QObject::disconnect(this,SIGNAL(runPhantomScrap(QHash<QString,QString>)),mainWin, SLOT(launchPhantomScrap(QHash<QString,QString>)));
}


void Scraper::startIpTimer()
{
    ipAddrTimerCount = 0;
    countIp = 0;
    ipAddrTimer->start(30000);
}

void Scraper::ipAddrTimeout()
{
    if (ipAddrTimerCount == 0)
        ipAddrTimerCount = countIp;
    else if (ipAddrTimerCount == countIp)
    {
        appendLog(tr("Retrieving ip addresse timed out"), "warning");
        sendAbortNetwork();// abort
        ipAddrTimer->stop();
        emit scrapButtonEnabled();
    }
    else
        ipAddrTimerCount = countIp;
}


void Scraper::stopIpTimer()
{
     ipAddrTimer->stop();
}

bool Scraper::isScrapRunning()
{
    return this->_scrapRunning;
}


void Scraper::setFootprint(QString footprint)
{
    this->footprint = footprint;
}

void Scraper::clearCustomList()
{
    this->custom1List.clear();
    this->custom2List.clear();
}

void Scraper::setCustomLoopList(QStringList loop1, QStringList loop2)
{
    this->custom1List   = loop1;
    this->custom2List   = loop2;
}

void Scraper::setSeSelected(QHash<QString, QVariant> scrapEngineConfig)
{
    this->seSelected.clear();
    this->seSelected = scrapEngineConfig;
}

void Scraper::setWhoisApiKey(QString apiKey)
{
    _whoisApiKey = apiKey;
}

void Scraper::setWhoisApiService(QString apiService)
{
    _whoisApiService = apiService;
}

void Scraper::clearFootprint()
{
    this->footprint.clear();
}

void Scraper::sendAbortNetwork()
{
    ipAddrTimer->stop();

    _abortCliked = true;
    //qDebug() << scrapurls.size();

    //scrapurls.clear();

    if(_scrapRunning)
    {
#ifdef Q_OS_WIN
        if(jsEnable)
            QProcess::startDetached("taskkill /F /IM phantomjs.exe");
        QProcess::startDetached("taskkill /F /IM xidel.exe");
#else
        if(jsEnable)
            QProcess::startDetached("killall phantomjs");
        QProcess::startDetached("killall xidel");
#endif
    }

    if(config->getConfigurationValue("autoSaveScrap") == "true")
        autoSaveScrapResultsFile.close();

//    if(this->captcha)
//        delete this->captcha;

    if(!this->seSelected.value("xpath_pagination").toString().isEmpty())
        emit updateTaskProgressBar(100,100,"");

    // On send le updateMultipleRows si le _resultsStack n'est pas vide
    if(!_resultsStack.isEmpty())
         emit updateMultipleRows(_resultsStack,true);

//    if(jsEnable)
//    emit scrapButtonEnabled();
//    else
    emit abortNetwork();

    emit updateNbItems();
    //emit updateScrapTableLayout();

    _scrapRunning = false;
}


void Scraper::disconnectReply(QNetworkReply *r, int type)
{
    r->disconnect(SIGNAL(error(QNetworkReply::NetworkError)));


    switch (type){
    case NETWORK_SCRAP: // Scrap
        QObject::disconnect(r, SIGNAL(finished()),this, SLOT(getScrapFinished()));
        break;
    case NETWORK_URL_BL: // Backlinks / url
        QObject::disconnect(r, SIGNAL(finished()),this, SLOT(getBacklinksFinished()));
        break;
    case NETWORK_OBL: // Outbound Links
        QObject::disconnect(r, SIGNAL(finished()),this, SLOT(getOblFinished()));
        break;
    case NETWORK_HTTPSTATUS: // getUrlRespCode
//        QObject::disconnect(r, SIGNAL(finished()),this, SLOT(getUrlRespCodeFinished()));
        QObject::disconnect(r, SIGNAL(metaDataChanged()),this, SLOT(getUrlRespCodeFinished()));
        break;
    case NETWORK_REDIR: // resolveRedir
        QObject::disconnect(r, SIGNAL(metaDataChanged()),this, SLOT(resolveRedirectionsFinished()));
        break;
    case NETWORK_DOFOLLOW: // checkDofollowNofollow
        QObject::disconnect(r, SIGNAL(finished()),this, SLOT(checkDofollowNofollowFinished()));
        break;
    case NETWORK_LINK_ALIVE: // link alive check
        QObject::disconnect(r, SIGNAL(finished()),this, SLOT(getLinkAliveFinished()));
        break;
    case NETWORK_PLATFORM: // Platform check
        QObject::disconnect(r, SIGNAL(finished()),this, SLOT(getPlatformFinished()));
        break;
    case NETWORK_WHOIS: // Whois check
        QObject::disconnect(r, SIGNAL(finished()),this, SLOT(getWhoisFinished()));
        break;
    case AHREFS_BALANCE: // ahrefs balance
        QObject::disconnect(r, SIGNAL(finished()),this->backlinks, SLOT(getAhrefsBalanceFinished()));
        break;
    case AHREFS_BL_COUNT: // ahrefs backlinks count
        QObject::disconnect(r, SIGNAL(finished()),this->backlinks, SLOT(getAhrefsBacklinksCount()));
        break;
    case AHREFS_BACKLINKS: // ahrefs backlinks
        QObject::disconnect(r, SIGNAL(finished()),this->backlinks, SLOT(getAhrefsBacklinks()));
        break;
    case MAJESTIC_BALANCE: // majestic balance
        QObject::disconnect(r, SIGNAL(finished()),this->backlinks, SLOT(getMajesticBalanceFinished()));
        break;
    case MAJESTIC_BACKLINKS: // majestic backlinks
        QObject::disconnect(r, SIGNAL(finished()),this->backlinks, SLOT(getMajesticBacklinks()));
        QObject::disconnect(r, SIGNAL(downloadProgress(qint64,qint64)), this, SLOT(donwloadProgress(qint64,qint64)));
        break;
    case MOZ_BACKLINKS: // moz backlinks
        QObject::disconnect(r, SIGNAL(finished()),this->backlinks, SLOT(getMozBacklinks()));
        QObject::disconnect(r, SIGNAL(downloadProgress(qint64,qint64)), this, SLOT(donwloadProgress(qint64,qint64)));
        break;
    default:
        break;
    }
}

// On devrait savoir si il y a pagination ou pas
QStringList Scraper::getXpath(QString str, QString queryXpath, QString custom1, QString custom2, QString xpathUrl, bool displayLog)
{

    QString tmpFileName, xidelCommand, xidelBinaryName, escapedQuery;
    QString outputEncoding;
    QString detectedCharset;
    QStringList arguments, xpathResult;
    QProcess *xidelProcess = new QProcess(nullptr);
    QJsonDocument jsonDoc, errJsonDoc;
    QTextDocument txtDoc;
    QDateTime timeFileName;
    int i,j;


//    qDebug() << "[Scraper::getXpath] Entering function ..." << str.isEmpty() << isScrapRunning();

    // Si le code source est empty, on return
    if(str.isEmpty() && isScrapRunning())
    {
        //emit appendLog(tr("Source code for : ") + xpathUrl + tr(" is empty !"), "error");
        xpathResult.append("");
        return xpathResult;
    }

    // Si on a abort, on return
//    if(!isScrapRunning())
//    {
//        xpathResult.append("");
//        return xpathResult;
//    }


    detectedCharset = "";
    outputEncoding = "input";
    queryXpath = queryXpath.trimmed();
    queryXpath.replace("{%footprint%}", this->footprint);
    if(queryXpath.contains(QRegExp("\\{%custom[12]{1}%\\}")))
    {
        queryXpath.replace("{%custom1%}", custom1);
        queryXpath.replace("{%custom2%}", custom2);
    }
    else if(queryXpath.contains(QRegExp("\\{%custom[12]::encoded%\\}")))
    {
        queryXpath.replace("{%custom1::encoded%}", this->replaceCustom(custom1,1));
        queryXpath.replace("{%custom2::encoded%}", this->replaceCustom(custom2,1));
    }

//    queryXpath.replace("{%custom1%}", custom1);
//    queryXpath.replace("{%custom2%}", custom2);

    escapedQuery = queryXpath;

#ifdef Q_OS_WIN32
    // On win 32, args are auto enclose by ' if no spaces are found, otherwise they are enclosed by "
    if(!escapedQuery.contains(" "))
        escapedQuery.replace("'","\\\'");
#endif

#ifdef Q_OS_WIN
    xidelBinaryName = "xidel.exe";
#else
    xidelBinaryName = "xidel";
#endif

    xpathResult.clear();

    // Test for new version
    scraperMutex.lock();
    timeFileName = QDateTime::currentDateTime();
    tmpFileName = _tmpStackDirectory+"/rddzStack"+timeFileName.toString("yyyyMMddhhmmsszzz");
    QFile file( tmpFileName );
    QTextStream stream( &file );
    scraperMutex.unlock();

    // For encoding purpose
    // We get the content-type if present
    QRegularExpression charsetRegExp("<meta.*?charset=[\"']?([^\"']+)[\"']?",QRegularExpression::CaseInsensitiveOption);
    QRegularExpressionMatch match = charsetRegExp.match(str);
    if (match.hasMatch())
        detectedCharset = match.captured(1);

    QTextCodec *codec = (!detectedCharset.isEmpty() && QTextCodec::codecForName(detectedCharset.toUtf8())) ? QTextCodec::codecForName(detectedCharset.toUtf8()) : QTextCodec::codecForName("UTF-8");


    if(displayLog)
    {
        emit appendLog(tr("Charset detected : %1 for %2").arg(QString(codec->name()), xpathUrl),"information");
        emit appendLog(tr("Stack filename : %1 for %2").arg(tmpFileName, xpathUrl), "information");
    }

    QString output = codec->toUnicode(str.toUtf8());
    // On ne re-ecrit pas le fichier si il existe deja
    if(!file.exists())
    {
        if ( file.open(QIODevice::ReadWrite) )
        {
            scraperMutex.lock();
             // We write the file into UTF8 format
            stream.setCodec("UTF-8");
            stream << output << endl;
            stream.flush();
            scraperMutex.unlock();
        }
        file.close();
    }

    if(config->fullDebug && config->getConfigurationValue("minimalLog") == "false")
        emit appendLog("[FullDebug] Xpath query : "+queryXpath);

       arguments << "--data" << tmpFileName << "-e" << escapedQuery << "--output-format" << "json-wrapped" << "--output-encoding" << outputEncoding;
    xidelCommand = QCoreApplication::applicationDirPath()+"/dist/"+xidelBinaryName;

    xidelProcess->start(xidelCommand, arguments);

    if (!xidelProcess->waitForStarted())
    {
        emit appendLog(tr("Unable to start xidel process"),"error");
        return xpathResult;
    }

    if (!xidelProcess->waitForFinished())
    {
       emit appendLog(tr("Xidel process has crashed"),"error");
        xidelProcess->kill();
        return xpathResult;
    }

    QByteArray results = xidelProcess->readAllStandardOutput();
    QString xpathError = xidelProcess->readAllStandardError();
    xidelProcess->close();

    xpathError.replace(QRegExp("([\\*]{4}[^\\*]+[\\*]{4}|^$)"),"");

     QJsonParseError *jsonParseError = new QJsonParseError;

    QString strResult(results);
    errJsonDoc = QJsonDocument::fromJson(xpathError.toUtf8());
    jsonDoc = QJsonDocument::fromJson(strResult.toUtf8(),jsonParseError);
    QJsonArray values = jsonDoc.array();

   if (errJsonDoc.isObject())
   {
       QJsonObject jsonObj;
       jsonObj = errJsonDoc.object();
       jsonObj = jsonObj.value("_error").toObject();

       emit appendLog(tr("[XPath Error] Unable to evaluate XPath query : %1 for %2 ").arg(queryXpath, xpathUrl), "error");
       emit appendLog(tr("[XPath Error]  %1").arg(jsonObj.value("_message").toString()), "error");

       xpathResult.append("");
       return xpathResult;
   }

    for (i=0;i<values.size();i++)
    {
        if(values.at(i).isArray())
        {
            QJsonArray arrayValues = values.at(i).toArray();
            for(j=0;j<arrayValues.size();j++)
            {
                QString strValue = arrayValues.at(j).toString();
                if(this->htmlEncoded == false)
                {
                    txtDoc.setHtml(strValue);
                    strValue = txtDoc.toPlainText();
                }
                xpathResult.append(strValue);
            }
        }
        else
        {
            if(values.at(i).isString() || values.at(i).isBool())
            {
                QString strValue = values.at(i).toString();;
                if(this->htmlEncoded == false)
                {
                    txtDoc.setHtml(strValue);
                    strValue = txtDoc.toPlainText();
                }
                xpathResult.append(strValue);
            }
            else if (values.at(i).isDouble())
                xpathResult.append(QString::number(values.at(i).toDouble()));
        }
    }

    if(xpathResult.isEmpty())
    {
        xpathResult.append("");

        if(config->getConfigurationValue("minimalLog") == "false")
        {
            if(displayLog == true)
                emit appendLog(tr("[XPath] No match for query : %1 on URL %2").arg(queryXpath,xpathUrl),"warning");
        }
    }

    // We delete temp file
    if(config->getConfigurationValue("htmlStack") != "true" || this->scrapurlsSize > 50 || this->forceDeleteStack)
    {
        QFile temporaryFile(tmpFileName);
        temporaryFile.remove();
    }

    return xpathResult;
}


void Scraper::resolveRedirections(QString url)
{
    QHash<QString,QVariant> queryParameters;
    queryParameters.insert("operationType",NETWORK_REDIR);
    this->sendRequest(url,queryParameters);
}

void Scraper::resolveRedirectionsFinished()
{

}

void Scraper::resolveRedirectionsProcessResults(int statusCode, QString url, QString redirectedUrl)
{
    QHash<int, QVariant> newValues;

    if(_abortCliked)
        return;

    scraperMutex.lock();
    this->countHeader++;
    this->scrapurlsSize--;
    this->urlsChecked++;
    scraperMutex.unlock();

    newValues.insert(scraptable->columnIndexes.value("httpCol"), statusCode);
    if(!redirectedUrl.isEmpty())
        newValues.insert(scraptable->columnIndexes.value("urlCol"), redirectedUrl);

    emit updateTaskProgressBar(this->countHeader, this->scraptable->scrapResults.size(), tr("Resolving redirections"));
    emit updateMultipleCol(url, newValues,true);

    if (this->countHeader == this->scraptable->scrapResults.size())
    {
        if(config->getAutoResolveRedir())
        {
            int nbItemsByCol = this->scraptable->getNbItemsByCol(this->scraptable->columnIndexes.value("httpCol"), true);
            if(scraptable->previousRedirLeft == nbItemsByCol)
                scraptable->redirRetry++;
            if(!scraptable->previousRedirLeft || (scraptable->previousRedirLeft != nbItemsByCol))
                scraptable->redirRetry = 0;
            scraptable->previousRedirLeft = nbItemsByCol;
        }
        this->doAutoTasks(REQUEST_REDIRECTION);
    }
    else if(startAt < this-> urlList.size())
    {
        scraperMutex.lock();
        ++startAt;
        scraperMutex.unlock();
        this->requestLauncher(3);
    }
}



void Scraper::getUrlRespCode(QString url)
{
    QHash<QString,QVariant> queryParameters;
    queryParameters.insert("operationType",NETWORK_HTTPSTATUS);
    this->sendRequest(url,queryParameters);
}

void Scraper::getUrlRespCodeFinished()
{

}

void Scraper::launchRequestSlot(int type)
{
    this->requestLauncher(type);
}

void Scraper::getUrlRespCodeProcessResults(int statusCode, QString url)
{
    QHash<int, QVariant> columnsData;

    if(_abortCliked)
        return;

    scraperMutex.lock();
    this->countHeader++;
    scraperMutex.unlock();

    emit updateTaskProgressBar(this->countHeader, this->scraptable->scrapResults.size(), tr("Retrieving HTTP status code"));

    // Comme on n'affiche plus les resultats au fur et a mesure, on peut build une stack, ce qui evitera de parcourir l'integralite du tableau de resultats de scrap pour chaque url
    columnsData.insert(mainWin->getColumnIndexByName("httpCol"),statusCode);
    _resultsStack.insert(url,columnsData);
    columnsData.clear();

    if (this->countHeader == this->scraptable->scrapResults.size())
    {
        emit updateMultipleRows(_resultsStack,true);
        this->doAutoTasks(REQUEST_STATUS_CODE);
    }
    else if(startAt < this->scraptable->scrapResults.size())
    {
        scraperMutex.lock();
        ++startAt;
        scraperMutex.unlock();
        this->requestLauncher(1);
    }
}

void Scraper::getBulkBacklinks(QStringList urls)
{
    int i;
    QUrl blQurl;
    QUrlQuery blQueryApi;
    QHash<QString,QVariant> queryParameters;

    i = 0;
    // First we test for Majestic ;)
    if(_blApiForScrap == "Majestic" || _blApiForScrap == "SEObserver")
    {
        foreach (QString url, urls) {
            blQueryApi.addQueryItem("item"+QString::number(i),url);
            i++;
        }
        blQueryApi.addQueryItem("items",QString::number(urls.size()));
    }

   if(_blApiForScrap == "Majestic" || _blApiForScrap == "SEObserver")
    {

       if(_blApiForScrap == "Majestic")
       {
        blQurl.setUrl("http://enterprise.majesticseo.com/api/json");
        blQueryApi.addQueryItem("privatekey",config->readGeneralConfig("ExternalPrivateKeys", "Majestic").toString());
        blQueryApi.addQueryItem("accesstoken",config->getConfigurationValue("majesticAccessToken"));
       }
       else // SEObserver
       {
           qint64 queryTimestamp = QDateTime::currentSecsSinceEpoch();
           QByteArray authSign = QCryptographicHash::hash(QCryptographicHash::hash(
                                   (config->readGeneralConfig("ExternalPrivateKeys", "SEObserver").toString() + QString::number(queryTimestamp)).toLatin1(),
                                   QCryptographicHash::Md5),QCryptographicHash::Sha1);
           blQurl.setUrl("https://api0.seobserver.com/api/json");
           blQueryApi.addQueryItem("app_api_key",config->getConfigurationValue("seobserverApiKey"));
           blQueryApi.addQueryItem("auth_signature",authSign.toHex());
           blQueryApi.addQueryItem("auth_ts",QString::number(queryTimestamp));
           blQueryApi.addQueryItem("auth_partner","RDDZ");

       }
        blQueryApi.addQueryItem("cmd","GetIndexItemInfo");
        blQueryApi.addQueryItem("datasource",config->getConfigurationValue("majesticIndex"));
        blQueryApi.addQueryItem("EnableResourceUnitFailover","1");
    }
    else if(config->getConfigurationValue("blApiForScrap","Free") == "Moz")
    {
        uint expireTime;
        QString mozAccessId,mozSecretKey,stringToSign,urlSafeSignature;
        QByteArray binarySignature;


        QDateTime currentDateTime = QDateTime::currentDateTime();
        mozAccessId = config->getConfigurationValue("mozAccessId");
        mozSecretKey = config->getConfigurationValue("mozSecretKey");
        expireTime = currentDateTime.toTime_t() + 300;
        stringToSign = mozAccessId+"\n"+QString::number(expireTime);
        binarySignature =  QMessageAuthenticationCode::hash(stringToSign.toUtf8(), mozSecretKey.toUtf8(), QCryptographicHash::Sha1);
        urlSafeSignature = QUrl::toPercentEncoding(binarySignature.toBase64());


        blQurl.setUrl("http://lsapi.seomoz.com/linkscape/url-metrics/");
        blQueryApi.addQueryItem("Cols","103079231524");
        blQueryApi.addQueryItem("AccessID",mozAccessId);
        blQueryApi.addQueryItem("Expires",QString::number(expireTime));
        blQueryApi.addQueryItem("Signature",urlSafeSignature);
        // We insert the list of URL on the QHash param
        queryParameters.insert("urlsList",urls);
    }

     blQurl.setQuery(blQueryApi);


    queryParameters.insert("operationType",NETWORK_URL_BL);
    queryParameters.insert("currentAPI",_blApiForScrap);
    this->sendRequest(blQurl.toString(),queryParameters);
}

void Scraper::getBacklinks(QString url)
{
    QUrl blQurl;
    QUrlQuery blQuery;
    QUrl qurl(url);
    QString subdomain,domain, scheme, tld, tmpUrl;

    domain     = qurl.host();
    scheme     = qurl.scheme();
    subdomain  = domain;
    // Bug du tld avec cette url http://www.viva.presse.fr/spip.php?page=forum&id_article=17524
    // presse.fr au lieu de fr
    tld        = qurl.topLevelDomain();
    tmpUrl     = domain.replace(tld,"");
    tmpUrl     = tmpUrl.section('.',-1);
    domain     = (tmpUrl.isEmpty()) ? domain : tmpUrl+tld;
    if(subdomain.count('.')==1)
        subdomain.prepend("www.");

    if (config->getConfigurationValue("blApiForScrap","Free") == "Ahrefs")
    {
        blQurl.setUrl("http://apiv2.ahrefs.com/");
        blQuery.addQueryItem("target",url);
        blQuery.addQueryItem("mode",config->getConfigurationValue("backlinksCheckMode"));
        blQuery.addQueryItem("output","json");
        blQuery.addQueryItem("from","metrics");
        blQuery.addQueryItem("select","backlinks");
        blQuery.addQueryItem("token",config->getConfigurationValue("ahrefsApiKey"));
        blQurl.setQuery(blQuery);
    }

    QHash<QString,QVariant> queryParameters;
    queryParameters.insert("operationType",NETWORK_URL_BL);
    queryParameters.insert("currentAPI",config->getConfigurationValue("blApiForScrap","Free"));
    // On force la redir a true pour init la initalUrl
    queryParameters.insert("redirFound",true);
    queryParameters.insert("initialUrl",url);
    this->sendRequest(blQurl.toString(),queryParameters);
}

void Scraper::getBacklinksFinished()
{

}

void Scraper::getBacklinksProcessResults(QHash<QString,QVariant> results, QString url)
{
    if(_abortCliked)
        return;

    int i;
    QHash<int, QVariant> columnsData;

    if(results.value("API").toString() == "Majestic" || results.value("API").toString() == "SEObserver")
    {
        for(i=0;i<results.value("Results").toHash().value("Urls").toList().size();i++)
        {
            scraperMutex.lock();
            this->countBls++;
            scraperMutex.unlock();

            // WE build the QHash for updating multiple column
            columnsData.clear();
            columnsData.insert(mainWin->getColumnIndexByName("blCol"),results.value("Results").toHash().value("ExtBackLinks").toList().at(i));
            columnsData.insert(mainWin->getColumnIndexByName("rdCol"),results.value("Results").toHash().value("RefDomains").toList().at(i));
            columnsData.insert(mainWin->getColumnIndexByName("cfCol"),results.value("Results").toHash().value("CitationFlow").toList().at(i));
            columnsData.insert(mainWin->getColumnIndexByName("tfCol"),results.value("Results").toHash().value("TrustFlow").toList().at(i));
            columnsData.insert(mainWin->getColumnIndexByName("ttftCol"),results.value("Results").toHash().value("TopicalTrustFlow_Topic_0").toList().at(i));
            columnsData.insert(mainWin->getColumnIndexByName("ttfvCol"),results.value("Results").toHash().value("TopicalTrustFlow_Value_0").toList().at(i));
            columnsData.insert(mainWin->getColumnIndexByName("majesticStatusCol"),results.value("Results").toHash().value("Status").toList().at(i));
            columnsData.insert(mainWin->getColumnIndexByName("lcdCol"),results.value("Results").toHash().value("LastCrawlDate").toList().at(i));


            if(getBacklinksMethod) // Not URL
            {
                emit updateMultipleCol(results.value("Results").toHash().value("Urls").toList().at(i).toString(),columnsData,false);
            }
            else // URL
            {
                emit updateMultipleCol(results.value("Results").toHash().value("Urls").toList().at(i).toString(),columnsData,true);
            }

        }
    }
    else if (results.value("API").toString() == "Moz")
    {
        for(i=0;i<results.value("Results").toHash().value("Urls").toList().size();i++)
        {
            scraperMutex.lock();
            this->countBls++;
            scraperMutex.unlock();

            // WE build the QHash for updating multiple column
            columnsData.clear();
            columnsData.insert(mainWin->getColumnIndexByName("blCol"),results.value("Results").toHash().value("ueid").toList().at(i));
            columnsData.insert(mainWin->getColumnIndexByName("paCol"),results.value("Results").toHash().value("upa").toList().at(i));
            columnsData.insert(mainWin->getColumnIndexByName("daCol"),results.value("Results").toHash().value("pda").toList().at(i));
            columnsData.insert(mainWin->getColumnIndexByName("mozRankCol"),results.value("Results").toHash().value("umrp").toList().at(i));

            if(getBacklinksMethod)
            {
                emit updateMultipleCol(results.value("Results").toHash().value("Urls").toList().at(i).toString(),columnsData,false);
            }
            else
            {
                emit updateMultipleCol(results.value("Results").toHash().value("Urls").toList().at(i).toString(),columnsData,true);
            }
        }
    }
    else if (results.value("API").toString() == "Ahrefs")
    {
        scraperMutex.lock();
        this->countBls++;
        scraperMutex.unlock();

         emit updateTableViewSignal(url,results.value("backlinks"),mainWin->getColumnIndexByName("blCol"));
    }

     emit updateTaskProgressBar(this->countBls, this->scraptable->scrapResults.size(), tr("Retrieving backlinks"));

    // Si le scrap est abort, on return
    if(_scrapRunning == false)
        return;

    if (this->countBls == this->scraptable->scrapResults.size())
        this->doAutoTasks(REQUEST_BACKLINKS);
    else if(startAt < this->scraptable->scrapResults.size())
    {
        scraperMutex.lock();
        ++startAt;
        scraperMutex.unlock();

        this->requestLauncher(4);
    }
}


double Scraper::getExternalLinksCount(QString src, QString url, bool dofollowPercent)
{
    QString domain, XPathQuery;
    QStringList output;
    QUrl qurl(url);
    double result;

    result = 0;
    domain = qurl.scheme()+"://"+qurl.host();

    if(src.isEmpty())
        return result;

    // First for dofollow, check the meta robot nofollow
     if(dofollowPercent == true)
     {
        output = getXpath(src,"//meta[@name=\"robots\" and contains(@content,\"nofollow\")]/@content","","",url);
        if(output.isEmpty())
            return 0;
        output.clear();
     }


    if(dofollowPercent == false)
        XPathQuery = "count(//a[not(starts-with(@href,\""+domain+"\")) and (starts-with(@href,\"http://\") or starts-with(@href,\"https://\"))])";
    else
        XPathQuery = "concat(count(//a[not(starts-with(@href,\""+domain+"\")) and (starts-with(@href,\"http://\") or starts-with(@href,\"https://\"))]),\",\",count(//a[not(starts-with(@href,\""+domain+"\")) and @rel=\"nofollow\" and (starts-with(@href,\"http://\") or starts-with(@href,\"https://\"))]))";


    output = getXpath(src,XPathQuery,"","",url);
    if(!output.isEmpty())
    {
        if(dofollowPercent == true)
        {
            QStringList results;
            double nofollow = 0, countLinks = 0;

            results = output.at(0).split(",");
            if(results.size() == 2)
            {
                countLinks = results.at(0).toDouble();
                nofollow = results.at(1).toDouble();
                result = (!countLinks) ? 0 : ((countLinks-nofollow) * 100) / countLinks;
            }
        }
        else
            result = output.at(0).toDouble();
    }
    return result;
}

QString Scraper::replaceCustom(QString custom, int encoded)
{
    QString substituteCustom = custom;

    if(tools->isGoogleUrl(custom))
        _googleUrlOnCustom = true;

    if(encoded) // custom is encoded
        substituteCustom = QUrl::toPercentEncoding(custom,"%");
    else
    {
        QUrl custom1Qurl(custom);
        if(custom1Qurl.isValid() && !custom1Qurl.isRelative())
            substituteCustom = custom1Qurl.toEncoded(); 
    }

    // Si on a un custom2, on le remet comme il doit etre
    if(substituteCustom.contains("%7B%25custom2%25%7D"))
        substituteCustom.replace("%7B%25custom2%25%7D", "{%custom2%}");
    return substituteCustom;
}



void Scraper::checkDofollowNofollow(QString url)
{
    QHash<QString,QVariant> queryParameters;
    queryParameters.insert("operationType",NETWORK_DOFOLLOW);;
    this->sendRequest(url,queryParameters);
}

void Scraper::checkDofollowNofollowFinished()
{

}

void Scraper::checkDofollowNofollowProcessResults(double rel, QString url)
{
    QHash<int, QVariant> columnsData;
    if(_abortCliked)
        return;

    scraperMutex.lock();
    this->countRel++;
    scraperMutex.unlock();
    emit updateTaskProgressBar(this->countRel, this->scraptable->scrapResults.size(), tr("Getting dofollow"));

    columnsData.insert(mainWin->getColumnIndexByName("dfCol"),rel);
    _resultsStack.insert(url,columnsData);
    columnsData.clear();

    if (this->countRel == this->scraptable->scrapResults.size())
    {
        emit updateMultipleRows(_resultsStack,true);
        this->doAutoTasks(REQUEST_DOFOLLOW);
    }
    else if(startAt < this->scraptable->scrapResults.size())
    {
        scraperMutex.lock();
        ++startAt;
        scraperMutex.unlock();
        this->requestLauncher(2);
    }
}


void Scraper::getObl(QString url)
{
    QHash<QString,QVariant> queryParameters;
    queryParameters.insert("operationType",NETWORK_OBL);;
    this->sendRequest(url,queryParameters);
}

void Scraper::getOblFinished()
{

}

void Scraper::getOblProcessResults(double obl, QString url)
{

    QHash<int, QVariant> columnsData;
    if(_abortCliked)
        return;

    scraperMutex.lock();
    this->countObl++;
    scraperMutex.unlock();

    emit updateTaskProgressBar(this->countObl, this->scraptable->scrapResults.size(), tr("Getting OBL"));

    // Comme on n'affiche plus les resultats au fur et a mesure, on peut build une stack, ce qui evitera de parcourir l'integralite du tableau de resultats de scrap pour chaque url
    columnsData.insert(mainWin->getColumnIndexByName("oblCol"),obl);
    _resultsStack.insert(url,columnsData);
    columnsData.clear();

    // Ne pas oublier sur le abort d'afficher de faire le updateMultipleRows si le _resultsStack n'est pas vide.

    if (this->countObl == scraptable->scrapResults.size())
    {
        emit updateMultipleRows(_resultsStack,true);
        this->doAutoTasks(REQUEST_OBL);
    }
    else if(startAt < scraptable->scrapResults.size())
    {
        scraperMutex.lock();
        ++startAt;
        scraperMutex.unlock();
        this->requestLauncher(5);
    }
}

void Scraper::getIpAddr(QString url)
{
    QtConcurrent::run(this, &Scraper::getIpAddrProcessResults, url);
}


void Scraper::getIpAddrProcessResults(QString url)
{
    QHash<int, QVariant> columnsData;
    if(_abortCliked)
        return;


    QHostInfo info;
    QUrl      qurl;
    QList<QHostAddress>  ipAddresses;
    QString ipAddress;



    ipAddress = "Unknow";
    qurl.setUrl(url);
    info = QHostInfo::fromName(qurl.host());

    scraperMutex.lock();
    this->countIp++;
    scraperMutex.unlock();

    if(!info.error())
    {
        ipAddresses = info.addresses();
        if(ipAddresses.size())
            ipAddress = ipAddresses.at(0).toString();
    }
    else
        emit appendLog(tr("Unable to retrieve IP address for host : %1").arg(url),"error");

    columnsData.insert(mainWin->getColumnIndexByName("ipCol"),ipAddress);
    _resultsStack.insert(url,columnsData);
    columnsData.clear();


     emit updateTaskProgressBar(this->countIp, this->scraptable->scrapResults.size(), tr("Retrieving IP addresses"));

    if (this->countIp == this->scraptable->scrapResults.size())
    {
        emit updateMultipleRows(_resultsStack,true);
        this->doAutoTasks(REQUEST_IP_ADDR);
    }
    else if(startAt < this->scraptable->scrapResults.size())
    {
        scraperMutex.lock();
        ++startAt;
        scraperMutex.unlock();
        this->requestLauncher(6);
    }
}

void Scraper::getLinkAliveMapper(QString url, QString blFor)
{
    QHash<QString,QVariant> queryParameters;
    queryParameters.insert("operationType",NETWORK_LINK_ALIVE);
    queryParameters.insert("realUrl",url);
    queryParameters.insert("searchForUrl",blFor);
    queryParameters.insert("redirFound",true);
    queryParameters.insert("initialUrl",url+"~|~"+blFor);
    this->sendRequest(url,queryParameters);
}

void Scraper::getLinkAlive(QString url)
{
    QHash<QString,QVariant> queryParameters;
    queryParameters.insert("operationType",NETWORK_LINK_ALIVE);
    queryParameters.insert("realUrl",url);
    queryParameters.insert("searchForUrl",this->footprint);
    queryParameters.insert("currentAction",this->currentAction);
    this->sendRequest(url,queryParameters);
}

void Scraper::getLinkAliveFinished()
{
    QNetworkReply *r = qobject_cast<QNetworkReply*>(sender());
    QString str(r->readAll());
    int httpstatus = r->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

    if(r)
    {
        emit deleteReplyFromQhash(r->property("realUrl").toString());
        if (httpstatus >= 300)
            str = "error : "+QString::number(httpstatus);
        QtConcurrent::run(this, &Scraper::getLinkAliveProcessResults, str,
                          r->property("realUrl").toString());
        disconnectReply(r, NETWORK_LINK_ALIVE);
        r->deleteLater();
        r->manager()->deleteLater();
    }
}

void Scraper::getLinkAliveProcessResults(QString status, QString url)
{
    if(_abortCliked)
        return;

    scraperMutex.lock();
    this->countLinkAlive++;
    scraperMutex.unlock();

    emit updateTaskProgressBar(this->countLinkAlive, this->scraptable->scrapResults.size(), tr("Link Alive check"));
    // We have to update if 2 columns are OK
    if(mainWin->getCurrentAction().contains("scrap", Qt::CaseInsensitive)) // Change for scrap and customScrap
        emit updateLinkAlive(url, status);
    else
    {
        QPair<QString,QString> firstPair;
        QPair<QString, QString> secondPair;

        firstPair.first = "urlCol";
        firstPair.second = url.split("~|~").at(0);
        secondPair.first = "blForCol";
        secondPair.second = (url.split("~|~").size() > 1) ? url.split("~|~").at(1) : "";
        emit updateTableViewMappingSignal(firstPair, secondPair, status, mainWin->getColumnIndexByName("linkAliveCol"));
    }

    if (this->countLinkAlive == scraptable->scrapResults.size())
        this->doAutoTasks(REQUEST_LINK_ALIVE);
    else if(startAt < scraptable->scrapResults.size())
    {
        scraperMutex.lock();
        ++startAt;
        scraperMutex.unlock();
        this->requestLauncher(REQUEST_LINK_ALIVE);
    }

}

void Scraper::getPlatform(QString url){
    QHash<QString,QVariant> queryParameters;
    queryParameters.insert("operationType",NETWORK_PLATFORM);
    this->sendRequest(url,queryParameters);
}

void Scraper::getPlatformFinished()
{

}

void Scraper::getPlatformProcessResults(QString platformString, QString url)
{
    if(_abortCliked)
        return;

    scraperMutex.lock();
    this->countPlatform++;
    scraperMutex.unlock();

    emit updateTaskProgressBar(this->countPlatform, this->scraptable->scrapResults.size(), tr("Platform check"));
    emit updatePlatform(url, platformString);


    if (this->countPlatform == scraptable->scrapResults.size())
        this->doAutoTasks(REQUEST_PLATFORM);
    else if(startAt < scraptable->scrapResults.size())
    {
        scraperMutex.lock();
        ++startAt;
        scraperMutex.unlock();
        this->requestLauncher(REQUEST_PLATFORM);
    }
}

void Scraper::getSingleWhois(QString domain)
{
    QUrl mashapeApiUrl("https://domainr.p.mashape.com/v2/status");
    QUrlQuery mashapeQueryApi;
    QUrl qurl("http://"+domain);
    QString currentTld = qurl.topLevelDomain().remove(0,1);

    mashapeQueryApi.addQueryItem("mashape-key",_whoisApiKey);
    mashapeQueryApi.addQueryItem("domain",domain);
    mashapeApiUrl.setQuery(mashapeQueryApi);

    QHash<QString,QVariant> queryParameters;
    queryParameters.insert("operationType",NETWORK_WHOIS);
    // On force la initalUrl au domain
    queryParameters.insert("redirFound",true);
    queryParameters.insert("initialUrl",domain);
    queryParameters.insert("apiKey",_whoisApiKey);
    queryParameters.insert("currentAPI",_whoisApiService);



    // On check si l'extension est supportee par l'API
    if(!currentTld.isEmpty() && Tools::extensionSupportedByApi(currentTld, "mashape"))
    this->sendRequest(mashapeApiUrl.toString(),queryParameters);
    else
        QtConcurrent::run(this, &Scraper::getWhoisProcessResults, _tldNotSupportedMsg, domain);

}

void Scraper::getMultipleWhois(QUrlQuery apiQurlQuery)
{
    // Dynadot part
    QUrl dynadotApiUrl("https://api.dynadot.com/api2.html");
    apiQurlQuery.addQueryItem("command","search");
    apiQurlQuery.addQueryItem("key",_whoisApiKey);
    dynadotApiUrl.setQuery(apiQurlQuery);


    QHash<QString,QVariant> queryParameters;
    queryParameters.insert("operationType",NETWORK_WHOIS);
    // On force la initalUrl au domain
    queryParameters.insert("redirFound",true);
    queryParameters.insert("apiKey",_whoisApiKey);
    queryParameters.insert("currentAPI",_whoisApiService);
    // Pour dynadot il faut Auth l'ip, du coup pas de proxies !!
    queryParameters.insert("proxyType","none");

    this->sendRequest(dynadotApiUrl.toString(),queryParameters);
}

void Scraper::getWhoisFinished()
{
    QNetworkReply *r = qobject_cast<QNetworkReply*>(sender());
    QString str(r->readAll());

    if(r)
    {
        emit deleteReplyFromQhash(r->property("realUrl").toString());
        QtConcurrent::run(this, &Scraper::getWhoisProcessResults, str,
                          r->property("realUrl").toString());
        disconnectReply(r, NETWORK_WHOIS);
        r->deleteLater();
        r->manager()->deleteLater();
    }
}

void Scraper::getWhoisProcessResults(QString isAvailable, QString url)
{
    if(_abortCliked)
        return;

    scraperMutex.lock();
    this->countWhois++;
    scraperMutex.unlock();

    emit updateTaskProgressBar(this->countWhois, this->urlList.size(), tr("Domain available check"));
    emit updateDomainAvailable(url, isAvailable,mainWin->getColumnIndexByName("domainAvailableCol"));

    if (this->countWhois == this->urlList.size())
    {
        this->doAutoTasks(REQUEST_WHOIS);
    }
    else if(startAt < this->urlList.size())
    {
        if(_whoisApiService == "Dynadot")
        {
            if(isAvailable != _tldNotSupportedMsg)
            {
            _dynadotResponsesOk++;
            //++startAt;
            if(!(_dynadotResponsesOk % (interval)))
                this->requestLauncher(REQUEST_WHOIS);
            }
        }
        else
        {
        scraperMutex.lock();
        ++startAt;
        scraperMutex.unlock();
        this->requestLauncher(REQUEST_WHOIS);
        }
    }
}

void Scraper::doAutoTasks(int type, bool silent)
{
    switch(type){
    case REQUEST_STATUS_CODE :
        if(!silent)
            emit appendLog(tr("Get HTTP Response code finished"));
        emit scrapButtonEnabled();
        if (config->getAutoRemoveBadUrl() && this->mainWin->getColumnIndexByName("httpCol") != -1)
            emit removeBadUrl();
        if (config->getAutoResolveRedir() && this->mainWin->getRedirUrls().count() != 0 && this->mainWin->getColumnIndexByName("httpCol") != -1)
            emit autoResolveRedirect();
        else if(config->getAutoDF() && this->mainWin->getColumnIndexByName("dfCol") != -1)
            emit autoCheckDofollowNofollow();
        else if(config->getAutoBL() && this->mainWin->getColumnIndexByName("blCol") != -1)
            emit autoBacklinks();
        else if(config->getAutoOBL() && this->mainWin->getColumnIndexByName("oblCol") != -1)
            emit autoObl();
        else if(config->getAutoIP() && this->mainWin->getColumnIndexByName("ipCol") != -1)
            emit autoIp();
        break;
    case REQUEST_REDIRECTION :
        if(!silent)
        emit appendLog(tr("Resolve redirections finished"));
        emit removeDuplicateUrl();
        emit scrapButtonEnabled();
        if (config->getAutoRemoveBadUrl() && this->mainWin->getColumnIndexByName("httpCol") != -1)
            emit removeBadUrl();
        if (config->getAutoResolveRedir() && this->scraptable->getNbItemsByCol(this->scraptable->columnIndexes.value("httpCol"), true) != 0 && scraptable->redirRetry <= 5 && config->readGeneralConfig("StdColumn","HTTP",true).toBool())
            emit autoResolveRedirect();
        else if(config->getAutoDF() && this->mainWin->getColumnIndexByName("dfCol") != -1)
            emit autoCheckDofollowNofollow();
        else if(config->getAutoBL() && this->mainWin->getColumnIndexByName("blCol") != -1)
            emit autoBacklinks();
        else if(config->getAutoOBL() && this->mainWin->getColumnIndexByName("oblCol") != -1)
            emit autoObl();
        else if(config->getAutoIP() && this->mainWin->getColumnIndexByName("ipCol") != -1)
            emit autoIp();
        break;
    case REQUEST_BACKLINKS :
        if(!silent)
            emit appendLog(tr("Check backlinks finished"));
        emit scrapButtonEnabled();
        if(config->getAutoOBL() && this->mainWin->getColumnIndexByName("oblCol") != -1)
            emit autoObl();
        else if(config->getAutoIP() && this->mainWin->getColumnIndexByName("ipCol") != -1)
            emit autoIp();
        break;
    case REQUEST_DOFOLLOW :
        if(!silent)
            emit appendLog(tr("Check Dofollow/Nofollow finished"));
        emit scrapButtonEnabled();
        if(config->getAutoBL() && this->mainWin->getColumnIndexByName("blCol") != -1)
            emit autoBacklinks();
        else if(config->getAutoOBL() && this->mainWin->getColumnIndexByName("oblCol") != -1)
            emit autoObl();
        else if(config->getAutoIP() && this->mainWin->getColumnIndexByName("ipCol") != -1)
            emit autoIp();
        break;
    case REQUEST_OBL :
        if(!silent)
            emit scrapButtonEnabled();
        emit appendLog(tr("Check outbound links finished"));
        if(config->getAutoIP() && this->mainWin->getColumnIndexByName("ipCol") != -1)
            emit autoIp();
        break;
    case REQUEST_IP_ADDR :
        if(!silent)
            this->stopIpTimer();
        emit scrapButtonEnabled();
        emit appendLog(tr("Get IP adresses finished"));
        break;
    case REQUEST_LINK_ALIVE :
        if(!silent)
            emit appendLog(tr("Check link alive finished"));
        emit scrapButtonEnabled();
        break;
    case REQUEST_PLATFORM :
        if(!silent)
            emit appendLog(tr("Platform check finished"));
        emit scrapButtonEnabled();
        break;
    case REQUEST_WHOIS :
        if(!silent)
            emit appendLog(tr("Domains available check finished"));
        emit scrapButtonEnabled();
        break;
    }
}


void Scraper::requestLauncher(int type)
{
    int i, nextStep;

    typedef void(Scraper::*networkFunc)(QString);
    QMap<int,networkFunc> funcMap;

    // Do the mapping with the #define declare into mainwindow.h
    funcMap.insert(1,&Scraper::getUrlRespCode);
    funcMap.insert(2,&Scraper::checkDofollowNofollow);
    funcMap.insert(3,&Scraper::resolveRedirections);
    funcMap.insert(4,&Scraper::getBacklinks);
    funcMap.insert(5,&Scraper::getObl);
    funcMap.insert(6,&Scraper::getIpAddr);
    funcMap.insert(7,&Scraper::getLinkAlive);
    funcMap.insert(8,&Scraper::getPlatform);
    funcMap.insert(9,&Scraper::getSingleWhois);


    networkFunc currentNetworkFunc = funcMap.value(type);

    if(_scrapRunning == false)
        return;


    if(!startAt)
    {
    scrapurlsSize = urlList.size();
    _threadsAlreadyLaunched = 0;
    _threadsTimedOut = 0;
    }

    if(type != REQUEST_LINK_ALIVE)
    this->footprint.clear();

    if(!this->urlList.size() && !this->urlMapping.size())
    {
        qDebug() << "UrlList is empty";
        this->doAutoTasks(type,true);
        return;
    }

    // Pour les link alive sur une recup de backlinks
    if(mainWin->getCurrentAction() == "backlinks" && type == REQUEST_LINK_ALIVE)
    {
        if (startAt == 0)
        {
            nextStep = (startAt+interval < urlMapping.size()) ?  startAt+interval : urlMapping.size();
            for(i=startAt;i < nextStep;i++)
                this->getLinkAliveMapper(urlMapping.value(i).at(0), urlMapping.value(i).at(1));
            scraperMutex.lock();
            startAt = i - 1;
            scraperMutex.unlock();
        }
        else if (startAt < urlMapping.size())
        {
            this->getLinkAliveMapper(urlMapping.value(startAt).at(0), urlMapping.value(startAt).at(1));
        }
    }
   else if(type == REQUEST_WHOIS && _whoisApiService == "Dynadot") // Pour le whois dynadot, on build une liste de "interval" domaines
    {
        QUrlQuery dynadotApiQuery;

        int domainLoop;
        domainLoop = 0;

        nextStep = (startAt+interval < urlList.size()) ?  startAt+interval : urlList.size();
        for(i=startAt;domainLoop < interval && i < urlList.size();i++)
        {
            QUrl qurl("http://"+urlList.at(i));
            QString currentTld = qurl.topLevelDomain().remove(0,1);
            QString currentUrl = urlList.at(i);

            if(!currentTld.isEmpty() && Tools::extensionSupportedByApi(currentTld, "dynadot"))
            {
                dynadotApiQuery.addQueryItem("domain"+QString::number(domainLoop),urlList.at(i));
                domainLoop++;
            }
            else
            {
                QtConcurrent::run(this, &Scraper::getWhoisProcessResults, _tldNotSupportedMsg, currentUrl);
            }
        }

        this->getMultipleWhois(dynadotApiQuery);
        scraperMutex.lock();
        startAt = i - 1;
        scraperMutex.unlock();
    }
    else if(type == REQUEST_BACKLINKS && _blApiForScrap != "Ahrefs") // Pour les bls Moz et Majestic, on peut faire du Bulk
    {
        QStringList urlsToCheck;
        nextStep = (startAt+interval < urlList.size()) ?  startAt+interval : urlList.size();
        urlsToCheck.clear();

        for(i=startAt;i < nextStep;i++)
        urlsToCheck.append(urlList.at(i));

        scraperMutex.lock();
        startAt = i - 1;
        scraperMutex.unlock();
        this->getBulkBacklinks(urlsToCheck);
    }
    else
    {
        if (startAt == 0)
        {
            nextStep = (startAt+interval < urlList.size()) ?  startAt+interval : urlList.size();
            for(i=startAt;i < nextStep;i++)
            {
                Scraper* obj = static_cast<Scraper*>( this );
                (*obj.*currentNetworkFunc)(urlList.at(i));
            }
            scraperMutex.lock();
            startAt = i - 1;
            scraperMutex.unlock();
        }
        else if (startAt < urlList.size())
        {
            Scraper* obj = static_cast<Scraper*>( this );
            (*obj.*currentNetworkFunc)(urlList.at(startAt));
        }
    }
}


QStringList Scraper::getNextScrapUrl()
{
    QStringList scrapurl;

    if(scrapurls.size())
    {
        scraperMutex.lock();
        scrapurl = scrapurls.takeAt(0);
        scraperMutex.unlock();
    }
    else
        scrapurl.append("");
    return scrapurl;
}

void Scraper::initScrapVars()
{
    _scrapRunning = true;
    _threadsAlreadyLaunched = 0;
    _proxyType = "none";
    _uaUsed = tools->randomUa(_uaList);
    // Gestion des proxies
    _proxies = (config->proxy_enable()) ? config->get_proxies() : QStringList();
    _proxyType = _proxies.isEmpty() ? "none" : "default";

    if(_proxyType != "none")
        tools->allProxies = _proxies;
}

void Scraper::launchScrap()
{
    QRegExp rxFp("\\{%footprint%\\}"), rxC1("\\{%custom1(::encoded)?%\\}"), rxC2("\\{%custom2(::encoded)?%\\}");
    QString scrapurl, scrapurl1, scrapurl2;
    QStringList scrapData;
    QByteArray ua;
    int i,j;

    // Setting all scrap config here !!
     ua = (!this->seSelected.value("specificUa","").toString().isEmpty()) ? this->seSelected.value("specificUa","").toByteArray() : tools->randomUa(config->get_ua("standard"));
    _scrapRunning = true;
    proxyUsed = "";
    this->resultsFound = 0;
    _threadsAlreadyLaunched = 0;
    scrapurls.clear();

    _proxyType = "none";
    // Global Configuration
    _retryOnTimeout = QVariant(config->getConfigurationValue("retryOnTimeout")).toBool();
    _minimalLogs = QVariant(config->getConfigurationValue("minimalLog")).toBool();
    _proxies = (config->proxy_enable()) ? config->get_proxies() : QStringList();
    _proxyType = _proxies.isEmpty() ? "none" : "default";
    _useProxyProviderRotation = config->useProxyRotating();

    // Scrap Engine configuration
    _scrapPause = this->seSelected.value("pause",0).toInt();
    _scrapPausePagination = this->seSelected.value("pausePagination",0).toBool();
    _xpathInterpreterIndex = this->seSelected.value("xpathInterpreter",0).toInt();
    _seJsonStructure = this->seSelected.value("jsonStructure").toString();
    htmlEncoded = this->seSelected.value("keepHtmlEncode").toBool();
    _resendReqStr = this->seSelected.value("resendReqStr").toString();
    _resendReqStrNotContains = this->seSelected.value("resendReqStrNotContains").toInt();
    _resendReqStrAction = this->seSelected.value("resendReqStrAction").toInt();
    _resendHTTP = this->seSelected.value("resendHTTP").toInt();
    _resendHTTPCondition = this->seSelected.value("resendHTTPCondition").toInt();
    _resendHTTPCode = this->seSelected.value("resendHTTPCode").toInt();
    _resendHTTPAction = this->seSelected.value("resendHTTPAction").toInt();
    _forceOutputEncoding = this->seSelected.value("forceOutputEncoding").toString();

    _httpLogFilename = QApplication::applicationDirPath()+"/httpLogs-"+QDateTime::currentDateTime().toString("yyyy-MM-dd")+".txt";


    if(_proxyType != "none")
        tools->allProxies = _proxies;

    scrapurl = this->seSelected.value("url").toString().trimmed().simplified();
    scrapUrlHasCustoms = scrapurl.contains(QRegExp("\\{%custom[12](::encoded)?%\\}"));

    // First we delete all cookies file
    emit appendLog(tr("Clearing cookie files ..."),"information");
    tools->removeFilesByExt(_tmpStackDirectory,"*.rddz");
    emit appendLog(tr("Cookie files cleared successfully"),"information");

    // And now we delete pic files
    emit appendLog(tr("Clearing temporary files ..."),"information");
    tools->removeFilesByExt(_tmpStackDirectory,"*rddz.png");
    tools->removeFilesByExt(_tmpStackDirectory,"rddzStack*");
    emit appendLog(tr("Temporary files cleared successfully"),"information");


    // footprint alone or no footprint (c1 && c2 is empty)
    if ((!this->footprint.isEmpty() || this->footprint.isEmpty()) && this->custom1List.isEmpty() && this->custom2List.isEmpty())
    {
        if(rxFp.indexIn(scrapurl) != -1)
            scrapurl.replace("{%footprint%}",this->footprint);
        scrapData.clear();
        scrapData.append(scrapurl);
        scrapData.append("");
        scrapData.append("");
        scrapurls.append(scrapData);
    }
    else
    {
        // footprint + anything else (c1 or c2 or c1 & c2)
        if (!this->footprint.isEmpty())
        {
            if(rxFp.indexIn(scrapurl) != -1)
                scrapurl.replace("{%footprint%}",this->footprint);
        }
        // else if no footprint used, dont touch scrapurl and process c1 / c2 / c1 + c2
        if (!this->custom1List.isEmpty())
        {
            for (i = 0; i<custom1List.size(); i++)
            {
                scrapurl1 = scrapurl; scrapData.clear();
                if(rxC1.indexIn(scrapurl1) != -1)
                    scrapurl1.replace(QRegExp("\\{%custom1(::encoded)?%\\}"), this->replaceCustom(this->custom1List[i], rxC1.cap(1).size()));
                if (!this->custom2List.isEmpty())
                {
                    for (j = 0; j<custom2List.size(); j++)
                    {
                        scrapurl2 = scrapurl1; scrapData.clear();
                        if(rxC2.indexIn(scrapurl2) != -1)
                            scrapurl2.replace(QRegExp("\\{%custom2(::encoded)?%\\}"), this->replaceCustom(this->custom2List[j], rxC2.cap(1).size()));

                        scrapData.append(scrapurl2);
                        scrapData.append(custom1List[i]);
                        scrapData.append(custom2List[j]);
                        scrapurls.append(scrapData);
                    }
                }
                else
                {
                    scrapData.append(scrapurl1);
                    scrapData.append(custom1List[i]);
                    scrapData.append("");
                    scrapurls.append(scrapData);
                }
            }
        }
        else
        {
            // c2 alone (or with fp)
            for (j = 0; j<custom2List.size(); j++)
            {
                scrapurl2 = scrapurl; scrapData.clear();
                if(rxC2.indexIn(scrapurl2) != -1)
                    scrapurl2.replace(QRegExp("\\{%custom2(::encoded)?%\\}"), this->replaceCustom(this->custom2List[j], rxC2.cap(1).size()));
                scrapData.append(scrapurl2);
                scrapData.append("");
                scrapData.append(custom2List[j]);
                scrapurls.append(scrapData);
            }
        }
    }

    scrapurlsSize = initScrapUrlSize = scrapurls.size();

    if(scrapUrlHasCustoms)
        emit updateCustomRatio(scrapurlsSize);

    // Safe value
    _parallelThreads = (config->readGeneralConfig("Global","pcustomthreads").toInt()) ? config->readGeneralConfig("Global","pcustomthreads").toInt() : QThread::idealThreadCount();


    // Si on a pas de pagination et qu'on use pas phantom, on pousse a 10x le nombre de coeurs
    if(_xpathInterpreterIndex && !this->jsEnable)
        _parallelThreads = QThread::idealThreadCount() * 10;


//     If nb proxies < parallelThreads => parallelThreads = nbProxies;
//     for Google only
    if ((Tools::isGoogleUrl(scrapurl) || _googleUrlOnCustom) && !config->useProxyRotating())
    {
        if(config->getNbProxies()  && (config->getNbProxies() < _parallelThreads))
            _parallelThreads = config->getNbProxies();
        else if (!config->getNbProxies())
            _parallelThreads = 1;
    }

    if(config->fullDebug && config->getConfigurationValue("minimalLog") == "false")
        emit appendLog("fullDebug[Scraper][ParallelCustomThreads] : " + QString::number(_parallelThreads), "default");

    // New
    this->nextUrlIndex = (this->urlList.size() < _parallelThreads) ? this->urlList.size() : _parallelThreads;
    this->threadTimeout = config->getConfigurationValue("scraperTimeout").toInt();
    _working = true;


    this->keepStackFiles = QVariant(config->getConfigurationValue("htmlStack")).toBool();
    // If UrlList > 20 on ne keep pas la stack
    if(scrapurlsSize > 20)
        this->keepStackFiles = false;

    emit appendLog(tr("Setting parallel threads to %1").arg(QString::number(_parallelThreads)),"information");

    // On build la subScrapUrls en fonction du nb de threads
    _subScrapNb = (_subScrapNb < scrapurlsSize) ? _subScrapNb : scrapurlsSize;
    _subScrapUrls = scrapurls.mid(0,_subScrapNb);

    for (i = 0;i<_parallelThreads;i++)
    {
        if(_scrapRunning == false)
            break;

        if (scrapurls.isEmpty())
            break;
        scrapData = getNextScrapUrl();
        if(scrapData.size() == 3)
        {
            scrapurl = scrapData[0];


            QHash<QString,QString> phantomArgs;
            scraperMutex.lock();
            phantomArgs.clear();
            phantomArgs.insert("url",scrapurl);
            phantomArgs.insert("custom1",scrapData[1]);
            phantomArgs.insert("custom2",scrapData[2]);
            phantomArgs.insert("userAgent",ua);
            scraperMutex.unlock();

            // Si pagination, on set la progressBar en mode va et vient
            if(!this->seSelected.value("xpath_pagination").toString().isEmpty())
                emit updateTaskProgressBar(0,0,tr("Scrap using engine : %1").arg(this->seSelected.value("display_name").toString()));

            if(this->jsEnable)
                emit runPhantomScrap(phantomArgs);
            else
            {
                QHash<QString,QVariant> requestArgs;
                requestArgs.insert("operationType",NETWORK_SCRAP);
                requestArgs.insert("userAgent",ua);
                requestArgs.insert("realUrl",scrapurl);
                requestArgs.insert("custom1",scrapData[1]);
                requestArgs.insert("custom2",scrapData[2]);

                this->sendRequest(scrapurl, requestArgs);
            }
        }
    }
}

void Scraper::updateNbOfThreadsLaunched()
{
    scraperMutex.lock();
    _threadsAlreadyLaunched++;
    scraperMutex.unlock();
}

void Scraper::updatePairProxyCookies(const QString &proxy, const QVariant &cookie)
{
   scraperMutex.lock();
   this->_cookiesForProxy.insert(proxy,cookie);
   scraperMutex.unlock();
}

void Scraper::updateBannedProxies(QString proxy, bool banned)
{
    if(banned)
        tools->proxiesBannedIndex.append(tools->getProxyIndex(proxy));
    else
        tools->proxiesBannedIndex.removeOne(tools->getProxyIndex(proxy));
}

void Scraper::updateItemsFound(int itemsFound)
{
    scraperMutex.lock();
    int totalItemsFound = this->scraptable->getScrapResultSize() + itemsFound;
    scraperMutex.unlock();
    emit updateNbItems(totalItemsFound);
}

void Scraper::sendRequest(const QString &url, QHash<QString, QVariant> params)
{
    int type = params.value("operationType",NETWORK_SCRAP).toInt();
    QHash<QString, QVariant> threadParameters;

    if(_scrapRunning == false)
        return;

    // On ne set pas d'UA pour les backlinks
    if(type == NETWORK_URL_BL)
        _uaUsed = "Mozilla/5.0 (Windows NT 6.1) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/41.0.2228.0 Safari/537.36";
    else
        _uaUsed = params.value("userAgent","").toByteArray();

    thread = new QThread();
    worker = new Worker(this->threadTimeout, tools);


    worker->moveToThread(thread);
    connect(worker, SIGNAL(scrapThreadReady()), thread, SLOT(start()));
    connect(thread, SIGNAL(started()), worker, SLOT(createRequest()));

    connect(worker, SIGNAL(finished()), thread, SLOT(quit()), Qt::DirectConnection);
    connect(worker, SIGNAL(finished()), worker, SLOT(deleteLater()));
    connect(thread, SIGNAL(finished()), thread, SLOT(deleteLater()));

    // Connect the timer to cause the thread to timeout
    connect(worker, SIGNAL(threadTimeout(QString,QHash<QString,QVariant>)), this, SLOT(threadTimeoutSlot(QString,QHash<QString,QVariant>)));

    switch(type){
    case NETWORK_SCRAP :
        connect(worker, &Worker::sourceCodeParsed, this, &Scraper::interpretScrapResults);
        break;
    case NETWORK_HTTPSTATUS :
        connect(worker, &Worker::sendHttpStatusCode, this, &Scraper::getUrlRespCodeProcessResults);
        break;
    case NETWORK_REDIR :
        connect(worker, &Worker::sendRedirection, this, &Scraper::resolveRedirectionsProcessResults);
        break;
    case NETWORK_OBL :
        connect(worker, &Worker::sendObl, this, &Scraper::getOblProcessResults);
        break;
    case NETWORK_LINK_ALIVE :
        connect(worker, &Worker::sendLinkAlive, this, &Scraper::getLinkAliveProcessResults);
        break;
    case NETWORK_DOFOLLOW :
        connect(worker, &Worker::sendDofollow, this, &Scraper::checkDofollowNofollowProcessResults);
        break;
    case NETWORK_PLATFORM :
        connect(worker, &Worker::sendCheckPlatform, this, &Scraper::getPlatformProcessResults);
        break;
    case NETWORK_WHOIS :
        _proxyType = "none";
        connect(worker, &Worker::sendWhoisResponse, this, &Scraper::getWhoisProcessResults);
        break;
    case NETWORK_URL_BL :
        connect(worker, &Worker::sendBacklinks, this, &Scraper::getBacklinksProcessResults);
        // Pour le check de backlinks, on fait du minimal log !!
        _minimalLogs = true;
        break;
    default :
        break;
    }


    // Connect network retry
    connect(worker, &Worker::sendRetry, this, &Scraper::sendRequest);
    // Connect the appendLog
    connect(worker, &Worker::sendMsg, this, &Scraper::sendAppendLog);
    // Connect the updater for number of threads launched
    connect(worker, &Worker::updateThreadsLaunched, this, &Scraper::updateNbOfThreadsLaunched);
    // Connect the abort
    connect(this, &Scraper::abortNetwork, worker, &Worker::abortSignalReceived,Qt::DirectConnection);
    // Connect the proxies banned signal
    connect(worker, &Worker::updateBannedProxies,this, &Scraper::updateBannedProxies);
    // Connect the abort signal
    connect(worker, &Worker::sendAbortSignal, this, &Scraper::sendAbortNetwork);
    // Connect the cookies signal
    //connect(worker, &Worker::updateProxyCookiePair, this, &Scraper::updatePairProxyCookies);
    // Connect number of items found
    connect(worker, &Worker::updateItemsFound,this,&Scraper::updateItemsFound);
    // Connect the updateTableResult
    connect(worker, &Worker::sendResultsToTable, this, &Scraper::updateTableResults);


    // We build the qHash for the thread
    threadParameters.insert("operationType", params.value("operationType",NETWORK_SCRAP));
    threadParameters.insert("userAgent", _uaUsed);
    threadParameters.insert("realUrl", params.value("realUrl",""));
    threadParameters.insert("custom1", params.value("custom1",""));
    threadParameters.insert("custom2", params.value("custom2",""));
    // For the JSON pre-treatment
    threadParameters.insert("jsonStructure", _seJsonStructure);
    // For the XPath interpreter
    threadParameters.insert("xpathInterpreter",_xpathInterpreterIndex);
    // Force output encoding
    threadParameters.insert("forceOutputEncoding", _forceOutputEncoding);

    threadParameters.insert("apiKey", params.value("apiKey",""));
    threadParameters.insert("currentAPI", params.value("currentAPI",""));
    threadParameters.insert("scrapUrlsSize", scrapurls.size());

    threadParameters.insert("redirFound", params.value("redirFound",false));
    threadParameters.insert("previousRedirections", params.value("previousRedirections",""));



    if(params.value("redirFound",false).toBool() == false)
         threadParameters.insert("initialUrl", url);
    else
        threadParameters.insert("initialUrl", params.value("initialUrl",""));


    // Options
    threadParameters.insert("scrapPause",_scrapPause);
    threadParameters.insert("scrapPausePagination",_scrapPausePagination);
    threadParameters.insert("retryOnTimeout",_retryOnTimeout);
    threadParameters.insert("urlsList", params.value("urlsList",QStringList()));
    threadParameters.insert("searchForUrl", params.value("searchForUrl",""));
    threadParameters.insert("currentAction", params.value("currentAction",""));
    threadParameters.insert("keepStackFiles", this->keepStackFiles);
    threadParameters.insert("keepHtmlEncode", this->htmlEncoded);
    threadParameters.insert("backlinksMethod", params.value("backlinksMethod",this->getBacklinksMethod));
    bool displayStackFile = (type == NETWORK_SCRAP) ? true : false;
    threadParameters.insert("displayStackFile", params.value("displayStackFile",displayStackFile));
    threadParameters.insert("minimalLogs",_minimalLogs);
    threadParameters.insert("useProxyProviderRotation",_useProxyProviderRotation);
    threadParameters.insert("resendReqStr",_resendReqStr);
    threadParameters.insert("resendReqStrNotContains",_resendReqStrNotContains);
    threadParameters.insert("resendReqStrAction",_resendReqStrAction);

    threadParameters.insert("resendHTTP",_resendHTTP);
    threadParameters.insert("resendHTTPCondition",_resendHTTPCondition);
    threadParameters.insert("resendHTTPCode",_resendHTTPCode);
    threadParameters.insert("resendHTTPAction",_resendHTTPAction);
    threadParameters.insert("httpLogFile",_httpLogFilename);

    threadParameters.insert("acceptLanguage",this->seSelected.value("acceptLanguage").toString().trimmed());
    threadParameters.insert("acceptEncoding",this->seSelected.value("acceptEncoding").toString().trimmed());
    threadParameters.insert("host",this->seSelected.value("host").toString().trimmed());
    threadParameters.insert("referer",this->seSelected.value("referer").toString().trimmed());
    threadParameters.insert("contentType",this->seSelected.value("contentType").toString().trimmed());
    threadParameters.insert("requestType",this->seSelected.value("requestType").toString().trimmed());
    threadParameters.insert("postArray",this->seSelected.value("postArray").toString().trimmed());

    threadParameters.insert("sourceCodeType",(!this->seSelected.value("sourceCodeType").toInt() ? "xml" : "json"));

    // Thread random number
    QByteArray threadHash;
    threadHash = QCryptographicHash::hash(url.toLatin1(),QCryptographicHash::Md5);

    QString tHashStr(threadHash.toHex());
    tHashStr.replace(QRegExp("\\D"),"");
    threadParameters.insert("threadUID",tHashStr.mid(0,9));


     // Proxy part
     threadParameters.insert("proxyType", params.value("proxyType",_proxyType));

    if(!params.value("proxyAddr","").toString().isEmpty())
    {
        threadParameters.insert("proxyAddr", params.value("proxyAddr"));
        threadParameters.insert("proxyPort", params.value("proxyPort"));
        threadParameters.insert("proxyUser", params.value("proxyUser"));
        threadParameters.insert("proxyPassword", params.value("proxyPassword"));

        threadParameters.insert("proxyType",params.value("proxyType",_proxyType));
    }

    // We also can pass the XPath for content and pagination
    // create a function in order to get correct XPath
    threadParameters.insert("xpathContent",
                            buildXPathExpression(this->seSelected.value("xpath_content").toString(),
                                                 threadParameters.value("custom1").toString(),
                                                 threadParameters.value("custom2").toString()));
    threadParameters.insert("xpathPagination",
                            buildXPathExpression(this->seSelected.value("xpath_pagination").toString(),
                                                 threadParameters.value("custom1").toString(),
                                                 threadParameters.value("custom2").toString()));

    QString scrapurl = url;
    // On test Google Suggest pour le wildcard
    if(scrapurl.contains(QRegularExpression("^https?://suggestqueries.google.com")))
    {
        QUrlQuery ggSuggestUrl(scrapurl);

        if(ggSuggestUrl.queryItemValue("q").contains("*"))
            scrapurl.append("&cp="+QString::number(ggSuggestUrl.queryItemValue("q").indexOf("*")));
    }

    this->worker->initScrapThread(scrapurl, threadParameters);
}

int Scraper::getScrapUrlsSize()
{
    return this->scrapurls.size();
}


void Scraper::threadTimeoutSlot(const QString &url, QHash<QString,QVariant> replyProperties)
{
    int operationType;

    this->scraperMutex.lock();
    this->_threadsTimedOut++;
    operationType = replyProperties.value("operationType").toInt();
    this->scraperMutex.unlock();

    QHash<QString, QVariant> results;

    // On devra tester nb de retry, si Google URL, si RDDZ proxies ...

    results.insert("url",url);
    results.insert("xpathContentRes",QVariant(QStringList()));
    results.insert("xpathPaginationRes","");

    if(operationType == NETWORK_SCRAP)
    {
    this->interpretScrapResults(results,replyProperties);
    }
    else if(operationType == NETWORK_HTTPSTATUS)
    {
        this->getUrlRespCodeProcessResults(503,url);
    }
    else if(operationType == NETWORK_REDIR)
    {
        this->resolveRedirectionsProcessResults(503,url);
    }
    else if(operationType == NETWORK_REDIR_STATUS)
    {
            this->resolveRedirectionsProcessResults(503,replyProperties.value("initialUrl").toString(),url);
    }
    else if(operationType == NETWORK_DOFOLLOW)
    {
        this->checkDofollowNofollowProcessResults(-2,url);
    }
    else if(operationType == NETWORK_WHOIS)
    {
        this->getWhoisProcessResults("API timeout",url);
    }
    else if(operationType == NETWORK_OBL)
    {
        this->getOblProcessResults(-1,url);
    }
    else if(operationType == NETWORK_PLATFORM)
    {
        this->getPlatformProcessResults(tr("Server timeout"),url);
    }
}


void Scraper::retryRequestSlot(const QString &url, QHash<QString, QVariant> replyProperties)
{
    qDebug() << "Scraper::retryRequestSlot Resend request for" << url;

    this->sendRequest(url, replyProperties);
}

void Scraper::updateTableResults(QStringList list,QHash<QString, QVariant> replyProperties, QString url)
{
    int i;
    QUrl nextUrl;
    QList<QVariant> variantList;

    if(!url.isEmpty())
    nextUrl = QUrl(url);


    // On update le nb de results
    scraperMutex.lock();
    this->resultsFound += list.size();
    emit updateNbItems(static_cast<int>(this->resultsFound));
    scraperMutex.unlock();


    // Si on save les results dans un fichier
    if(config->getConfigurationValue("autoSaveScrap") == "true")
    {
        QStringList listCopy = list;
        QString textResults;
        listCopy.removeAll("");

        if(listCopy.size())
        {
            if(!config->getConfigurationValue("lineBreak").toInt())
                textResults = listCopy.join("\r\n");
            else
                textResults = listCopy.join("\n");
            scrapDataStream << textResults;
        }
    }


    for (i = 0;i<list.size();i++)
    {
        if(!list[i].isEmpty())
        {
            scraperMutex.lock();
            QString newItem = this->seSelected.value("prefix").toString()+list.at(i)+this->seSelected.value("suffix").toString();
            newItem.replace("{%custom1%}", replyProperties.value("custom1","").toString());
            newItem.replace("{%custom2%}", replyProperties.value("custom2","").toString());
            newItem.replace("{%footprint%}", this->footprint);
            newItem.replace("{%scrapurl%}", nextUrl.toString());
            list[i] = newItem;
            scraperMutex.unlock();
        }
    }

    // On update la vue
    variantList = Tools::stringListToVariantList(list);

    // On traite la liste ici en cas de custom header
    if(this->customScrapColumn)
    {
        QString csvScheme;

        csvScheme.append(mainWin->getCsvEnclosedBy());
        csvScheme.append(mainWin->getCsvSeparator());
        csvScheme.append(mainWin->getCsvEnclosedBy());

        // For customColumn scrap result
        for(i = 0; i<variantList.size();i++)
        {
            QVariantList stackVariantList = Tools::stringListToVariantList(variantList.at(i).toString().split(csvScheme));
            this->scraptable->buildScrapResultsHash(stackVariantList,-1,-1,"row");
        }
    }
    else
        // For std scrap result
        this->scraptable->buildScrapResultsHash(variantList,this->scraptable->columnIndexes.value("urlCol"));
}



/**
 * @brief Scraper::interpretScrapResults
 * @param results
 *
 * Interpret results of scrap
 * Key for results are :
 * - url
 * - xpathContentRes
 * - xpathPaginationRes
 */
void Scraper::interpretScrapResults(QHash<QString, QVariant> results, QHash<QString, QVariant> replyProperties)
{
    QString nextPageUrl = results.value("xpathPaginationRes","").toString();
    QUrl nextUrl;
    QHash<QString,QVariant> requestArgs;
    QStringList list;
    QUrl url;

    if(!results.value("url","").toString().isEmpty())
        nextUrl = QUrl(results.value("url").toString());

    list = results.value("xpathContentRes").toStringList();

    this->updateTableResults(list, replyProperties, results.value("url").toString());

    QList<QVariant> variantList = Tools::stringListToVariantList(list);

    // If pagination not empty, re-launch scrap for next page
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
            this->sendRequest(nextPageUrl,replyProperties);
        }
    }
    else
    {
        scrapurlsSize--;
        testingCounter++;

        if(scrapUrlHasCustoms)
            emit updateCustomRatio(scrapurlsSize);
        if(this->seSelected.value("xpath_pagination").toString().isEmpty() && scrapUrlHasCustoms)
            emit updateTaskProgressBar(initScrapUrlSize-scrapurlsSize, initScrapUrlSize, tr("Processing customs"));

        // We have to test if scrap is finished or not
        if (scrapurls.isEmpty() && scrapurlsSize == 0)
        {
            emit updateScrapTableLayout();
            if (!variantList.isEmpty() || this->scraptable->scrapResults.size())
            {
                emit removeDuplicateUrl();
                if (   ( config->getAutoGetStatus() ||
                        config->getAutoRemoveBadUrl() ||
                        config->getAutoResolveRedir() ) && config->readGeneralConfig("StdColumn","HTTP",true).toBool())
                    emit autoHttpStatus();
                else if(config->getAutoDF() && this->config->readGeneralConfig("StdColumn","DF",true).toBool())
                    emit autoCheckDofollowNofollow();
                else if(config->getAutoBL() && this->config->readGeneralConfig("StdColumn","BL",true).toBool())
                    emit autoBacklinks();
                else if(config->getAutoOBL() && this->config->readGeneralConfig("StdColumn","OBL",true).toBool())
                    emit autoObl();
                else
                {
                    if(!this->seSelected.value("xpath_pagination").toString().isEmpty())
                        emit updateTaskProgressBar(100,100,"");

                    emit appendLog(tr("// Scrap finished"));
                    emit showSystrayMsg(tr("Scrap finished"));
                    emit scrapButtonEnabled();
                }
            }
            else
            {
                if(!this->seSelected.value("xpath_pagination").toString().isEmpty())
                    emit updateTaskProgressBar(100,100,"");

                emit appendLog(tr("// Scrap finished"));
                emit showSystrayMsg(tr("Scrap finished"));
                emit scrapButtonEnabled();
            }
        }
        else if(!scrapurls.isEmpty() && (_threadsAlreadyLaunched >= _parallelThreads))
        {

            QStringList scrapData = getNextScrapUrl();
            if(scrapData.size() == 3)
            {
                QString scrapurl = scrapData[0];
                requestArgs.insert("operationType",NETWORK_SCRAP);
                requestArgs.insert("realUrl",scrapurl);
                requestArgs.insert("custom1",scrapData[1]);
                requestArgs.insert("custom2",scrapData[2]);
                this->sendRequest(scrapurl, requestArgs);
            }
        }
    }
}

QString Scraper::buildXPathExpression(QString xpathQuery, QString custom1, QString custom2)
{
    QString escapedQuery;

    xpathQuery = xpathQuery.trimmed();
    xpathQuery.replace("{%footprint%}", this->footprint);
    if(xpathQuery.contains(QRegExp("\\{%custom[12]{1}%\\}")))
    {
        xpathQuery.replace("{%custom1%}", custom1);
        xpathQuery.replace("{%custom2%}", custom2);
    }
    else if(xpathQuery.contains(QRegExp("\\{%custom[12]::encoded%\\}")))
    {
        xpathQuery.replace("{%custom1::encoded%}", this->replaceCustom(custom1,1));
        xpathQuery.replace("{%custom2::encoded%}", this->replaceCustom(custom2,1));
    }

    escapedQuery = xpathQuery;

    return escapedQuery;
}


void Scraper::phantomRenderFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    QProcess *phantomProcess = qobject_cast<QProcess*>(sender());
    QByteArray output;
    QStringList args = phantomProcess->arguments();
    QString url, custom1, custom2, proxyType, proxy, originalUrl,  cookieFilename,clipRectFilename,userAgent, referrerUrl;
    QHash<QString, QString> scrapArgs;

    if(exitStatus == QProcess::CrashExit)
    {
        qDebug() << "Phantom Crashed";
         phantomProcess->close();
        return;
    }

    if(!exitCode && exitStatus)
    {
        qDebug() << "Phantom timeout";
        phantomProcess->kill();
        if(this->_scrapRunning == true)
            emit appendLog(tr("Page loading timeout for %1").arg(url), "warning");
        this->scrapFinished(QStringList(), "standard");
        return;
    }



    output = phantomProcess->readAllStandardOutput();
    phantomProcess->close();
    scrapArgs.clear();
    proxy           = "";
    userAgent       = (args.filter("userAgent=").size()) ? args.filter("userAgent=").at(0) : "";
    proxyType       = (args.filter("--proxy-type=").size()) ? args.filter("--proxy-type=").at(0) : "none";
    url             = (args.filter("url=").size()) ? args.filter("url=").at(0) : "";
    originalUrl     = (args.filter("originalUrl=").size()) ? args.filter("originalUrl=").at(0) : "";
    custom1         = (args.filter("custom1=").size()) ? args.filter("custom1=").at(0) : "";
    custom2         = (args.filter("custom2=").size()) ? args.filter("custom2=").at(0) : "";
    cookieFilename  = (args.filter("cookieFilename=").size()) ? args.filter("cookieFilename=").at(0) : "";
    clipRectFilename = (args.filter("clipRect=").size()) ? args.filter("clipRect=").at(0) : "";
    referrerUrl      = (args.filter("referrerUrl=").size()) ? args.filter("referrerUrl=").at(0) : "";


    custom1.replace("custom1=","");
    custom2.replace("custom2=","");
    url.replace("url=","");
    originalUrl.replace("originalUrl=","");
    cookieFilename.replace("cookieFilename=","");
    proxyType.replace("--proxy-type=","");
    clipRectFilename.replace("clipRect=","");
    referrerUrl.replace("referrerUrl","");
    userAgent.replace("userAgent","");

    if(proxyType != "none")
    {
        QString proxyHost, proxyAuth;
        proxyHost     = (args.filter("--proxy=").size()) ? args.filter("--proxy=").at(0) : "";
        proxyAuth     = (args.filter("--proxy-auth=").size()) ? args.filter("--proxy-auth=").at(0) : "";
        proxyHost.replace("--proxy=","");
        if(!proxyAuth.isEmpty())
            proxyAuth.replace("--proxy-auth=",":");
        proxy = proxyHost+proxyAuth;
    }

    scrapArgs.insert("url",url);
    scrapArgs.insert("userAgent",userAgent);
    scrapArgs.insert("custom1",custom1);
    scrapArgs.insert("custom2",custom2);
    scrapArgs.insert("output",output);
    scrapArgs.insert("proxy",proxy);
    scrapArgs.insert("originalUrl",originalUrl);
    scrapArgs.insert("cookieFilename",cookieFilename);
    scrapArgs.insert("clipRect",clipRectFilename);
    scrapArgs.insert("referrerUrl",referrerUrl);

    QNetworkAccessManager *qnam = nullptr;
    QtConcurrent::run(this, &Scraper::scrapProcessResults, scrapArgs,qnam);
}

void Scraper::phantomRenderError(QProcess::ProcessError error)
{
    if(error == 1)
    {
        this->scrapFinished(QStringList(), "standard");
    }
}

void Scraper::phantomReadyReadStandardOutput()
{

}

void Scraper::phantomReadyReadStandardError()
{
    QProcess *phantomProcess = qobject_cast<QProcess*>(sender());
    emit appendLog(phantomProcess->readAllStandardError(), "error");
    phantomProcess->kill();
}

void Scraper::donwloadProgress(qint64 bytesReceived, qint64 bytesTotal)
{
    if(bytesTotal == -1)
        emit updateTaskProgressBar(0,0,tr("Downloading data"));
    else
        emit updateTaskProgressBar(static_cast<int>(bytesReceived),static_cast<int>(bytesTotal),tr("Downloading data"));
}


void Scraper::getScrapFinished()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    QNetworkAccessManager *qnam = reply->manager();
    QHash<QString, QString> scrapArgs;
    QUrl newurl;
    QString detectedCharset, currentProxy;
    QNetworkProxy           proxy;
    int httpstatus, type;

    type = NETWORK_SCRAP;
    detectedCharset = "";
    scrapArgs.clear();
    proxy           = qnam->proxy();
    currentProxy    = proxy.hostName()+":"+QString::number(proxy.port())+"::";
    emit deleteReplyFromQhash(reply->property("realUrl").toString());

    if(reply)
    {
        scraperMutex.lock();
        QByteArray replyContent(reply->readAll());
        scraperMutex.unlock();
        // We get the content-type if present
        QRegularExpression charsetRegExp("<meta.*?charset=[\"']?([^\"']+)[\"']?");
        QRegularExpressionMatch match = charsetRegExp.match(replyContent);
        if (match.hasMatch())
            detectedCharset = match.captured(1);

        QTextCodec *codec = (!detectedCharset.isEmpty() && QTextCodec::codecForName(detectedCharset.toUtf8())) ? QTextCodec::codecForName(detectedCharset.toUtf8()) : QTextCodec::codecForName("UTF-8");

        QString output = codec->toUnicode(replyContent);

        // On doit tester si Google Captcha avant de Kill la query
        scrapArgs.insert("url",reply->property("url").toString());
        scrapArgs.insert("userAgent",reply->property("userAgent").toString());
        scrapArgs.insert("custom1",reply->property("custom1").toString());
        scrapArgs.insert("custom2",reply->property("custom2").toString());
        scrapArgs.insert("output",output);
        scrapArgs.insert("proxy",reply->property("proxy_used").toString());
        scrapArgs.insert("originalUrl",reply->property("realUrl").toString());
        scrapArgs.insert("captchaId",reply->property("captchaId").toString());

        httpstatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if(httpstatus >= 300 && httpstatus < 400)
        {
            newurl = reply->url().resolved(reply->attribute(QNetworkRequest::RedirectionTargetAttribute).toUrl());
            if(Tools::isGoogleUrl(scrapArgs.value("url")))
            {

                if(Tools::isGoogleCaptchaUrl(newurl.toString()))
                {
                    if(!config->useProxyRotating())
                    {
                    emit appendLog(tr("[GOOGLE Captcha] for url %1").arg(reply->url().toString()), "warning");
                    }
                    else
                    {
                        qnam = nullptr;
                      newurl = QString(reply->property("realUrl").toString());
                    }
                }
            }
            reply->close();
            emit createHttpRequest(newurl.toString().simplified(), scrapArgs.value("ua"), type, QStringList() << newurl.toString().simplified() << scrapArgs.value("custom1") << scrapArgs.value("custom2"),newurl,qnam);
        }
        else
        {
           QtConcurrent::run(this, &Scraper::scrapProcessResults, scrapArgs, qnam);
        }
        disconnectReply(reply, NETWORK_SCRAP);
        reply->deleteLater();
    }
}



void Scraper::scrapProcessResults(QHash<QString, QString> args, QNetworkAccessManager *qnam)
{

    QUrl nextUrl(args.value("url"));
    QString nextPageUrl, scrapText, defaultNamespace, sePagination, seContent, str, custom1, custom2, proxyUsed;
    QByteArray ua = "standard";
    QStringList list;
    QUrl url;
    int i;


    scraperMutex.lock();
    // Init some args
    str = args.value("output");
    custom1 = args.value("custom1");
    custom2 = args.value("custom2");
    proxyUsed = args.value("proxy");
    scraperMutex.unlock();



    // Test du resend request
    if(!_resendReqStr.isEmpty() && _useProxyProviderRotation)
    {
        if(str.contains(_resendReqStr))
        {
            emit appendLog(tr("String \"%1\" found in source code. Re-sending request ").arg(_resendReqStr) + nextUrl.toString(), "information");
            emit runPhantomScrap(args);
            return;
        }
    }


    // Pour les captchas avec phantomJs, on test direct si captcha dans le src code
        if (Tools::isGoogleUrl(nextUrl.toString()))
        {
            if(str.contains("<meta http-equiv=\"refresh\""))
            {
                emit appendLog(tr("[Google captcha] : captcha code is correct. Continue"),"success");
                QStringList metaRefresh = getXpath(str, "//meta[@http-equiv=\"refresh\"]/@content/string()");
                int indexOfEqual = metaRefresh.at(0).indexOf('=') + 1;

                args.insert("url",metaRefresh.at(0).mid(indexOfEqual));
                args.insert("userAgent","standard");
                emit runPhantomScrap(args);
                return;
            }
            else if(nextUrl.toString().contains(QRegExp("^https?://(ipv4|ipv6).google")) && !str.contains("src=\"/sorry/image?id")) // Captcha OK
            {
                QUrlQuery continueQuery(nextUrl);
                nextUrl.setUrl(nextUrl.toString().replace(QRegularExpression("^(https?://)(ipv4|ipv6).google."),"\\1www.google."));
                emit appendLog(nextUrl.toString().replace(QRegularExpression("^(https?://)(ipv4|ipv6).google."),"\\1www.google."));
                emit appendLog(tr("[Google Captcha] : code entered OK !! Return code : %1 for captcha image  %2").arg(continueQuery.queryItemValue("captcha"), args.value("captchaImg")),"success");

            }
            else if(str.contains("src=\"/sorry/image?id")) // Captcha
            {
                // On test pour voir si on vient deja d'un captcha
                if(nextUrl.toString().contains(QRegExp("^https?://(ipv4|ipv6).google")))
                {
                    QUrlQuery continueQuery(nextUrl);
                    emit appendLog(tr("[Google Captcha] : code entered was wrong !! Return code is : %1 for %2").arg(continueQuery.queryItemValue("captcha"),args.value("captchaImg")),"error");
                }

                // Pour rotation IP auto, on relance
                if(config->useProxyRotating())
                {
                    // Reset proxy to switch to another one
                    args.insert("proxy","");
                    args.insert("userAgent","standard");
                    emit runPhantomScrap(args);
                    return;
                }

                tools->proxiesBanned.append(proxyUsed);
                if(config->fullDebug && config->getConfigurationValue("minimalLog") == "false")
                    emit appendLog("fullDebug[Scraper::scrapProcessResults] nextUrl value : " + nextUrl.toString(),"fulldebug");

                // Reset proxy to switch to another one
                args.insert("proxy","");
                args.insert("userAgent","standard");
                emit runPhantomScrap(args);
                return;
            }
            else if(str.contains("<title>Sorry...</title>") && !str.contains("src=\"/sorry/image?id")) // Proxy blacklist
            {
                tools->proxiesBanned.append(proxyUsed);
                emit appendLog("Proxy "+proxyUsed+" blacklisted by Google", "error");
                // Todo : Remove proxy from list + relaunch scrap from nextUrl
                tools->proxiesBanned.removeDuplicates();
                if (config->get_proxies().size() == tools->proxiesBanned.size())
                    this->scrapFinished(QStringList(), "standard");
                else
                {
                    // Reset proxy to switch to another one
                    args.insert("proxy","");
                    args.insert("userAgent","standard");
                    emit runPhantomScrap(args);
                }
                return;
            }
        }

    if(tools->proxiesBanned.indexOf(proxyUsed) != -1)
        tools->proxiesBanned.removeAll(proxyUsed);





    scraperMutex.lock();
    this->countScraper++;
    scrapText = str;
    defaultNamespace = "";
    scraperMutex.unlock();


    // On test si resource Timeout par PhantomJs
    if(str.startsWith("Timeout :"))
    {
        emit appendLog(tr("Page loading timeout for url : %1").arg(nextUrl.toString()), "warning");

        // Mettre en place le retryOnTimeout
        if(config->useProxyRotating() || _retryOnTimeout)
            emit runPhantomScrap(args);
        else
        this->scrapFinished(QStringList(), ua);

        return;
    }
    // On test code erreur (mode nojs)
    else if(str.startsWith("Error ["))
    {
        emit appendLog(str, "error");
        this->scrapFinished(QStringList(), ua);
        return;
    }
    // On test le silent mode (nojs)
    else if(str == "NoAppendLog")
    {
        this->scrapFinished(QStringList(), ua);
        return;
    }


    // Si systeme de rotation d'IP auto, on relance la query si on ne trouve pas un pattern spécial !!
    if(config->useProxyRotating() && scrapText.isEmpty())
    {
        emit appendLog("[Proxy "+proxyUsed+"] " + tr("Source code for : %1 is empty ! I resend the request").arg(nextUrl.toString()),"warning");

        nextPageUrl = nextUrl.toString();
        // On utilise cette methode uniquement avec phantom now, la version sans js utilise le worker
            emit runPhantomScrap(args);
        return;
    }


    if (!this->seSelected.value("xpathPagination").toString().isEmpty())
    {
        sePagination = this->seSelected.value("xpath_pagination").toString();
        QStringList getXpathResults = this->getXpath(scrapText, sePagination, custom1, custom2,nextUrl.toString(),false);
        nextPageUrl = (getXpathResults.size()) ? getXpathResults.at(0) : "";
        nextPageUrl.replace("&amp;","&");
    }
    seContent = this->seSelected.value("xpath_content").toString();
    list.clear();
    list = this->getXpath(scrapText, seContent, custom1, custom2, nextUrl.toString(),true);
    url = QUrl(nextPageUrl);

    if(!list.isEmpty())
    {
        if(list.at(0).isEmpty())
        {
            // Faire un test pour les proxies RDDZ
            if(scrapText.isEmpty())
            emit appendLog("[Proxy "+proxyUsed+"] " + tr("Source code for : %1 is empty !").arg(nextUrl.toString()), "error");
            list.clear();
        }
    }

    // We update the number of found items
    scraperMutex.lock();
    this->resultsFound += list.size();
    emit updateNbItems(static_cast<int>(this->resultsFound));
    scraperMutex.unlock();



    if(config->getConfigurationValue("minimalLog") == "false")
    emit appendLog("Found "+QString::number(list.size())+" items for url "+nextUrl.toString());

    if (url.isRelative() && !nextPageUrl.isEmpty())
    {
        QString urlPath;
        scraperMutex.lock();
        urlPath = (nextPageUrl.startsWith('/')) ? "" : nextUrl.path().section('/',0,-2)+"/";
        if(nextPageUrl.startsWith('?') || nextPageUrl.startsWith('#'))
            nextPageUrl = nextUrl.toString()+nextPageUrl;
        else
            nextPageUrl = QUrl(nextUrl.scheme()+"://"+nextUrl.host().trimmed()).resolved(QUrl(urlPath+nextPageUrl)).toString();
        scraperMutex.unlock();
    }

        if (!(this->seSelected.value("prefix").toString().isEmpty() &&
              this->seSelected.value("suffix").toString().isEmpty()))
        for (i = 0;i<list.size();i++)
        {
            if(!list[i].isEmpty())
            {
                scraperMutex.lock();
                QString newItem = this->seSelected.value("prefix").toString()+list.at(i)+this->seSelected.value("suffix").toString();
                newItem.replace("{%custom1%}", custom1);
                newItem.replace("{%custom2%}", custom2);
                newItem.replace("{%footprint%}", this->footprint);
                newItem.replace("{%scrapurl%}", nextUrl.toString());
                list[i] = newItem;
                scraperMutex.unlock();
            }
        }

    if(config->getConfigurationValue("autoSaveScrap") == "true")
    {
        scraperMutex.lock();
        QStringList listCopy = list;
        QString textResults;
        listCopy.removeAll("");
        if(!config->getConfigurationValue("lineBreak").toInt())
            textResults = listCopy.join("\r\n");
        else
            textResults = listCopy.join("\n");
        scrapDataStream << textResults;
        scraperMutex.unlock();

    }

    QList<QVariant> variantList = Tools::stringListToVariantList(list);
    // On traite la liste ici en cas de custom header
    if(this->customScrapColumn)
    {
        QString csvScheme;

        csvScheme.append(mainWin->getCsvEnclosedBy());
        csvScheme.append(mainWin->getCsvSeparator());
        csvScheme.append(mainWin->getCsvEnclosedBy());


        // For customColumn scrap result
        for(i = 0; i<variantList.size();i++)
        {
            QVariantList stackVariantList = Tools::stringListToVariantList(variantList.at(i).toString().split(csvScheme));
            this->scraptable->buildScrapResultsHash(stackVariantList,-1,-1,"row");
        }
    }
    else
        this->scraptable->buildScrapResultsHash(variantList,this->scraptable->columnIndexes.value("urlCol"));
    // On set la pause ici, si pas que pour la pagination

    if(this->seSelected.value("pause",0).toInt() && this->seSelected.value("pausePagination").toBool() == false)
    {
    QTime dieTime= QTime::currentTime().addSecs(this->seSelected.value("pause",0).toInt());
    while( QTime::currentTime() < dieTime )
        QCoreApplication::processEvents(QEventLoop::AllEvents, 100);
    }



    if(!nextPageUrl.isEmpty())
    {
        if (nextPageUrl.contains(QRegExp("^https?://(www|m|maps).google")))
            nextPageUrl.replace("+"," ");
        args.insert("url",nextPageUrl);
        args.insert("referrerUrl",nextUrl.toEncoded());
        // Reset proxy to switch to another one
        args.insert("proxy","");

        // Sinon on set la pause ici si que pour la pagination
        if(this->seSelected.value("pause",0).toInt() && this->seSelected.value("pausePagination").toBool())
        {
        QTime dieTime= QTime::currentTime().addSecs(this->seSelected.value("pause",0).toInt());
        while( QTime::currentTime() < dieTime )
            QCoreApplication::processEvents(QEventLoop::AllEvents, 100);
        }


        if(this->jsEnable)
            emit runPhantomScrap(args);
        else
            emit createHttpRequest(nextPageUrl, args.value("userAgent"), NETWORK_SCRAP, QStringList() << nextPageUrl << args.value("custom1") << args.value("custom2"), QUrl(),qnam);
    }
    else if (nextPageUrl.isEmpty())
    {
        if(qnam)
            qnam->deleteLater();
        this->scrapFinished(list, ua);
    }
}

void Scraper::sendAppendLog(QString msg, QString logStyle)
{
    emit appendLog(msg,logStyle);
}

void Scraper::scrapFinished(QStringList list, QString ua, QNetworkAccessManager *qnam)
{
    // ScrapurlsSize pour avoir une seule fois le msg scrapFinished
    scraperMutex.lock();
    scrapurlsSize--;
    scraperMutex.unlock();

    if(scrapUrlHasCustoms)
        emit updateCustomRatio(scrapurlsSize);

    if(this->seSelected.value("xpath_pagination").toString().isEmpty() && scrapUrlHasCustoms)
        emit updateTaskProgressBar(initScrapUrlSize-scrapurlsSize, initScrapUrlSize, tr("Processing customs"));

    if (scrapurls.isEmpty() && scrapurlsSize == 0)
    {
        if (!list.isEmpty() || this->scraptable->scrapResults.size())
        {
            emit removeDuplicateUrl();
            if (   ( config->getAutoGetStatus() ||
                    config->getAutoRemoveBadUrl() ||
                    config->getAutoResolveRedir() ) && config->readGeneralConfig("StdColumn","HTTP",true).toBool())
                emit autoHttpStatus();
            else if(config->getAutoDF() && this->config->readGeneralConfig("StdColumn","DF",true).toBool())
                emit autoCheckDofollowNofollow();
            else if(config->getAutoBL() && this->config->readGeneralConfig("StdColumn","BL",true).toBool())
                emit autoBacklinks();
            else if(config->getAutoOBL() && this->config->readGeneralConfig("StdColumn","OBL",true).toBool())
                emit autoObl();
            else
            {
                if(!this->seSelected.value("xpath_pagination").toString().isEmpty())
                    emit updateTaskProgressBar(100,100,"");

                emit updateScrapTableLayout();
                emit appendLog(tr("// Scrap finished"));
                emit showSystrayMsg(tr("Scrap finished"));
                emit scrapButtonEnabled();
            }
        }
        else
        {
            if(!this->seSelected.value("xpath_pagination").toString().isEmpty())
                emit updateTaskProgressBar(100,100,"");

             emit updateScrapTableLayout();
            emit appendLog(tr("// Scrap finished"));
            emit showSystrayMsg(tr("Scrap finished"));
            emit scrapButtonEnabled();
            if(config->getConfigurationValue("autoSaveScrap") == "true")
                autoSaveScrapResultsFile.close();

        }
    }
    else if(!scrapurls.isEmpty() && scrapurlsSize > 0)
    {
        QHash<QString,QString> phantomArgs;
        QStringList scrapData = getNextScrapUrl();
        if(scrapData.size() == 3)
        {
            QString scrapurl = scrapData[0];
            phantomArgs.insert("url",scrapurl);
            phantomArgs.insert("custom1",scrapData[1]);
            phantomArgs.insert("custom2",scrapData[2]);
            phantomArgs.insert("userAgent",ua);

            if(this->jsEnable)
                emit runPhantomScrap(phantomArgs);
            else
            {
                if(qnam)
                    qnam->deleteLater();
                emit createHttpRequest(scrapurl, ua, NETWORK_SCRAP, QStringList() << scrapurl << scrapData[1] << scrapData[2]);
            }

        }
    }
}

void Scraper::setGetBacklinksMethod(int method)
{
    this->scraperMutex.lock();
    this->getBacklinksMethod = method;
    this->scraperMutex.unlock();
}

