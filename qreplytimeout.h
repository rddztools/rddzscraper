#ifndef QREPLYTIMEOUT_H
#define QREPLYTIMEOUT_H

#include <QObject>
#include <QNetworkReply>
#include <QTimer>
#include "scraper.h"


class QReplyTimeout : public QObject
{
      Q_OBJECT
public:
    QReplyTimeout(QNetworkReply* reply, const int timeout, Scraper *scraper);


private slots:
  void timeout();

private:
  Scraper *scraper;
};

#endif // QREPLYTIMEOUT_H
