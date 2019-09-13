#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "tools.h"
#ifdef WIN32
#include "time.h"
#endif
#include <QMessageBox>
#include <QFileDialog>
#include <QUrl>
#include <QJsonDocument>

Tools::Tools()
{
    urlProxies.clear();
    proxiesBannedIndex.clear();
}

Tools::~Tools()
{

}

void Tools::clearUrlProxies()
{
    urlProxies.clear();
}


int Tools::setProxy(QNetworkAccessManager *m, QStringList l_proxies, QString url, int type)
{
    QNetworkProxy   proxy;
    QNetworkProxy   curProxy;
    QString         curProxyStr;
    QString         proxy_attr;
    QStringList     l_proxy_attr;
    QStringList     proxyUsed, proxiesBannedList;
    int i;


    if (urlProxies.count(url) >= l_proxies.count() && l_proxies.size() > 1)
    {
        urlProxies.remove(url);
        return (1);
    }

    proxy_attr.clear();
    l_proxy_attr.clear();
    if (l_proxies.size() == 0)
    {
        proxy.setType(QNetworkProxy::NoProxy);
        m->setProxy(proxy);
        return (0);
    }

    proxyUsed = urlProxies.values(url);
    proxiesBannedList = this->proxiesBannedDomain.values(this->getDomain(url, false));


    if(type != 7 && type!=8)
    {
        for (i = 0;i < proxyUsed.count();i++)
            l_proxies.removeOne(proxyUsed.at(i));
        for (i = 0;i < proxiesBannedList.count();i++)
            l_proxies.removeOne(proxiesBannedList.at(i));
    }
    else // For captchas, we clear relations url/proxy
        urlProxies.clear();

    if(!l_proxies.size())
    {
        qDebug() << "Or return 1 here";
        return(1);
    }

    proxy_attr = this->randomProxy(l_proxies);
    l_proxy_attr = proxy_attr.split(":");
    proxy.setType(QNetworkProxy::HttpProxy);
    proxy.setHostName(l_proxy_attr[0]);
    if(l_proxy_attr.size() == 1)
        proxy.setPort(80);
    else
        proxy.setPort(l_proxy_attr[1].simplified().toUShort());
    if(l_proxy_attr.count() > 2)
    {
        proxy.setUser(l_proxy_attr[2]);
        proxy.setPassword(l_proxy_attr[3]);
    }
    m->setProxy(proxy);
    curProxy = m->proxy();
    curProxyStr = curProxy.hostName()+":"+QString::number(curProxy.port());
    if (!curProxy.password().isEmpty() && !curProxy.user().isEmpty())
        curProxyStr += ":"+curProxy.user()+":"+curProxy.password();

    // Special case for link alive
    if(type != 13)
    urlProxies.insertMulti(url, curProxyStr);
    return (0);
}

QString Tools::switchProxy(QStringList l_proxies, QString url)
{
    int i;
    QStringList proxyUsed;

    if (urlProxies.count(url) >= l_proxies.count() && l_proxies.size() > 1)
    {
        urlProxies.remove(url);
        return QString();
    }

    // TODO : check blacklist for domain, not URL

    proxyUsed = urlProxies.values(url);
    proxiesIndex = (proxiesIndex >= 0 && proxiesIndex < l_proxies.size()) ? proxiesIndex : l_proxies.size()-1;

    //qDebug() << "proxies Index : " << proxiesIndex;

    for (;proxiesIndex >= 0;proxiesIndex--)
    {
        if(this->proxiesBanned.indexOf(l_proxies.at(proxiesIndex)) != -1)
            continue;

        if(proxyUsed.size())
        {
            if(proxyUsed.indexOf(l_proxies.at(proxiesIndex)) != -1)
                continue;
        }

        urlProxies.insertMulti(url,l_proxies.at(proxiesIndex));

        if(!proxiesIndex)
        proxiesIndex = l_proxies.size()-1;

        i = proxiesIndex;
        proxiesIndex--;

        return l_proxies.at(i);
    }

    return QString();

}

QMap<int, QVariantList> Tools::parseInputFile(QByteArray content, QStringList args, int colCount, QStringList scrapFileHeaderList, QHash<int, QString> tableHeaderData)
{
    QMap<int, QVariantList> results;
    QString headerMozFiletype("# API : Moz");
    QString headerAhrefsFiletype("# API : Ahrefs");
    QString headerMajesticFiletype("# API : Majestic");
    QTextStream in(&content);
    QString line;
    QStringList my_values;
    QVariantList variantList;
    QString filetype;

    QString type = args.at(0);
    QString separator = args.at(1);


    int i, resultsIndex;
    int colNb, currentLineCol;
    int scrapFileHeaderListSize;

    scrapFileHeaderListSize = scrapFileHeaderList.size() - 1;

    for(i=resultsIndex=0;!in.atEnd();i++)
    {
        line = in.readLine();

        if (line.contains(headerAhrefsFiletype))
        {
            filetype = "Ahrefs";
            i++;
        }
        else if(line.contains(headerMajesticFiletype))
        {
            filetype = "Majestic";
            i++;
        }
        else if(line.contains(headerMozFiletype))
        {
            filetype = "Moz";
            i++;
        }

        variantList.clear();
        if (!line.isEmpty())
        {
            if(!line.startsWith('#') && !line.startsWith("\"#"))
            {
                if(type == "csv")
                {
                    // first line is the csv header, so we don't need it
                    if(!i)
                        continue;
                    my_values = line.split(separator).replaceInStrings(QRegExp("(^\")|(\"$')"),"");
                    my_values.replaceInStrings("\"\"","\"");
                    my_values.replaceInStrings("\"","");
                }
                else
                    my_values = line.split(separator);

                // If we have a column, we don't count
                if(my_values.size() == 1)
                {
                    variantList.append(my_values.at(0));
                    // We fill the list with empty values
                    for (colNb = 1; colNb < colCount ;colNb++)
                     variantList.append("");
                }
                else
                {

                    for (colNb = currentLineCol = 0; colNb < colCount ;colNb++)
                    {
                        QByteArray compareFrom = scrapFileHeaderList.at(currentLineCol).toUtf8();
                        QByteArray compareTo = tableHeaderData.value(colNb).toUtf8();

                        if(tr(compareFrom.constData()) == tr(compareTo.constData()) && (colNb < my_values.size()))
                        {
                            variantList.append(my_values.at(currentLineCol));
                            if(currentLineCol < scrapFileHeaderListSize)
                                currentLineCol++;

                        }
                        else
                            variantList.append("");
                    }
                }
                results.insert(resultsIndex,variantList);
                resultsIndex++;
            }
        }
    }
    return results;
}


QByteArray Tools::openFile(QString filetype)
{
    QString display,lastPath;
    QSettings *globalSettings =  new QSettings(QSettings::IniFormat, QSettings::UserScope,"RDDZ_Scraper", "RDDZ_Scraper");
    int i;

    if(filetype == "proxies")
        display = "Proxies file type (*.txt)";
    else if (filetype == "scrap")
        display = "scrap file (*.txt *.csv)";
    else if (filetype == "se")
        display = "SE file (*.sql)";

    lastPath.clear();

    globalSettings->beginGroup("Global");
    lastPath = globalSettings->value("LastPath","").toString();
    globalSettings->endGroup();
    globalSettings->sync();

    if(lastPath.isEmpty())
        lastPath = QApplication::applicationDirPath();



    QStringList filesname = QFileDialog::getOpenFileNames(this, "Open File", lastPath, display);
    QByteArray stack;

    for(i=0;i<filesname.size();i++)
    {
            QFile f(filesname.at(i));

           //qDebug() << "Sarting reading file : " << QDateTime::currentDateTime();

            if (f.open(QIODevice::ReadOnly | QIODevice::Unbuffered) )
            {
                if(filetype == "scrap" && !i)
                {
                    QFileInfo fileInfo(f);

                    globalSettings->beginGroup("Global");
                    globalSettings->setValue("LastPath",fileInfo.absolutePath());
                    globalSettings->endGroup();

                }
                stack.append(f.readAll());
            }
            f.close();
    }
    return stack;
}

void Tools::writeFile(QString filename, QMap<int,QStringList> content, QString format, QHash<QString, QChar> delimiters)
{
    QByteArray line_content;
    QString builtDelimiter;
    QFile f;

    if(format == ".csv")
    {
        builtDelimiter.append(delimiters.value("csvDelimiter"));
        builtDelimiter.append(delimiters.value("csvSeparator"));
        builtDelimiter.append(delimiters.value("csvDelimiter"));
    }
    else if(format == ".txt")
    {
        builtDelimiter.append(delimiters.value("txtDelimiter"));
    }

    if (filename != "")
    {
        f.setFileName(filename);
        if ( f.open(QIODevice::WriteOnly) )
        {
            QTextStream streamFileOut(&f);
            streamFileOut.setCodec("UTF-8");
            streamFileOut.setGenerateByteOrderMark(true);

            QMapIterator<int,QStringList> lineIterator(content);
            lineIterator.toFront();

            while(lineIterator.hasNext())
            {
                lineIterator.next();
                line_content.clear();

                // On traite le content ici
                if(format==".csv")
                {
                    QStringList currentLine = lineIterator.value();
                    currentLine.replaceInStrings(QString(delimiters.value("csvDelimiter")),"\\" + QString(delimiters.value("csvDelimiter")));
                    line_content = currentLine.join(builtDelimiter).toUtf8() ;
                    line_content.prepend(delimiters.value("csvDelimiter").toLatin1());
                    line_content.append(QString(delimiters.value("csvDelimiter")));
                    streamFileOut << line_content;
                }
                else if(format == ".txt")
                {
                    QStringList currentLine = lineIterator.value();
                    line_content = currentLine.join(builtDelimiter).toUtf8() ;
                    streamFileOut << line_content;
                }
                else if(format == ".sql")
                {
                    line_content = lineIterator.value().join("").toUtf8();
                    streamFileOut << line_content;
                }

                streamFileOut << "\n";
                streamFileOut.flush();
            }
            f.close();
        }
        else
            QMessageBox::warning(this, "Erreur","Impossible d'enregister le fichier : "+filename);
    }

}


void Tools::saveFile(QMap<int, QStringList> content, QStringList extensions, QHash<QString, QChar> delimiters)
{
    QString format, filter, filename,extension, separator;
    QFileDialog *saveDlg;
    QRegExp rx("\\*(.*)\\)");
    int i;

    for(i=0;i<extensions.count();i++)
    {
        if(extensions.at(i) == ".txt")
            format = "Text File";
        else if (extensions.at(i) == ".csv")
            format = "CSV File";
        else if (extensions.at(i) == ".xml")
            format = "XML File";
        else if (extensions.at(i) == ".sql")
            format = "SQL File";

        separator = ((i+1)==extensions.count()) ? "" : ";;";
        filter.append(""+format+" (*"+extensions.at(i)+")"+separator);
    }

    saveDlg     = new QFileDialog(this, "Save File", QString(), filter);
    saveDlg->setAcceptMode(QFileDialog::AcceptSave);
    saveDlg->setViewMode(QFileDialog::Detail);
    if (saveDlg->exec())
    {
    filename    =  saveDlg->selectedFiles().at(0);
    extension   =  static_cast<char>(rx.indexIn(saveDlg->selectedNameFilter()));
    extension   =  rx.cap(1);

    if(filename.contains(extension))
        filename.replace(extension,"");
    this->writeFile(filename+extension,content,extension,delimiters);
    }
}

QPair<int,QByteArray> Tools::sendBlockingRequest(QUrl Url)
{
    QPair<int, QByteArray> response;
    QNetworkAccessManager *networkMgr = new QNetworkAccessManager();
    QNetworkReply *reply = networkMgr->get(QNetworkRequest(Url));

    QEventLoop loop;
    QObject::connect(reply, SIGNAL(finished()), &loop, SLOT(quit()));
    loop.exec();
    const QByteArray data=reply->readAll();
    int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    reply->deleteLater();

    response.first = statusCode;
    response.second = data;
    return response;
}

QByteArray Tools::randomUa(QStringList ua)
{
    QByteArray result;
    int random_ua_index = 0;

    if (ua.size() > 1)
    {
        int initSeed = static_cast<int>(QDateTime::currentMSecsSinceEpoch());
        int end = ua.size() -1;
        qsrand(static_cast<uint>(initSeed));
        random_ua_index = qrand()%(end);
    }
    result.append(ua.at(random_ua_index));
    return result;
}

void Tools::deleteItemsFromQtree(QList<int> list, QTreeWidget *qtree)
{
    for (int start=list.size()-1;start>=0;start--)
        qtree->takeTopLevelItem(list.at(start));
}

int Tools::getProxyIndex(QString proxy)
{
    return allProxies.indexOf(proxy);
}

void Tools::setBannedProxy(QString proxy, bool banned)
{
    if(banned)
    {
        if(!this->proxiesBannedIndex.contains(getProxyIndex(proxy)))
        proxiesBannedIndex.append(getProxyIndex(proxy));
    }
    else
        proxiesBannedIndex.removeOne(getProxyIndex(proxy));
}

QString Tools::randomProxy(QStringList proxies, int uid)
{
    // We have to return a proxy who isn't the proxiesBanned
    int start = 0;
    int end = proxies.size() -1;
    int initSeed = (uid) ? uid : static_cast<int>(QDateTime::currentMSecsSinceEpoch());

    // We sort the QList first
    std::sort(proxiesBannedIndex.begin(), proxiesBannedIndex.end());

    if(proxiesBannedIndex.size() >= proxies.size())
    {
        return QString();
    }

    int max = end - start + 1 - proxiesBannedIndex.size();

    QMutex locker;
    locker.lock();
    qsrand(static_cast<uint>(initSeed));
    int random_index = qrand()%(max);
    locker.unlock();

    for (int i = 0;i<proxiesBannedIndex.size();i++)
    {
        int ex = proxiesBannedIndex.at(i);
        if (random_index < ex){
            break;
        }
        random_index++;
    }
    return proxies.at(random_index);
}

char * Tools::myQstrtoStr(QString qstr)
{
    QByteArray ba = qstr.toLatin1();
    char *str = ba.data();
    return str;
}

QStringList Tools::proxyInfos(QString proxy)
{
    int i,proxy_size;
    QString lastField;
    QStringList infos;
    infos.clear();

    infos = proxy.split(':');
    proxy_size = infos.count();


    if(proxy_size>4)
    {
        lastField.clear();
        for(i=3;i<proxy_size;i++)
        {
            lastField.append(infos.at(i));
            if(i+1 != proxy_size)
                lastField.append(":");
        }
        infos[3] = lastField;
        proxy_size = 4;
    }
    else
    {
        infos.append("");
        infos.append("");
    }
    return infos;
}

QString Tools::getDomain(QString url, bool domainOnly, bool scheme)
{
    QUrl qurl(url);
    QString domain,tempUrl,tld, host;


    domain="";
    if(url.isEmpty())
        return domain;

    if(scheme)
    domain = qurl.scheme()+"://";
    tld = qurl.topLevelDomain();
    host = qurl.host();


    if(domainOnly)
    {
        tempUrl = host.mid(0,host.size()-tld.size());
        if(tempUrl == "www")
            return tld.mid(1);
        tempUrl = tempUrl.section(".",-1,-1);
        domain = domain+tempUrl+tld;
    }
    else
        domain = domain+host;

    return domain;
}



QString Tools::toCamelCase(QString str)
{
    QString output;
    int j = 0;
    QRegularExpression regexp("\\w+");
    QRegularExpressionMatchIterator i = regexp.globalMatch(str);
    while (i.hasNext()) {
        QRegularExpressionMatch match = i.next();
        QString value = match.captured().toLower();
        if(!j)
            output.append(value);
        else
        {
            output.append(value.left(1).toUpper()+value.mid(1));
        }
        j++;
    }
    return output;
}

QList<QVariant> Tools::stringListToVariantList(QStringList list)
{
    QMutex toolMutex;

    toolMutex.lock();

    QVariantList variantList;
    QMutableListIterator <QString> listIterator(list);
    while(listIterator.hasNext())
    {
        listIterator.next();
        variantList.append(listIterator.value());
    }

    toolMutex.unlock();
    return variantList;
}

QStringList Tools::dedupStringList(QStringList list)
{
    QStringList outputList;
    outputList.clear();

    QSet<QString> stringSet = QSet<QString>::fromList(list);
    outputList = stringSet.toList();
    return outputList;
}

bool Tools::isGoogleUrl(QString url)
{
    if (url.contains(QRegExp("^https?://(www|m|maps|ipv4|ipv6|news).google")))
            return true;
    return false;

}

bool Tools::isGoogleCaptchaUrl(QString url)
{
    if (url.contains(QRegExp("^https?://(ipv4|ipv6).google.com/sorry/")))
        return true;
    return false;
}

bool Tools::isGoogleCaptchaImgUrl(QString url)
{
    if (url.contains("/sorry/image"))
        return true;
    return false;
}

void Tools::removeFilesByExt(QString directory, QString fileExt)
{
    QDir tmpDir(directory);
    QStringList filesToDel;
    tmpDir.setNameFilters(QStringList() << fileExt);
    filesToDel = tmpDir.entryList();

    if(filesToDel.size())
    {
        int fileCount;
        QFile fileToDel;
        for(fileCount=0;fileCount<filesToDel.size();fileCount++)
        {
            fileToDel.setFileName(tmpDir.absolutePath()+"/"+filesToDel.at(fileCount));
            fileToDel.remove();
        }
    }
}

QString Tools::jsonToXml(QString jsonStr, int currentIndex, int subIndex, QString outputXml)
{
    QJsonParseError *parserror = new QJsonParseError;
    QString inputJson = jsonStr;
    QJsonDocument jsonDoc = QJsonDocument::fromJson(inputJson.toUtf8(), parserror);
    QJsonArray jsonArr;
    QJsonObject jsonObj;
    int i,j;

    if(jsonDoc.isObject())
    {
        jsonObj = jsonDoc.object();
    }
    else if (jsonDoc.isArray())
    {
        jsonArr = jsonDoc.array();

//         qDebug() << "Global array size : " << jsonArr.size();
        if(!currentIndex && outputXml.isEmpty())
        outputXml += "<array>";
        for (i=currentIndex;i<jsonArr.size();i++)
        {
            if(jsonArr.at(i).isString())
                outputXml += "<string>"+jsonArr.at(i).toString()+"</string>";
            else if(jsonArr.at(i).isNull())
                outputXml += "<null></null>";
            else if(jsonArr.at(i).isUndefined())
                outputXml += "<undefined>"+jsonArr.at(i).toString()+"</undefined>";
            else if(jsonArr.at(i).isDouble())
                outputXml += "<double>"+QString::number(jsonArr.at(i).toDouble())+"</double>";
            else if(jsonArr.at(i).isArray()) // recursive loop
            {
                QJsonArray subArray = jsonArr.at(i).toArray();
                outputXml += "<array>";
                for (j=subIndex;j<subArray.size();j++)
                this->jsonToXml(inputJson,i,j+1,outputXml);
                outputXml += "</array>";
            }
            else
                qDebug() << "jsonValue is another type !!!";

        }
        outputXml += "</array>";
    }
    else
    {
        qDebug() << parserror->errorString() << parserror->offset;
    }
    return outputXml;
}

QString Tools::encryptIt(QByteArray string)
{
    return QString(QCryptographicHash::hash(string, QCryptographicHash::Sha1).toHex());
}

bool Tools::extensionSupportedByApi(QString tld, QString api)
{
    QStringList mashapeExt, dynadotExt;
    // Pour Domain tools
//    mashapeExt << "ac" << "ad" << "aero" << "ae" << "af" << "ag" << "ai" << "al" << "am" << "an" << "ao" << "aq" << "ar" << "asia" << "as" << "at" << "au" << "aw" << "ax" << "az" << "ba" << "bb" << "bd" << "be" << "bf" << "bg" << "bh" << "biz" << "bi" << "bj" << "bm" << "bn" << "bo" << "br" << "bs" << "bt" << "bv" << "bw" << "by" << "bz" << "cat" << "ca" << "cc" << "cd" << "cf" << "cg" << "ch" << "ci" << "ck" << "cl" << "cm" << "cn" << "com" << "coop" << "co.uk" << "co" << "cr" << "cu" << "cv" << "cx" << "cy" << "cz" << "de" << "dj" << "dk" << "dm" << "do" << "dz" << "ec" << "edu" << "ee" << "eg" << "er" << "es" << "et" << "eu" << "fi" << "fj" << "fk" << "fm" << "fo" << "fr" << "ga" << "gb" << "gd" << "ge" << "gf" << "gg" << "gh" << "gi" << "gl" << "gm" << "gn" << "gov" << "gp" << "gq" << "gr" << "gs" << "gt" << "gu" << "gw" << "gy" << "hk" << "hm" << "hn" << "hr" << "ht" << "hu" << "id" << "ie" << "il" << "im" << "info" << "in" << "io" << "iq" << "ir" << "is" << "it" << "je" << "jm" << "jobs" << "jo" << "jp" << "ke" << "kg" << "kh" << "ki" << "km" << "kn" << "kp" << "kr" << "kw" << "ky" << "kz" << "la" << "lb" << "lc" << "li" << "lk" << "lr" << "ls" << "lt" << "lu" << "lv" << "ly" << "ma" << "mc" << "md" << "me" << "mg" << "mh" << "mil" << "mk" << "ml" << "mm" << "mn" << "mobi" << "mo" << "mp" << "mq" << "mr" << "ms" << "mt" << "museum" << "mu" << "mv" << "mw" << "mx" << "my" << "mz" << "name" << "na" << "nc" << "net" << "ne" << "nf" << "ng" << "ni" << "nl" << "no" << "np" << "nr" << "nu" << "nz" << "om" << "org" << "pa" << "pe" << "pf" << "pg" << "ph" << "pk" << "pl" << "pm" << "pn" << "pr" << "pro" << "ps" << "pt" << "pw" << "py" << "qa" << "re" << "ro" << "rs" << "ru" << "rw" << "sa" << "sb" << "sc" << "sd" << "se" << "sg" << "sh" << "si" << "sj" << "sk" << "sl" << "sm" << "sn" << "so" << "sr" << "st" << "su" << "sv" << "sy" << "sz" << "tc" << "td" << "tel" << "tf" << "tg" << "th" << "tj" << "tk" << "tl" << "tm" << "tn" << "to" << "tp" << "travel" << "tr" << "tt" << "tv" << "tw" << "tz" << "ua" << "ug" << "uk" << "us" << "uy" << "uz" << "va" << "vc" << "ve" << "vg" << "vi" << "vn" << "vu" << "wf" << "ws" << "ye" << "yt" << "za" << "zm" << "zw";

    // Pour domainR
    mashapeExt << "aaa" << "aarp" << "abarth" << "abb" << "abbott" << "abbvie" << "abc" << "able" << "abogado" << "abudhabi" << "ac" << "academy" << "accenture" << "accountant" << "accountants" << "acer" << "aco" << "active" << "actor" << "ad" << "adac" << "ads" << "adult" << "ae" << "aeg" << "aero" << "aetna" << "af" << "afamilycompany" << "afl" << "africa" << "africamagic" << "ag" << "agakhan" << "agency" << "ai" << "aig" << "aigo" << "airbus" << "airforce" << "airtel" << "akdn" << "al" << "alcon" << "alfaromeo" << "alibaba" << "alipay" << "allfinanz" << "allfinanzberater" << "allfinanzberatung" << "allstate" << "ally" << "alsace" << "alstom" << "am" << "amazon" << "americanexpress" << "americanfamily" << "amex" << "amfam" << "amica" << "amp" << "amsterdam" << "an" << "analytics" << "android" << "anquan" << "ansons" << "anthem" << "antivirus" << "anz" << "ao" << "aol" << "apartments" << "app" << "apple" << "aq" << "aquarelle" << "aquitaine" << "ar" << "arab" << "aramco" << "archi" << "architect" << "army" << "arpa" << "art" << "arte" << "as" << "asda" << "asia" << "associates" << "astrium" << "at" << "athleta" << "attorney" << "au" << "auction" << "audi" << "audible" << "audio" << "auspost" << "author" << "auto" << "autoinsurance" << "autos" << "avery" << "avianca" << "aw" << "aws" << "ax" << "axa" << "axis" << "az" << "azure" << "ba" << "baby" << "baidu" << "banamex" << "bananarepublic" << "band" << "bank" << "banque" << "bar" << "barcelona" << "barclaycard" << "barclays" << "barefoot" << "bargains" << "baseball" << "basketball" << "bauhaus" << "bayern" << "bb" << "bbb" << "bbc" << "bbt" << "bbva" << "bcg" << "bcn" << "bd" << "be" << "beats" << "beauty" << "beer" << "beknown" << "bentley" << "berlin" << "best" << "bestbuy" << "bet" << "bf" << "bg" << "bh" << "bharti" << "bi" << "bible" << "bid" << "bike" << "bing" << "bingo" << "bio" << "biz" << "bj" << "black" << "blackfriday" << "blanco" << "blockbuster" << "blog" << "bloomberg" << "bloomingdales" << "blue" << "bm" << "bms" << "bmw" << "bn" << "bnl" << "bnpparibas" << "bo" << "boats" << "boehringer" << "bofa" << "bom" << "bond" << "boo" << "book" << "booking" << "boots" << "bosch" << "bostik" << "boston" << "bot" << "boutique" << "box" << "br" << "bradesco" << "bridgestone" << "broadway" << "broker" << "brother" << "brussels" << "bs" << "bt" << "budapest" << "bugatti" << "buick" << "build" << "builders" << "business" << "buy" << "buzz" << "bv" << "bw" << "bway" << "by" << "bz" << "bzh" << "ca" << "cab" << "cadillac" << "cafe" << "cal" << "call" << "calvinklein" << "cam" << "camera" << "camp" << "canalplus" << "cancerresearch" << "canon" << "capetown" << "capital" << "capitalone" << "car" << "caravan" << "cards" << "care" << "career" << "careers" << "carinsurance" << "cars" << "cartier" << "casa" << "case" << "caseih" << "cash" << "cashbackbonus" << "casino" << "cat" << "catalonia" << "catering" << "catholic" << "cba" << "cbn" << "cbre" << "cbs" << "cc" << "cd" << "ceb" << "center" << "ceo" << "cern" << "cf" << "cfa" << "cfd" << "cg" << "ch" << "chanel" << "changiairport" << "channel" << "charity" << "chartis" << "chase" << "chat" << "cheap" << "chesapeake" << "chevrolet" << "chevy" << "chintai" << "chk" << "chloe" << "christmas" << "chrome" << "chrysler" << "church" << "ci" << "cimb" << "cipriani" << "circle" << "cisco" << "citadel" << "citi" << "citic" << "city" << "cityeats" << "ck" << "cl" << "claims" << "cleaning" << "click" << "clinic" << "clinique" << "clothing" << "cloud" << "club" << "clubmed" << "cm" << "cn" << "co" << "coach" << "codes" << "coffee" << "college" << "cologne" << "com" << "comcast" << "commbank" << "community" << "company" << "compare" << "computer" << "comsec" << "condos" << "connectors" << "construction" << "consulting" << "contact" << "contractors" << "cooking" << "cookingchannel" << "cool" << "coop" << "corp" << "corsica" << "country" << "coupon" << "coupons" << "courses" << "cpa" << "cr" << "credit" << "creditcard" << "creditunion" << "cricket" << "crown" << "crs" << "cruise" << "cruises" << "csc" << "cu" << "cuisinella" << "cv" << "cw" << "cx" << "cy" << "cymru" << "cyou" << "cz" << "dabur" << "dad" << "dance" << "data" << "date" << "dating" << "datsun" << "day" << "dclk" << "dds" << "de" << "deal" << "dealer" << "deals" << "degree" << "delivery" << "dell" << "delmonte" << "deloitte" << "delta" << "democrat" << "dental" << "dentist" << "desi" << "design" << "deutschepost" << "dev" << "dhl" << "diamonds" << "diet" << "digikey" << "digital" << "direct" << "directory" << "discount" << "discover" << "dish" << "diy" << "dj" << "dk" << "dm" << "dnb" << "dnp" << "do" << "docomo" << "docs" << "doctor" << "dodge" << "dog" << "doha" << "domains" << "doosan" << "dot" << "dotafrica" << "download" << "drive" << "dstv" << "dtv" << "dubai" << "duck" << "dunlop" << "duns" << "dupont" << "durban" << "dvag" << "dvr" << "dwg" << "dz" << "earth" << "eat" << "ec" << "eco" << "ecom" << "edeka" << "edu" << "education" << "ee" << "eg" << "email" << "emerck" << "emerson" << "energy" << "engineer" << "engineering" << "enterprises" << "epost" << "epson" << "equipment" << "er" << "ericsson" << "erni" << "es" << "esq" << "estate" << "esurance" << "et" << "etisalat" << "eu" << "eurovision" << "eus" << "events" << "everbank" << "exchange" << "expert" << "exposed" << "express" << "extraspace" << "fage" << "fail" << "fairwinds" << "faith" << "family" << "fan" << "fans" << "farm" << "farmers" << "fashion" << "fast" << "fedex" << "feedback" << "ferrari" << "ferrero" << "fi" << "fiat" << "fidelity" << "fido" << "film" << "final" << "finance" << "financial" << "financialaid" << "finish" << "fire" << "firestone" << "firmdale" << "fish" << "fishing" << "fit" << "fitness" << "fj" << "fk" << "flickr" << "flights" << "flir" << "florist" << "flowers" << "fls" << "flsmidth" << "fly" << "fm" << "fo" << "foo" << "food" << "foodnetwork" << "football" << "ford" << "forex" << "forsale" << "forum" << "foundation" << "fox" << "fr" << "free" << "fresenius" << "frl" << "frogans" << "frontdoor" << "frontier" << "ftr" << "fujitsu" << "fujixerox" << "fun" << "fund" << "furniture" << "futbol" << "fyi" << "ga" << "gal" << "gallery" << "gallo" << "gallup" << "game" << "games" << "gap" << "garden" << "garnier" << "gay" << "gb" << "gbiz" << "gcc" << "gd" << "gdn" << "ge" << "gea" << "gecompany" << "ged" << "gent" << "genting" << "george" << "gf" << "gg" << "ggee" << "gh" << "gi" << "gift" << "gifts" << "gives" << "giving" << "gl" << "glade" << "glass" << "gle" << "global" << "globalx" << "globo" << "gm" << "gmail" << "gmbh" << "gmc" << "gmo" << "gmx" << "gn" << "godaddy" << "gold" << "goldpoint" << "golf" << "goo" << "goodhands" << "goodyear" << "goog" << "google" << "gop" << "got" << "gotv" << "gov" << "gp" << "gq" << "gr" << "grainger" << "graphics" << "gratis" << "gree" << "green" << "gripe" << "grocery" << "group" << "gs" << "gt" << "gu" << "guardian" << "guardianlife" << "guardianmedia" << "gucci" << "guge" << "guide" << "guitars" << "guru" << "gw" << "gy" << "hair" << "halal" << "hamburg" << "hangout" << "haus" << "hbo" << "hdfc" << "hdfcbank" << "health" << "healthcare" << "heart" << "heinz" << "help" << "helsinki" << "here" << "hermes" << "hgtv" << "hilton" << "hiphop" << "hisamitsu" << "hitachi" << "hiv" << "hk" << "hkt" << "hm" << "hn" << "hockey" << "holdings" << "holiday" << "home" << "homedepot" << "homegoods" << "homes" << "homesense" << "honda" << "honeywell" << "horse" << "hospital" << "host" << "hosting" << "hot" << "hoteis" << "hotel" << "hoteles" << "hotels" << "hotmail" << "house" << "how" << "hr" << "hsbc" << "ht" << "htc" << "hu" << "hughes" << "hyatt" << "hyundai" << "ibm" << "icbc" << "ice" << "icu" << "id" << "idn" << "ie" << "ieee" << "ifm" << "iinet" << "ikano" << "il" << "im" << "imamat" << "imdb" << "immo" << "immobilien" << "in" << "inc" << "indians" << "industries" << "infiniti" << "info" << "infosys" << "infy" << "ing" << "ink" << "institute" << "insurance" << "insure" << "int" << "intel" << "international" << "intuit" << "investments" << "io" << "ipiranga" << "iq" << "ir" << "ira" << "irish" << "is" << "iselect" << "islam" << "ismaili" << "ist" << "istanbul" << "it" << "itau" << "itv" << "iveco" << "iwc" << "jaguar" << "java" << "jcb" << "jcp" << "je" << "jeep" << "jetzt" << "jewelry" << "jio" << "jlc" << "jll" << "jm" << "jmp" << "jnj" << "jo" << "jobs" << "joburg" << "jot" << "joy" << "jp" << "jpmorgan" << "jpmorganchase" << "jprs" << "juegos" << "juniper" << "justforu" << "kaufen" << "kddi" << "ke" << "kerastase" << "kerryhotels" << "kerrylogisitics" << "kerryproperties" << "ketchup" << "kfh" << "kg" << "kh" << "ki" << "kia" << "kid" << "kids" << "kiehls" << "kim" << "kinder" << "kindle" << "kitchen" << "kiwi" << "km" << "kn" << "koeln" << "komatsu" << "konami" << "kone" << "kosher" << "kp" << "kpmg" << "kpn" << "kr" << "krd" << "kred" << "kuokgroup" << "kw" << "ky" << "kyknet" << "kyoto" << "kz" << "la" << "lacaixa" << "ladbrokes" << "lamborghini" << "lamer" << "lancaster" << "lancia" << "lancome" << "land" << "landrover" << "lanxess" << "lasalle" << "lat" << "latino" << "latrobe" << "law" << "lawyer" << "lb" << "lc" << "lds" << "lease" << "leclerc" << "lefrak" << "legal" << "lego" << "lexus" << "lgbt" << "li" << "liaison" << "lidl" << "life" << "lifeinsurance" << "lifestyle" << "lighting" << "like" << "lilly" << "limited" << "limo" << "lincoln" << "linde" << "link" << "lipsy" << "live" << "livestrong" << "living" << "lixil" << "lk" << "llc" << "llp" << "loan" << "loans" << "locker" << "locus" << "loft" << "lol" << "london" << "loreal" << "lotte" << "lotto" << "love" << "lpl" << "lplfinancial" << "lr" << "ls" << "lt" << "ltd" << "ltda" << "lu" << "lundbeck" << "lupin" << "luxe" << "luxury" << "lv" << "ly" << "ma" << "macys" << "madrid" << "maif" << "mail" << "maison" << "makeup" << "man" << "management" << "mango" << "map" << "market" << "marketing" << "markets" << "marriott" << "marshalls" << "maserati" << "matrix" << "mattel" << "maybelline" << "mba" << "mc" << "mcd" << "mcdonalds" << "mckinsey" << "md" << "me" << "med" << "media" << "medical" << "meet" << "melbourne" << "meme" << "memorial" << "men" << "menu" << "meo" << "merck" << "merckmsd" << "metlife" << "mf" << "mg" << "mh" << "miami" << "microsoft" << "mih" << "mii" << "mil" << "mini" << "mint" << "mit" << "mitek" << "mitsubishi" << "mk" << "ml" << "mlb" << "mls" << "mm" << "mma" << "mn" << "mnet" << "mo" << "mobi" << "mobile" << "mobily" << "moda" << "moe" << "moi" << "mom" << "monash" << "money" << "monster" << "montblanc" << "mopar" << "mormon" << "mortgage" << "moscow" << "moto" << "motorcycles" << "mov" << "movie" << "movistar" << "mozaic" << "mp" << "mq" << "mr" << "mrmuscle" << "mrporter" << "ms" << "msd" << "mt" << "mtn" << "mtpc" << "mtr" << "mu" << "multichoice" << "museum" << "music" << "mutual" << "mutualfunds" << "mutuelle" << "mv" << "mw" << "mx" << "my" << "mz" << "mzansimagic" << "na" << "nab" << "nadex" << "nagoya" << "name" << "naspers" << "nationwide" << "natura" << "navy" << "nba" << "nc" << "ne" << "nec" << "net" << "netaporter" << "netbank" << "netflix" << "network" << "neustar" << "new" << "newholland" << "news" << "next" << "nextdirect" << "nexus" << "nf" << "nfl" << "ng" << "ngo" << "nhk" << "ni" << "nico" << "nike" << "nikon" << "ninja" << "nissan" << "nissay" << "nl" << "no" << "nokia" << "northlandinsurance" << "northwesternmutual" << "norton" << "now" << "nowruz" << "nowtv" << "np" << "nr" << "nra" << "nrw" << "ntt" << "nu" << "nyc" << "nz" << "obi" << "observer" << "off" << "office" << "okinawa" << "olayan" << "olayangroup" << "oldnavy" << "ollo" << "olympus" << "om" << "omega" << "one" << "ong" << "onl" << "online" << "onyourside" << "ooo" << "open" << "oracle" << "orange" << "org" << "organic" << "orientexpress" << "origins" << "osaka" << "otsuka" << "ott" << "overheidnl" << "ovh" << "pa" << "page" << "pamperedchef" << "panasonic" << "panerai" << "paris" << "pars" << "partners" << "parts" << "party" << "passagens" << "patagonia" << "patch" << "pay" << "payu" << "pccw" << "pe" << "persiangulf" << "pet" << "pets" << "pf" << "pfizer" << "pg" << "ph" << "pharmacy" << "phd" << "philips" << "phone" << "photo" << "photography" << "photos" << "physio" << "piaget" << "pics" << "pictet" << "pictures" << "pid" << "pin" << "ping" << "pink" << "pioneer" << "piperlime" << "pitney" << "pizza" << "pk" << "pl" << "place" << "play" << "playstation" << "plumbing" << "plus" << "pm" << "pn" << "pnc" << "pohl" << "poker" << "politie" << "polo" << "porn" << "post" << "pr" << "pramerica" << "praxi" << "press" << "prime" << "pro" << "prod" << "productions" << "prof" << "progressive" << "promo" << "properties" << "property" << "protection" << "pru" << "prudential" << "ps" << "pt" << "pub" << "pw" << "pwc" << "py" << "qa" << "qpon" << "qtel" << "quebec" << "quest" << "qvc" << "racing" << "radio" << "raid" << "ram" << "re" << "read" << "realestate" << "realtor" << "realty" << "recipes" << "red" << "redken" << "redstone" << "redumbrella" << "rehab" << "reise" << "reisen" << "reit" << "reliance" << "ren" << "rent" << "rentals" << "repair" << "report" << "republican" << "rest" << "restaurant" << "retirement" << "review" << "reviews" << "rexroth" << "rich" << "richardli" << "ricoh" << "rightathome" << "ril" << "rio" << "rip" << "rmit" << "ro" << "rocher" << "rocks" << "rockwool" << "rodeo" << "rogers" << "roma" << "room" << "root" << "rs" << "rsvp" << "ru" << "rugby" << "ruhr" << "run" << "rw" << "rwe" << "ryukyu" << "sa" << "saarland" << "safe" << "safety" << "safeway" << "sakura" << "sale" << "salon" << "samsclub" << "samsung" << "sandvik" << "sandvikcoromant" << "sanofi" << "sap" << "sapo" << "sapphire" << "sarl" << "sas" << "save" << "saxo" << "sb" << "sbi" << "sbs" << "sc" << "sca" << "scb" << "schaeffler" << "schmidt" << "scholarships" << "school" << "schule" << "schwarz" << "schwarzgroup" << "science" << "scjohnson" << "scor" << "scot" << "sd" << "se" << "search" << "seat" << "secure" << "security" << "seek" << "select" << "sener" << "services" << "ses" << "seven" << "sew" << "sex" << "sexy" << "sfr" << "sg" << "sh" << "shangrila" << "sharp" << "shaw" << "shell" << "shia" << "shiksha" << "shoes" << "shop" << "shopping" << "shopyourway" << "shouji" << "show" << "showtime" << "shriram" << "si" << "silk" << "sina" << "singles" << "site" << "sj" << "sk" << "ski" << "skin" << "skolkovo" << "sky" << "skydrive" << "skype" << "sl" << "sling" << "sm" << "smart" << "smile" << "sn" << "sncf" << "so" << "soccer" << "social" << "softbank" << "software" << "sohu" << "solar" << "solutions" << "song" << "sony" << "soy" << "spa" << "space" << "spiegel" << "sport" << "sports" << "spot" << "spreadbetting" << "sr" << "srl" << "srt" << "st" << "stada" << "staples" << "star" << "starhub" << "statebank" << "statefarm" << "statoil" << "stc" << "stcgroup" << "stockholm" << "storage" << "store" << "stream" << "stroke" << "studio" << "study" << "style" << "su" << "sucks" << "supersport" << "supplies" << "supply" << "support" << "surf" << "surgery" << "suzuki" << "sv" << "svr" << "swatch" << "swiftcover" << "swiss" << "sx" << "sy" << "sydney" << "symantec" << "systems" << "sz" << "tab" << "taipei" << "talk" << "taobao" << "target" << "tata" << "tatamotors" << "tatar" << "tattoo" << "tax" << "taxi" << "tc" << "tci" << "td" << "tdk" << "team" << "tech" << "technology" << "tel" << "telecity" << "telefonica" << "temasek" << "tennis" << "terra" << "teva" << "tf" << "tg" << "th" << "thai" << "thd" << "theater" << "theatre" << "theguardian" << "thehartford" << "tiaa" << "tickets" << "tienda" << "tiffany" << "tips" << "tires" << "tirol" << "tj" << "tjmaxx" << "tjx" << "tk" << "tkmaxx" << "tl" << "tm" << "tmall" << "tn" << "to" << "today" << "tokyo" << "tools" << "top" << "toray" << "toshiba" << "total" << "tour" << "tours" << "town" << "toyota" << "toys" << "tp" << "tr" << "trade" << "tradershotels" << "trading" << "training" << "transformers" << "translations" << "transunion" << "travel" << "travelchannel" << "travelers" << "travelersinsurance" << "travelguard" << "trust" << "trv" << "tt" << "tube" << "tui" << "tunes" << "tushu" << "tv" << "tvs" << "tw" << "tz" << "ua" << "ubank" << "ubs" << "uconnect" << "ug" << "uk" << "ultrabook" << "um" << "ummah" << "unicom" << "unicorn" << "university" << "uno" << "uol" << "ups" << "us" << "uy" << "uz" << "va" << "vacations" << "vana" << "vanguard" << "vanish" << "vc" << "ve" << "vegas" << "ventures" << "verisign" << "vermögensberater" << "vermögensberatung" << "versicherung" << "vet" << "vg" << "vi" << "viajes" << "video" << "vig" << "viking" << "villas" << "vin" << "vip" << "virgin" << "visa" << "vision" << "vista" << "vistaprint" << "viva" << "vivo" << "vlaanderen" << "vn" << "vodka" << "volkswagen" << "volvo" << "vons" << "vote" << "voting" << "voto" << "voyage" << "vu" << "vuelos" << "wales" << "walmart" << "walter" << "wang" << "wanggou" << "warman" << "watch" << "watches" << "weather" << "weatherchannel" << "web" << "webcam" << "weber" << "webjet" << "webs" << "website" << "wed" << "wedding" << "weibo" << "weir" << "wf" << "whoswho" << "wien" << "wiki" << "williamhill" << "wilmar" << "win" << "windows" << "wine" << "winners" << "wme" << "wolterskluwer" << "woodside" << "work" << "works" << "world" << "wow" << "ws" << "wtc" << "wtf" << "xbox" << "xerox" << "xfinity" << "xihuan" << "xin" << "xperia" << "xxx" << "xyz" << "yachts" << "yahoo" << "yamaxun" << "yandex" << "ye" << "yellowpages" << "yodobashi" << "yoga" << "yokohama" << "you" << "youtube" << "yt" << "yu" << "yun" << "za" << "zappos" << "zara" << "zero" << "zip" << "zippo" << "zm" << "zone" << "zuerich" << "zulu" << "zw";

    dynadotExt << "com" << "org" << "co" << "net" << "club" << "me" << "info" << "global" << "link" << "space" << "top" << "in" << "biz" << "xyz" << "uk" << "moe" << "us" << "tv" << "website" << "ninja" << "ca" << "中国" << "cn" << "mobi" << "ws" << "eu" << "name" << "mx" << "sx" << "sexy" << "de" << "asia" << "im" << "ooo" << "vc" << "pw" << "be" << "so" << "在线" << "click" << "at" << "guru" << "world" << "भारत" << "forsale" << "орг" << "机构" << "онлайн" << "pizza" << "nyc" << "limited" << "network" << "help" << "pub" << "tel" << "red" << "toys" << "lt" << "sc" << "nl" << "la" << "bz" << "中文网" << "pl" << "fm" << "xxx" << "ph" << "lc" << "cc" << "uno" << "singles" << "graphics" << "diet" << "life" << "kaufen" << "business" << "vision" << "software" << "deals" << "social" << "band" << "公司" << "rocks" << "vegas" << "wang" << "reviews" << "holdings" << "domains" << "city" << "discount" << "market" << "events" << "media" << "संगठन" << "clothing" << "tips" << "today" << "photography" << "organic" << "mn" << "ag" << "bike" << "plumbing" << "ventures" << "camera" << "equipment" << "estate" << "gallery" << "lighting" << "contractors" << "land" << "technology" << "construction" << "directory" << "kitchen" << "diamonds" << "enterprises" << "voyage" << "shoes" << "careers" << "photos" << "recipes" << "limo" << "cab" << "company" << "computer" << "center" << "systems" << "academy" << "management" << "menu" << "berlin" << "training" << "solutions" << "support" << "builders" << "email" << "education" << "institute" << "repair" << "camp" << "glass" << "ruhr" << "ceo" << "شبكة" << "сайт" << "solar" << "coffee" << "international" << "house" << "florist" << "みんな" << "holiday" << "marketing" << "rich" << "tattoo" << "buzz" << "gift" << "guitars" << "pics" << "photo" << "viajes" << "farm" << "codes" << "onl" << "pink" << "shiksha" << "boutique" << "kim" << "blue" << "移动" << "cheap" << "zone" << "build" << "cool" << "watch" << "kiwi" << "agency" << "bargains" << "actor" << "best" << "dance" << "wien" << "wiki" << "works" << "expert" << "luxury" << "democrat" << "immobilien" << "futbol" << "moda" << "foundation" << "exposed" << "vacations" << "villas" << "flights" << "rentals" << "cruises" << "condos" << "tienda" << "properties" << "maison" << "nagoya" << "productions" << "partners" << "dating" << "bid" << "trade" << "webcam" << "qpon" << "archi" << "consulting" << "cards" << "catering" << "cleaning" << "community" << "parts" << "industries" << "supplies" << "supply" << "tools" << "tokyo" << "christmas" << "party" << "adult" << "clinic" << "fit" << "one" << "finance" << "casino" << "chat" << "juegos" << "voto" << "style" << "bingo" << "dental" << "apartments" << "porn" << "video" << "school" << "football" << "tennis" << "koeln" << "quebec" << "degree" << "tires" << "science" << "care" << "coach" << "sarl" << "navy" << "fund" << "black" << "cymru" << "legal" << "cooking" << "career" << "attorney" << "gratis" << "republican" << "credit" << "dentist" << "blackfriday" << "بازار" << "investments" << "auction" << "cash" << "gripe" << "london" << "wtf" << "moscow" << "rest" << "energy" << "voting" << "hiphop" << "tax" << "fail" << "fashion" << "immo" << "engineer" << "work" << "healthcare" << "cologne" << "lease" << "restaurant" << "wedding" << "gifts" << "press" << "airforce" << "creditcard" << "army" << "host" << "yoga" << "surgery" << "rehab" << "claims" << "mortgage" << "place" << "gives" << "flowers" << "country" << "casa" << "money" << "москва" << "loans" << "okinawa" << "direct" << "bio" << "lawyer" << "capital" << "fish" << "poker" << "garden" << "soy" << "scot" << "lgbt" << "digital" << "hosting" << "memorial" << "associates" << "reisen" << "surf" << "jetzt" << "haus" << "vote" << "town" << "horse" << "beer" << "fishing" << "how" << "audio" << "fitness" << "vet" << "guide" << "yokohama" << "design" << "bar" << "schule" << "services" << "sale" << "ngo" << "green" << "report" << "pictures" << "delivery" << "osaka" << "accountants" << "rodeo" << "ink" << "vodka" << "网络" << "rip" << "furniture" << "university" << "exchange" << "property" << "wales" << "engineering" << "church" << "cricket" << "insure" << "desi" << "financial";

    if(api == "mashape")
        return mashapeExt.contains(tld);
    else if(api == "dynadot")
        return dynadotExt.contains(tld);

    return false;
}

qint64 Tools::getCurrentTimestamp()
{
    return QDateTime::currentSecsSinceEpoch();
}

QString Tools::getPlatformCategories(QJsonArray categoriesJson, QJsonObject parentObject)
{
    QStringList categories;
    categories.clear();

    QJsonObject::iterator categorieIterator;
    for (categorieIterator = parentObject.begin(); categorieIterator != parentObject.end(); ++categorieIterator)
    {
        if(categoriesJson.contains(categorieIterator.key().toInt()))
        {
             categories.append( categorieIterator->toObject().value("name").toString());
        }
    }

        return (categories.isEmpty()) ? "" : categories.join(" / ");
}

bool Tools::plateformHasHeader(QJsonObject fileHeaders, QList<QPair<QByteArray, QByteArray> > serverHeaders)
{
    int i;
    for(i=0;i<serverHeaders.size();i++)
    {
        QJsonObject::iterator headerIterator;
        for (headerIterator = fileHeaders.begin(); headerIterator != fileHeaders.end(); ++headerIterator)
        {
            QString regexStr = headerIterator.value().toString().replace(QRegExp("\\\\;[^\"]+"),"");
            QRegularExpression headerReg(regexStr, QRegularExpression::CaseInsensitiveOption);
            QRegularExpressionMatch headerMatch = headerReg.match(serverHeaders.at(i).second);
            if(headerIterator.key().toUtf8() == serverHeaders.at(i).first && headerMatch.hasMatch())
                return true;
        }
    }
    return false;
}

bool Tools::platformHasValue(QJsonValue jsonValue, QString src)
{
    if(jsonValue.isArray())
    {
        QJsonArray htmlArray = jsonValue.toArray();
        foreach(QJsonValue regexValue, htmlArray)
        {
            QString regexStr = regexValue.toString().replace(QRegExp("\\\\;[^\"]+"),"").replace(" ","\\s");
            QRegularExpression htmlRegex(regexStr, QRegularExpression::CaseInsensitiveOption);
            QRegularExpressionMatch match = htmlRegex.match(src);
            if (match.hasMatch())
            return true;
        }
    }
    else if(jsonValue.isString())
    {
        QString regexStr = jsonValue.toString().replace(QRegExp("\\\\;[^\"]+"),"").replace(" ","\\s");
        QRegularExpression htmlRegex(regexStr, QRegularExpression::CaseInsensitiveOption);
        QRegularExpressionMatch match = htmlRegex.match(src);
        if (match.hasMatch())
        return true;
    }
    return false;
}

bool Tools::platformHasScript(QJsonValue jsonValue, QString src)
{
    if(jsonValue.isArray())
    {
        QJsonArray htmlArray = jsonValue.toArray();
        foreach(QJsonValue regexValue, htmlArray)
        {
            QString cleanRegex = regexValue.toString().replace(QRegExp("\\\\;[^\"]+"),"").replace(" ","\\s");
            QString regexStr = "<script.+\\s+src=[\"']([^\"]*" + cleanRegex + "[^\"]*)[\"']";
            QRegularExpression htmlRegex(regexStr, QRegularExpression::CaseInsensitiveOption);
            QRegularExpressionMatch match = htmlRegex.match(src);
            if (match.hasMatch())
            return true;
        }
    }
    else if(jsonValue.isString())
    {
        QString cleanRegex = jsonValue.toString().replace(QRegExp("\\\\;[^\"]+"),"").replace(" ","\\s");
        QString regexStr = "<script.+\\s+src=[\"']([^\"]*" + cleanRegex + "[^\"]*)[\"']";
        QRegularExpression htmlRegex(regexStr, QRegularExpression::CaseInsensitiveOption);
        QRegularExpressionMatch match = htmlRegex.match(src);
        if (match.hasMatch())
        return true;
    }
    return false;
}

bool Tools::platformHasMeta(QJsonObject jsonObj, QString src)
{
    QJsonObject::iterator metaIterator;
    for (metaIterator = jsonObj.begin(); metaIterator != jsonObj.end(); ++metaIterator)
    {
        // We built the regex string

        // 2 steps, first we check if name is present, and after we check the content
        QString cleanRegex = metaIterator.value().toString().replace(QRegExp("\\\\;[^\"]+"),"").replace(" ","\\s");
        QString regexStr = "<meta.+name=[\"']" + metaIterator.key() + "[\"'][^>]+>";
        QRegularExpression metaRegex(regexStr, QRegularExpression::CaseInsensitiveOption);
        QRegularExpressionMatch metaMatch = metaRegex.match(src);
        if(metaMatch.hasMatch())
        {
            QRegularExpression metaContentRegex(".+\\s+content=[\"']([^\"]*" + cleanRegex + "[^\"]*)[\"']",QRegularExpression::CaseInsensitiveOption);
            QRegularExpressionMatch metaContentMatch = metaContentRegex.match(metaMatch.captured(0));
            if(metaContentMatch.hasMatch())
            return true;
        }
    }
    return false;
}


QString Tools::platformDetector(QString src, QList<QPair<QByteArray, QByteArray> > headers)
{
    // Inspire from Wappalyzer
    QString appPathStr    = QDir::toNativeSeparators(QApplication::applicationDirPath()+QDir::separator());
    QJsonDocument jsonDoc;
    QJsonObject parentObject, jsonObj;
    QJsonArray categoryToCheck;
    QString output("Not Found");
    QList<double> includeCats;
    includeCats << 1 << 2 << 6 << 7 << 8 << 11 << 18;

//    QFile platformsDoc(appPathStr+"/dist/platforms.json");
    QFile platformsDoc(appPathStr+"/dist/apps.json");
    if (!platformsDoc.open(QIODevice::ReadOnly | QIODevice::Text))
           return output;
    QByteArray platformsContent = platformsDoc.readAll();
    platformsDoc.close();

    jsonDoc = QJsonDocument::fromJson(platformsContent);
    parentObject = jsonDoc.object();
    jsonObj = parentObject.value("apps").toObject();


    QJsonObject::iterator platformIterator;
    for(int indexCat=0;indexCat<includeCats.size();indexCat++)
    {
    for (platformIterator = jsonObj.begin(); platformIterator != jsonObj.end(); ++platformIterator)
    {
        QString platformName = platformIterator.key();

            if(platformIterator.value().isObject())
            {
                QJsonObject platformObj = platformIterator.value().toObject();
                categoryToCheck = platformObj.value("cats").toArray();

                    if(categoryToCheck.toVariantList().contains(includeCats.at(indexCat)))
                    {
                        // We check headers
                        if(platformObj.value("headers").isObject())
                        {
                            if(Tools::plateformHasHeader(platformObj.value("headers").toObject(),headers))
                            {
//                                qDebug() << "Headers found";
                            output = Tools::getPlatformCategories(platformObj.value("cats").toArray(),parentObject.value("categories").toObject()) + " | " + platformName;
                            return output;
                            }
                        }
                        // We check  meta
                        if(Tools::platformHasMeta(platformObj.value("meta").toObject(),src))
                        {
//                            qDebug() << "Meta found";
                            output = Tools::getPlatformCategories(platformObj.value("cats").toArray(),parentObject.value("categories").toObject()) + " | " + platformName;
                            return output;
                        }
                        // We check scripts
                        if(Tools::platformHasScript(platformObj.value("script"),src))
                        {
//                            qDebug() << "script found";
                            output = Tools::getPlatformCategories(platformObj.value("cats").toArray(),parentObject.value("categories").toObject()) + " | " + platformName;
                            return output;
                        }
                        // We check html
                        if(Tools::platformHasValue(platformObj.value("html"),src))
                        {
//                            qDebug() << "Html found";
                            output = Tools::getPlatformCategories(platformObj.value("cats").toArray(),parentObject.value("categories").toObject()) + " | " + platformName;
                            return output;
                        }

                    }


                }
            }
    }
    return output;

}
