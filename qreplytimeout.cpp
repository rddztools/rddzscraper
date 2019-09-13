#include "qreplytimeout.h"


QReplyTimeout::QReplyTimeout(QNetworkReply* reply, const int timeout, Scraper *scraper) : QObject(reply) {
  Q_ASSERT(reply);
    this->scraper = scraper;

  if (reply) {
    QTimer::singleShot(timeout, this, SLOT(timeout()));
//    qDebug() << "[" <<  QDateTime::currentDateTime() << "] Launch request : " << reply->url() ;
  }
}

void QReplyTimeout::timeout()
{
    QNetworkReply* reply = static_cast<QNetworkReply*>(parent());
    int type;
    QString url;

    if (reply->isRunning() && !reply->isFinished())
    {
        type = reply->property("type").toInt();
        url = reply->property("realUrl").toString();


        switch(type){
        case NETWORK_HTTPSTATUS:
            this->scraper->appendLog(tr("[HTTP Status] Thread timeout for URL : %1").arg(reply->property("realUrl").toString()), "warning");
            reply->close();
            break;
        case NETWORK_REDIR:
            this->scraper->appendLog(tr("[Resolve redirection] Thread timeout for URL : %1").arg(reply->property("realUrl").toString()), "warning");
            reply->close();
            break;
        case NETWORK_OBL:
            this->scraper->appendLog(tr("[OBL] Thread timeout for URL : %1").arg(reply->property("realUrl").toString()), "warning");
            reply->close();
            break;
        case NETWORK_DOFOLLOW:
            this->scraper->appendLog(tr("[Dofollow] Thread timeout for URL : %1").arg(reply->property("realUrl").toString()), "warning");
            reply->close();
            break;
        case NETWORK_LINK_ALIVE:
            this->scraper->appendLog(tr("[Link alive] Thread timeout for URL : %1").arg(reply->property("realUrl").toString()), "warning");
            reply->close();
            break;
        case NETWORK_PLATFORM:
            this->scraper->appendLog(tr("[Platform check] Thread timeout for URL : %1").arg(reply->property("realUrl").toString()), "warning");
            reply->close();
            break;
        case NETWORK_PROXY:
            this->scraper->appendLog(tr("[Proxy] Thread timeout for Proxy : %1").arg(reply->property("realUrl").toString()), "warning");
            reply->close();
            break;
        case NETWORK_URL_BL:
            this->scraper->appendLog(tr("[Backlinks] Thread timeout for Proxy : %1").arg(reply->property("realUrl").toString()), "warning");
            reply->close();
            break;
        case NETWORK_SCRAP:
            this->scraper->appendLog(tr("[Scrap] Thread timeout for URL : %1").arg(reply->property("realUrl").toString()), "warning");
            reply->close();
            break;

        }
    }
}
