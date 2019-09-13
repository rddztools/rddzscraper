#ifndef SCRAPTABLE_H
#define SCRAPTABLE_H

#include <QAbstractTableModel>
#include <QBrush>
#include <QHashIterator>
#include <QSortFilterProxyModel>
#include <QtConcurrent/QtConcurrent>
#include <QUrl>
#include <qdebug.h>
#include <qvariant.h>

#define MAXROWS 200

class scrapTable : public QAbstractTableModel
{
    Q_OBJECT
public:
    scrapTable(QObject *parent=0);

//    enum Column { urlCol, prCol, httpCol, dfCol, blCol, cfCol, tfCol, oblCol, ipCol, colCount };


    int colCount;
    int redirRetry;
    int previousRedirLeft;
    int columnClicked;

    int visibleRows;

    QString currentTableModel;
//    QVector<QString> tableHeaderData;
//    QVector<QString> tableHeaderToolTip;

    QHash<int, QString> tableHeaderData;
    QHash<int, QString> tableHeaderToolTip;

    QHash<QString, int> columnIndexes;
    QHash<int,int> headerColSize;


    int rowCount(const QModelIndex &parent) const;
    int columnCount(const QModelIndex &parent) const;
    QVariant data(const QModelIndex &index, int role) const;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const;
    bool setHeaderData(int section, Qt::Orientation orientation, const QVariant & value, int role = Qt::EditRole);
    bool removeRows(int row, int count, const QModelIndex & parent = QModelIndex());
    void sort(int column, Qt::SortOrder order=Qt::AscendingOrder);
    void clearHeaderData();
    void clearHeaderToolTip();
    void resetColumnIndexes();

    Qt::ItemFlags flags(const QModelIndex &index) const;
    int setItem(int row, int col, QVariant value );

    //void updateLayout();
    void updateTableView(QString url, QVariant value, int column);
    void updateMultipleColumn(QString url, QHash<int, QVariant> values, bool basedOnUrl, bool domainOnly);
    void updateMultipleRows(QHash<QString, QHash<int, QVariant> > resultsStack, bool basedOnUrl, bool domainOnly);
    void updateAllDomains(QString url, QVariant value, int column, bool domainOnly);

    void    clearScrapResultsHash();
    void    buildScrapResultsHash(QList<QVariant> list, int col, int startAt=-1, QString mode="col");
    void    reBuildScrapResults(QMap<int, QVariantList> newMapResults);

    int getNbItemsByCol(int column, bool redir=false);
    QVariant getItemByIndex(QModelIndex modelIndex);
    QStringList getAllUrls();
    QVariantList getItemsByColumn(int col);
    int getScrapResultSize();


    // Remove functions
    int removeBadUrls();
    int removeDuplicateUrl();
    int removeDuplicateDomain(QString metricCol="PR", QString apiUsed="Free");
    int removeSubdomains();
    int removeSelectedRows(QModelIndexList indexList);
    int removeColumnContaining(int column, QString search, bool containing=true);
    int removeColumnMatching(int column, QString userRegex, bool containing=true);
    int removeRegexMask(QString userRegex);
    int removeColumnByFilter(int column, double value, QString filter);

    // Search and replace
    void searchAndReplace(QString searchFor, QString replaceBy, bool regex, bool ignoreScheme);

    void trimToRoot();
    void trimToLastFolder();
    void keepOnlyDomain();


    QMap<int, QVariantList> scrapResults;
//    QHash<int, QVariantList> scrapResults;

    QSortFilterProxyModel *proxyModel;

    void updateTableViewMapping(QPair<QString, QString> firstPair, QPair<QString, QString> secondPair, QVariant value, int col);


signals:
    void            disableCursor();
    void            enableCursor();

public slots:
    void updateLayout();
    void    clearColumn();
    void    scrollBarValueChanged(int value);

private:
    QBrush okBrush;
    QBrush errorBrush;
    QBrush redirBrush;
    QMutex scraptableLocker;

    QHash<QString,QVariantList> _stackScrapResults;
    QMap<int, QVariantList> _subScrapResults;
    int    _currentScrollBarOffset;

    void quickSort(int left, int right, int column, int sorting);
    void    customTableSorting(int column, int sorting);
};

#endif // SCRAPTABLE_H
