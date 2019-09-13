#include "scraptable.h"
#include "tools.h"
#include <QTextCursor>
#include <QTextBlock>

scrapTable::scrapTable(QObject *parent)
    : QAbstractTableModel(parent)
{
    // We define color here
    this->okBrush.setColor(QColor(129,217,211));
    this->okBrush.setStyle(Qt::SolidPattern);
    this->errorBrush.setColor(QColor(255,114,95));
    this->errorBrush.setStyle(Qt::SolidPattern);
    this->redirBrush.setColor(QColor(255,234,115));
    this->redirBrush.setStyle(Qt::SolidPattern);

    this->colCount=0;
    this->currentTableModel = "scrap";
    redirRetry = 0;
    previousRedirLeft = 0;
    columnClicked = 0;
    _currentScrollBarOffset = 0;

    _stackScrapResults.clear();

    qRegisterMetaType< QList<QPersistentModelIndex> >("QList<QPersistentModelIndex>");
    qRegisterMetaType<QTextCursor>("QTextCursor");
    qRegisterMetaType<QTextBlock>("QTextBlock");
    qRegisterMetaType<QVector<int> >("QVector<int>");
    qRegisterMetaType<QAbstractItemModel::LayoutChangeHint>("QAbstractItemModel::LayoutChangeHint");
}

int scrapTable::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    return this->scrapResults.size();
}

int scrapTable::columnCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    return colCount;
}


QVariant scrapTable::data(const QModelIndex &index, int role) const
{
    QVariantList defaultListVariant;
    QMutex ptilocker;

    if(!index.isValid())
        return QVariant();

    // We load only only visibleRow
    int row = index.row() - _currentScrollBarOffset;
    if(row > _subScrapResults.size() || row < 0)
        return QVariant();

    if(index.column() > this->colCount)
        return QVariant();

    if(!QVariant(row).canConvert(QMetaType::Int))
        return QVariant();

    if (role == Qt::BackgroundRole || role == Qt::DisplayRole)
    {
        ptilocker.lock();
        defaultListVariant.clear();
        defaultListVariant = _subScrapResults.value(row);
        ptilocker.unlock();
    }

    if (role == Qt::BackgroundRole)
    {

        if (this->columnIndexes.contains("httpCol"))
        {
            if (defaultListVariant.size()-1 >= this->columnIndexes.value("httpCol"))
            {
                if(defaultListVariant.at(this->columnIndexes.value("httpCol")).toInt() >= 200 && defaultListVariant.at(this->columnIndexes.value("httpCol")).toInt() < 300)
                    return this->okBrush;
                else if(defaultListVariant.at(this->columnIndexes.value("httpCol")).toInt() >= 300 && defaultListVariant.at(this->columnIndexes.value("httpCol")).toInt() < 400)
                    return this->redirBrush;
                else if(defaultListVariant.at(this->columnIndexes.value("httpCol")).toInt() >= 400)
                    return this->errorBrush;
            }
        }
    }

    if(role == Qt::DisplayRole)
    {
        if (defaultListVariant.size()-1 >= index.column())
        {
            if(defaultListVariant.at(index.column()).type() == QVariant::Double && index.column() != this->columnIndexes.value("blCol"))
            {
                return QString::number(defaultListVariant.at(index.column()).toDouble(),'f',2).toDouble();
            }
            else
                return defaultListVariant.at(index.column());
        }
        else
            return QVariant();
    }

    if (role == Qt::TextAlignmentRole )
    {
        if(index.column())
            return QVariant(Qt::AlignCenter | Qt::AlignVCenter);
        else
            return QVariant(Qt::AlignLeft | Qt::AlignVCenter);
    }

    return QVariant();
}

bool scrapTable::setHeaderData(int section, Qt::Orientation orientation, const QVariant &value, int role)
{
    if(orientation == Qt::Horizontal && (role == Qt::DisplayRole || role == Qt::EditRole))
    {
        this->tableHeaderData.insert(section, value.toString());
        return true;
    }
    if(orientation == Qt::Horizontal && role == Qt::ToolTipRole)
    {
        this->tableHeaderToolTip.insert(section, value.toString());
        return true;
    }
    return false;
}

bool scrapTable::removeRows(int row, int count, const QModelIndex &parent)
{
    if(scrapResults.isEmpty())
        return false;

    if(scrapResults.contains(row))
    {
        beginRemoveRows(parent,row,row + count -1);
        scrapResults.remove(row);
        endRemoveRows();
        return true;
    }
    return false;
}

void scrapTable::clearHeaderData()
{
    this->colCount = 0;
    this->tableHeaderData.clear();
    this->clearHeaderToolTip();
    this->resetColumnIndexes();
}

void scrapTable::clearHeaderToolTip()
{
    this->tableHeaderToolTip.clear();
}

void scrapTable::resetColumnIndexes()
{
    // Now just clear the qhash
    this->columnIndexes.clear();
}

QVariant scrapTable::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation == Qt::Horizontal)
    {
        if(role == Qt::DisplayRole)
        return this->tableHeaderData.value(section);
        if (role == Qt::ToolTipRole)
        return this->tableHeaderToolTip.value(section);
        if (role == Qt::SizeHintRole)
          return QSize(this->headerColSize.value(section),20);

         return QVariant();
    }
    if(orientation == Qt::Vertical)
        return section;

    return QVariant();
}

int scrapTable::setItem(int row, int col, QVariant value)
{
    if ( row < 0 || col < 0 || row >= scrapResults.size() || col >= colCount )
        return 0;	// error!

    if (scrapResults.contains(row))
    {
        scraptableLocker.lock();
        QList<QVariant> myList(scrapResults.value(row));
        if(myList.size() >= col)
        {
            myList.replace(col,value);
            scrapResults.insert(row,myList);
            emit dataChanged( createIndex( row, col ), createIndex( row, col ) );
            scraptableLocker.unlock();

            return 1;		// success!
        }
        else
        {
            return 0;
        }
    }
    return 0;
}

void scrapTable::updateLayout()
{
    this->scrollBarValueChanged(_currentScrollBarOffset);
}

void scrapTable::updateTableView(QString url, QVariant value, int column)
{
    if(column == -1)
        return;
    else
    {
        QMapIterator<int, QVariantList> hashIterator(scrapResults);
        hashIterator.toFront();
        while(hashIterator.hasNext())
        {
            hashIterator.next();

            if(hashIterator.value().at(this->columnIndexes.value("urlCol")).toString() == url)
                this->setItem(hashIterator.key(),column,value);
        }
    }
}



void scrapTable::updateTableViewMapping(QPair<QString,QString> firstPair, QPair<QString,QString> secondPair, QVariant value, int column)
{
    if(column == -1)
        return;
    else
    {
        scraptableLocker.lock();
        QMapIterator<int, QVariantList> hashIterator(scrapResults);
        scraptableLocker.unlock();

        hashIterator.toFront();
        while(hashIterator.hasNext())
        {
            hashIterator.next();

            if(hashIterator.value().at(this->columnIndexes.value(firstPair.first)).toString() == firstPair.second && hashIterator.value().at(this->columnIndexes.value(secondPair.first)).toString() == secondPair.second)
                this->setItem(hashIterator.key(),column,value);
        }
    }
}

void scrapTable::updateAllDomains(QString url, QVariant value, int column, bool domainOnly)
{
    if(column == -1)
        return;
    else
    {
        scraptableLocker.lock();
        QMapIterator<int, QVariantList> hashIterator(scrapResults);
        scraptableLocker.unlock();

        hashIterator.toFront();
        while(hashIterator.hasNext())
        {
            hashIterator.next();
                 if(Tools::getDomain(hashIterator.value().at(this->columnIndexes.value("urlCol")).toString(),domainOnly, false) == url)
                     this->setItem(hashIterator.key(),column,value);

        }
    }
}

void scrapTable::updateMultipleColumn(QString url, QHash<int, QVariant> values, bool basedOnUrl, bool domainOnly)
{
    QMapIterator<int, QVariantList> hashIterator(scrapResults);
    QHashIterator<int, QVariant> columnIterator(values);

    hashIterator.toFront();
    while(hashIterator.hasNext())
    {
        hashIterator.next();
        columnIterator.toFront(); // For each line, we restore the column hash pointer to front

        if(basedOnUrl == false) // Update domains only
        {
            if(Tools::getDomain(hashIterator.value().at(this->columnIndexes.value("urlCol")).toString(),domainOnly, false) == url)
            {
                while(columnIterator.hasNext())
                {
                    columnIterator.next();
                    if(columnIterator.key() != -1)
                    this->setItem(hashIterator.key(),columnIterator.key(),columnIterator.value());
                }
            }
        }
        else
        {
            if(hashIterator.value().at(this->columnIndexes.value("urlCol")).toString() == url)
            {
                while(columnIterator.hasNext())
                {
                    columnIterator.next();
                    if(columnIterator.key() != -1)
                    this->setItem(hashIterator.key(),columnIterator.key(),columnIterator.value());
                }
            }
        }
    }
}

void scrapTable::updateMultipleRows(QHash<QString, QHash<int, QVariant> > resultsStack, bool basedOnUrl, bool domainOnly)
{
    QMapIterator<int, QVariantList> hashIterator(scrapResults);
    Q_UNUSED(domainOnly);

    hashIterator.toFront();
    while(hashIterator.hasNext())
    {
        hashIterator.next();

        if(basedOnUrl == true)
        {
            if(resultsStack.contains(hashIterator.value().at(this->columnIndexes.value("urlCol")).toString()))
            {
                QHashIterator<int, QVariant> columnIterator(resultsStack.value(hashIterator.value().at(this->columnIndexes.value("urlCol")).toString()));

                columnIterator.toFront();
                while(columnIterator.hasNext())
                {
                    columnIterator.next();
                    if(columnIterator.key() != -1)
                    this->setItem(hashIterator.key(),columnIterator.key(),columnIterator.value());
                }
            }
        }
    }
}

void scrapTable::clearColumn()
{
    QMapIterator<int, QVariantList> hashIterator(scrapResults);

    hashIterator.toFront();
    while(hashIterator.hasNext())
    {
        hashIterator.next();
        this->setItem(hashIterator.key(),this->columnClicked,QVariant());
    }
    this->updateLayout();
}

void scrapTable::clearScrapResultsHash()
{
    emit beginResetModel();
    this->scrapResults.clear();
    emit endResetModel();
}

void scrapTable::reBuildScrapResults(QMap<int, QVariantList> newMapResults)
{
    emit beginResetModel();
    this->scrapResults.clear();
    this->scrapResults = newMapResults;
    this->scrollBarValueChanged(0);
    emit endResetModel();
}

// InsertRows cause Crash on Mac
void scrapTable::buildScrapResultsHash(QList<QVariant> list, int col, int startAt, QString mode)
{
    // If col is equal to -1 and mode == col we return
    if(col == -1 && mode == "col")
        return;

    if(!list.isEmpty())
    {
        int i,j,scrapResultIndex;
        QList<QVariant> stack;

        scraptableLocker.lock();
        if(startAt == -1)
            scrapResultIndex = this->scrapResults.size();
        else
            scrapResultIndex = startAt;

        i = j = 0;

        if(col != -1) // columns
        {
            for(i=0;i<list.size();i++,scrapResultIndex++)
            {
                stack.clear();
                for(j=0;j<colCount;j++)
                {
                    if(j == col)
                        stack.append(list.at(i));
                    else
                    {
                        if(this->scrapResults.contains(scrapResultIndex))
                        {
                            if(this->scrapResults.value(scrapResultIndex).size() >= j)
                                stack.append(scrapResults.value(scrapResultIndex).at(j));
                            else
                               stack.append("");
                        }
                        else
                        stack.append("");
                    }
                }
                this->scrapResults.insert(scrapResultIndex,stack);
            }
        }
        else // Rows
        {
            stack.clear();
            for(i=0;i<this->colCount;i++)
            {
                if(i<list.size())
                {
                    if(!list.at(i).toString().isEmpty())
                    {
                        if((i == this->columnIndexes.value("tfCol",-1) || i == this->columnIndexes.value("cfCol",-1) || i == this->columnIndexes.value("oblCol",-1) || i == this->columnIndexes.value("httpCol",-1) || i == this->columnIndexes.value("ttfvCol",-1)))
                            stack.append(list.at(i).toInt());
                        else if((i == this->columnIndexes.value("paCol",-1) || i == this->columnIndexes.value("daCol",-1)))
                            stack.append(static_cast<int>(qRound(list.at(i).toDouble())));
                        else if((i == this->columnIndexes.value("dfCol",-1) || i == this->columnIndexes.value("blCol",-1) || i == this->columnIndexes.value("mozRankCol",-1)))
                            stack.append(list.at(i).toDouble());
                        else
                            stack.append(list.at(i));
                    }
                    else
                      stack.append("");
                }
                else
                    stack.append("");
            }
            this->scrapResults.insert(this->scrapResults.size(),stack);
        }
        scraptableLocker.unlock();
    }
}

int scrapTable::getNbItemsByCol(int column, bool redir)
{
    if(scrapResults.isEmpty())
        return 0;

    if(column == -1)
        return 0;

    int items = 0;

    QMapIterator<int, QVariantList> hashIterator(scrapResults);
    hashIterator.toFront();
    while(hashIterator.hasNext())
    {
        hashIterator.next();
        if(redir)
        {
            if(hashIterator.value().at(column).toInt() > 300 && hashIterator.value().at(column).toInt() < 400)
                items++;
        }
        else
        {
            if(!hashIterator.value().at(column).toString().isEmpty())
                items++;
        }
    }

    return items;
}

QVariant scrapTable::getItemByIndex(QModelIndex modelIndex)
{
    if(!modelIndex.isValid())
        return QVariant();

    if(modelIndex.column() < 0 || modelIndex.column() > this->colCount)
        return QVariant();

    return scrapResults.value(modelIndex.row()).at(modelIndex.column());
}

QStringList scrapTable::getAllUrls()
{
    QStringList stack;
    int i;

    stack.clear();
    if(!scrapResults.size())
        return stack;

    for(i=0;i<scrapResults.size();i++)
        if(scrapResults.value(i).size() > 0)
            stack.append(scrapResults.value(i).at(this->columnIndexes.value("urlCol")).toString());

    return stack;
}

QVariantList scrapTable::getItemsByColumn(int col)
{
    QVariantList stack;
    int i;

    stack.clear();
    if(!scrapResults.size())
        return stack;

    if(col > this->colCount || col < 0)
        return stack;

    for(i=0;i<scrapResults.size();i++)
        if(scrapResults.value(i).size() > 0)
            stack.append(scrapResults.value(i).at(col));

    return stack;
}

int scrapTable::getScrapResultSize()
{
    return this->scrapResults.size();
}


void scrapTable::searchAndReplace(QString searchFor, QString replaceBy, bool regex, bool ignoreScheme)
{
    QString currentStrValue;
    QString substr;
    QString originalSubStr;
    QVariantList currentVariantList;
    QRegExp searchRegex(searchFor);
    QMutableMapIterator<int, QVariantList> mapIterator(scrapResults);
    emit disableCursor();
    mapIterator.toFront();
    while(mapIterator.hasNext())
    {
        mapIterator.next();
        currentVariantList = mapIterator.value();
        currentStrValue = currentVariantList.at(this->columnIndexes.value("urlCol")).toString();
        if(regex)
        {
            currentStrValue.replace(searchRegex,replaceBy);
        }
        else
        {
            if(ignoreScheme)
            {
                substr = currentStrValue.mid(currentStrValue.indexOf("://")+3);
                originalSubStr = substr;
                substr.replace(searchFor,replaceBy);
                currentStrValue.replace(originalSubStr,substr);
            }
            else
                currentStrValue.replace(searchFor,replaceBy);
        }
        currentVariantList.replace(this->columnIndexes.value("urlCol"),currentStrValue);
        mapIterator.setValue(currentVariantList);

    }
    emit this->updateLayout();
    emit enableCursor();
}

int scrapTable::removeBadUrls()
{
    int itemsRemoved, i;
    QMap<int, QVariantList> stackMap;
    itemsRemoved = i = 0;

    if(scrapResults.isEmpty())
        return 0;

    if(this->columnIndexes.value("httpCol") != -1)
    {
        stackMap.clear();
        QMapIterator<int, QVariantList> mapIterator(scrapResults);
        mapIterator.toFront();
        while(mapIterator.hasNext())
        {
            mapIterator.next();
            if((mapIterator.value().at(this->columnIndexes.value("httpCol")).toInt() >= 200 && mapIterator.value().at(this->columnIndexes.value("httpCol")).toInt() < 400) || mapIterator.value().at(this->columnIndexes.value("httpCol")).toString().isEmpty())
            {
                stackMap.insert(i, mapIterator.value());
                i++;
            }
            else
                itemsRemoved++;
        }

        emit beginResetModel();
        scrapResults.clear();
        scrapResults = stackMap;
        emit endResetModel();
    }
    return itemsRemoved;

}

int scrapTable::removeDuplicateUrl()
{
    int i, itemsRemoved, hashIndex;
    QHash<QString,int> dedupUrls;
    QMap<int, QVariantList> stackHash;
    scraptableLocker.lock();
    QMapIterator<int, QVariantList> hashIterator(scrapResults);
    scraptableLocker.unlock();
    QList<int> indexesToKeep;
    itemsRemoved = 0;

    if(scrapResults.isEmpty())
        return 0;

    if(this->columnIndexes.value("urlCol") == -1)
        return 0;

    dedupUrls.clear();
    indexesToKeep.clear();
    scraptableLocker.lock();
    hashIterator.toFront();
    while(hashIterator.hasNext())
    {
        hashIterator.next();
        if(hashIterator.value().size())
        dedupUrls.insert(hashIterator.value().at(this->columnIndexes.value("urlCol")).toString(),hashIterator.key());
    }
    scraptableLocker.unlock();


    indexesToKeep = dedupUrls.values();
    itemsRemoved = scrapResults.size() - indexesToKeep.size();

    if(!itemsRemoved)
        return 0;

    stackHash.clear();
    std::sort(indexesToKeep.begin(), indexesToKeep.end());

    scraptableLocker.lock();
    for(i=hashIndex=0;i<indexesToKeep.size();i++)
    {
        if(scrapResults.contains(indexesToKeep.at(i)))
        {
            stackHash.insert(hashIndex,scrapResults.value(indexesToKeep.at(i)));
            hashIndex++;
        }
    }
    scraptableLocker.unlock();

    beginResetModel();
    scrapResults.clear();
    scrapResults = stackHash;
    endResetModel();

    return itemsRemoved;
}

int scrapTable::removeDuplicateDomain(QString metricCol, QString apiUsed)
{
    QMap<QString, QList<int> > mapStack;
    QMap<int,QVariantList> stackHash;
    QMapIterator<int, QVariantList> hashIterator(scrapResults);
    QList<int> indexesToKeep;
    QString currentDomain, previousDomain;
    QUrl domain;
    bool firstLoop;
    bool dedupWithNoMetrics;
    int i,currentPr,highestPr,itemsRemoved, hashIndex;

    if(scrapResults.isEmpty())
        return 0;

    if(this->columnIndexes.value("urlCol") == -1)
        return 0;

    dedupWithNoMetrics = false;

    // We have to check if metricCol can be used with apiUsed
    if(metricCol == "PR" && (apiUsed == "Free" || apiUsed == "Majestic" || apiUsed == "Ahrefs" || apiUsed == "Moz"))
        metricCol = "prCol";
    else if(metricCol == "CF" && (apiUsed == "Majestic" || apiUsed == "Free"))
        metricCol = "cfCol";
    else if(metricCol == "TF" && (apiUsed == "Majestic" || apiUsed == "Free"))
        metricCol = "tfCol";
    else if(metricCol == "TTFV" && apiUsed == "Majestic")
        metricCol = "ttfvCol";
    else if(metricCol == "Domain Rank" && apiUsed == "Ahrefs")
        metricCol = "ahrefsDomainRankCol";
    else if(metricCol == "URL Rank" && apiUsed == "Ahrefs")
        metricCol = "ahrefsUrlRankCol";
    else if(metricCol == "Mozrank" && apiUsed == "Moz")
        metricCol = "mozRankCol";
    else if(metricCol == "PA" && apiUsed == "Moz")
        metricCol = "paCol";
    else if(metricCol == "DA" && apiUsed == "Moz")
        metricCol = "daCol";
    else
    {
        dedupWithNoMetrics = true;
    }


    hashIterator.toFront();
    while(hashIterator.hasNext())
    {
        hashIterator.next();
        if(dedupWithNoMetrics)
            mapStack.insert(hashIterator.value().at(this->columnIndexes.value("urlCol")).toString(), QList<int>() << 0 << hashIterator.key());
        else
            mapStack.insert(hashIterator.value().at(this->columnIndexes.value("urlCol")).toString(), QList<int>() << hashIterator.value().at(this->columnIndexes.value(metricCol)).toInt() << hashIterator.key());

    }


    highestPr = 0;
    firstLoop = true;
    QMapIterator<QString, QList<int> > mapIterator(mapStack);
    mapIterator.toFront();
    while(mapIterator.hasNext())
    {
        mapIterator.next();
        domain = QUrl(mapIterator.key());
        currentDomain  = domain.host();

        currentPr   = mapIterator.value().at(0);
        if(firstLoop == true || currentDomain != previousDomain)
        {
            indexesToKeep.append(mapIterator.value().at(1));
            previousDomain = currentDomain;
            highestPr = currentPr;
            firstLoop = false;
        }
        else if (currentDomain == previousDomain)
        {
            if(currentPr > highestPr)
            {
                indexesToKeep.removeLast();
                highestPr = currentPr;
                indexesToKeep.append(mapIterator.value().at(1));
            }
        }
    }

    itemsRemoved = scrapResults.size() - indexesToKeep.size();
    if(!itemsRemoved)
        return itemsRemoved;

    stackHash.clear();
    std::sort(indexesToKeep.begin(), indexesToKeep.end());

    scraptableLocker.lock();
    for(i=hashIndex=0;i<indexesToKeep.size();i++)
    {
        if(scrapResults.contains(indexesToKeep.at(i)))
        {
            stackHash.insert(hashIndex,scrapResults.value(indexesToKeep.at(i)));
            hashIndex++;
        }
    }
    scraptableLocker.unlock();


    beginResetModel();
    scrapResults.clear();
    scrapResults = stackHash;
    endResetModel();

    return itemsRemoved;
}

int scrapTable::removeSubdomains()
{
    QString url;
    int i,nbDot,itemsRemoved;
    QMap<int, QVariantList> stackHash;
    QMapIterator<int, QVariantList> hashIterator(scrapResults);

    if(scrapResults.isEmpty())
        return 0;

    if(this->columnIndexes.value("urlCol") == -1)
        return 0;

    i = 0;

    hashIterator.toFront();
    while(hashIterator.hasNext())
    {
        hashIterator.next();
        url = hashIterator.value().at(this->columnIndexes.value("urlCol")).toString();
        QUrl qurl(url);
        if(qurl.isValid())
        {
            url = qurl.host();
            url.remove(qurl.topLevelDomain());
            nbDot = url.count(".");

            if (nbDot < 1 || (nbDot == 1 && url.startsWith("www.")))
            {
                stackHash.insert(i,hashIterator.value());
                i++;
            }
        }
    }

    itemsRemoved = scrapResults.size() - stackHash.size();

    if(!itemsRemoved)
        return 0;

    beginResetModel();
    scrapResults.clear();
    scrapResults = stackHash;
    endResetModel();

    return itemsRemoved;
}

int scrapTable::removeSelectedRows(QModelIndexList indexList)
{
    QMutableListIterator<QModelIndex>  listIterator(indexList);
    int i;

    if(!indexList.size())
        return 0;

    listIterator.toFront();
    while(listIterator.hasNext())
    {
        listIterator.next();
        this->scrapResults.remove(listIterator.value().row());
    }
    // We re-index the QMap
    QMap<int, QVariantList> orderedMap;
    QMapIterator<int, QVariantList> mapIterator(this->scrapResults);

    orderedMap.clear();
    mapIterator.toFront();
    for(i=0;mapIterator.hasNext();i++)
    {
        mapIterator.next();
        orderedMap.insert(i, mapIterator.value());
    }

    beginResetModel();
    scrapResults.clear();
    scrapResults = orderedMap;
    endResetModel();


    return indexList.size() / this->colCount;
}

int scrapTable::removeColumnContaining(int column, QString search, bool containing)
{

    int i,itemsRemoved;
    QMapIterator<int, QVariantList> hashIterator(scrapResults);
    QMap<int, QVariantList> stackHash;

    if(scrapResults.isEmpty())
        return 0;

    if(column == -1)
        return 0;

    hashIterator.toFront();
    for(i=0;hashIterator.hasNext();)
    {
        hashIterator.next();
        if(containing == true)
        {
            if(!hashIterator.value().at(column).toString().contains(search))
            {
                stackHash.insert(i,hashIterator.value());
                i++;
            }
        }
        else if(containing == false)
        {
            if(hashIterator.value().at(column).toString().contains(search))
            {
                stackHash.insert(i,hashIterator.value());
                i++;
            }
        }
    }

    itemsRemoved = this->scrapResults.size() - stackHash.size();

    if(!itemsRemoved)
        return 0;


    beginResetModel();
    scrapResults.clear();
    scrapResults = stackHash;
    endResetModel();

    return itemsRemoved;
}

int scrapTable::removeColumnMatching(int column, QString userRegex, bool containing)
{

    int i,itemsRemoved;
    QMapIterator<int, QVariantList> hashIterator(scrapResults);
    QMap<int, QVariantList> stackHash;
    QRegExp customRegex(userRegex);

    if(scrapResults.isEmpty())
        return 0;

    if(column == -1)
        return 0;


    hashIterator.toFront();
    for(i=0;hashIterator.hasNext();)
    {
        hashIterator.next();
        if(containing == true)
        {
            if(customRegex.indexIn(hashIterator.value().at(column).toString()) == -1)
            {
                stackHash.insert(i,hashIterator.value());
                i++;
            }
        }
        else
        {
            if(customRegex.indexIn(hashIterator.value().at(column).toString()) != -1)
            {
                stackHash.insert(i,hashIterator.value());
                i++;
            }
        }
    }

    itemsRemoved = this->scrapResults.size() - stackHash.size();

    if(!itemsRemoved)
        return 0;

    beginResetModel();
    scrapResults.clear();
    scrapResults = stackHash;
    endResetModel();

    return itemsRemoved;

}

int scrapTable::removeRegexMask(QString userRegex)
{
    int itemsChanged = 0;
    QMutableMapIterator<int, QVariantList> mapIterator(scrapResults);
    QRegularExpression re(userRegex);

    if(scrapResults.isEmpty())
        return 0;

    if(this->columnIndexes.value("urlCol") == -1)
        return 0;


    if (!re.isValid()) {
        QString errorString = re.errorString();
        // Connect an appendlog here !!
        qDebug() << errorString;
    }
    else
    {
        mapIterator.toFront();
        while(mapIterator.hasNext())
        {
            mapIterator.next();
            QList<QVariant> hashValue;
            QString oldUrl(mapIterator.value().at(this->columnIndexes.value("urlCol")).toString());
            QRegularExpressionMatch match = re.match(oldUrl);

            if (match.hasMatch()) {
                hashValue = mapIterator.value();
                hashValue.replace(this->columnIndexes.value("urlCol"),oldUrl.remove(re));
                mapIterator.setValue(hashValue);
                itemsChanged++;
            }
        }
        endResetModel();
    }

    return itemsChanged;
}

int scrapTable::removeColumnByFilter(int column, double value, QString filter)
{
    int i,itemsRemoved;
    QMapIterator<int, QVariantList> hashIterator(scrapResults);
    QMap<int, QVariantList> stackHash;

    if(scrapResults.isEmpty())
        return 0;

    if(column < 0 || column > this->colCount)
        return 0;

    hashIterator.toFront();
    for(i=0;hashIterator.hasNext();)
    {
        hashIterator.next();

        // WIP: Issue 16
        // not sure about the removing of .toDouble()
        if(filter == "equal" && hashIterator.value().at(column) != value)
        {
            stackHash.insert(i,hashIterator.value());
            i++;
        }
        else if(filter == "notequal" && hashIterator.value().at(column) == value)
        {
            stackHash.insert(i,hashIterator.value());
            i++;
        }
        else if(filter == "lower" && hashIterator.value().at(column) >= value)
        {
            stackHash.insert(i,hashIterator.value());
            i++;
        }
        else if(filter == "higher" && hashIterator.value().at(column) <= value)
        {
            stackHash.insert(i,hashIterator.value());
            i++;
        }
    }

    itemsRemoved = this->scrapResults.size() - stackHash.size();

    if(!itemsRemoved)
        return 0;


    beginResetModel();
    scrapResults.clear();
    scrapResults = stackHash;
    endResetModel();

    return itemsRemoved;
}

void scrapTable::trimToRoot()
{
    QString host,scheme,rootDomain;
    QStringList cleanUrls;

    if(this->columnIndexes.value("urlCol") == -1)
        return;

    QMapIterator<int, QVariantList> hashIterator(scrapResults);
    hashIterator.toFront();
    beginResetModel();
    while(hashIterator.hasNext())
    {
        hashIterator.next();
        QUrl url(hashIterator.value().at(this->columnIndexes.value("urlCol")).toString().simplified());
        host = url.host();
        scheme = url.scheme();
        rootDomain = scheme+"://"+host;
        cleanUrls.append(rootDomain);
    }
    cleanUrls.removeDuplicates();
    this->scrapResults.clear();
    this->buildScrapResultsHash(Tools::stringListToVariantList(cleanUrls),this->columnIndexes.value("urlCol"));
    endResetModel();
}

void scrapTable::trimToLastFolder()
{
    QRegExp rxHost("([^/]+/?$)");
    QString url, lastFolderUrl, tmpUrl, capture;
    QStringList cleanUrls;


    if(this->columnIndexes.value("urlCol") == -1)
        return;

    QMapIterator<int, QVariantList> hashIterator(scrapResults);
    hashIterator.toFront();
    while(hashIterator.hasNext())
    {
        hashIterator.next();
        lastFolderUrl = url = tmpUrl = hashIterator.value().at(this->columnIndexes.value("urlCol")).toString().simplified();
        if (tmpUrl.endsWith("/"))
            tmpUrl.chop(1);
        // We check if we are not treating the root URL
        if(tmpUrl !=  Tools::getDomain(tmpUrl,false))
        {
            if(rxHost.indexIn(lastFolderUrl) > -1)
            {
                QUrl curUrl(lastFolderUrl);
                capture = rxHost.cap(1);
                if(capture.mid(0,capture.lastIndexOf("/")) != curUrl.host())
                {
                    lastFolderUrl.replace(QRegExp("[^/]+/?$"), "");
                    cleanUrls.append(lastFolderUrl);
                }
            }
        }
        else
            cleanUrls.append(url);
    }

    cleanUrls.removeDuplicates();
    this->scrapResults.clear();
    this->buildScrapResultsHash(Tools::stringListToVariantList(cleanUrls),this->columnIndexes.value("urlCol"));
    endResetModel();
}

void scrapTable::keepOnlyDomain()
{
    QString url;
    QStringList cleanUrls;

    if(this->columnIndexes.value("urlCol") == -1)
        return;

    QMapIterator<int, QVariantList> hashIterator(scrapResults);
    hashIterator.toFront();
    while(hashIterator.hasNext())
    {
        hashIterator.next();
        url = hashIterator.value().at(this->columnIndexes.value("urlCol")).toString().simplified();
        cleanUrls.append(Tools::getDomain(url));
    }

    cleanUrls.removeDuplicates();
    this->scrapResults.clear();
    this->buildScrapResultsHash(Tools::stringListToVariantList(cleanUrls),this->columnIndexes.value("urlCol"));
    endResetModel();
}


Qt::ItemFlags scrapTable::flags(const QModelIndex &index) const
{
    if (!index.isValid())
        return Qt::ItemIsEnabled;

    return QAbstractTableModel::flags(index) | Qt::ItemIsEditable;
}

void scrapTable::sort(int column, Qt::SortOrder order)
{
//    qDebug() << "Sorting column " << column << order;
    emit disableCursor();
    this->customTableSorting(column,order);
    this->scrollBarValueChanged(_currentScrollBarOffset);
    emit enableCursor();
    return;

//        emit disableCursor();
////        qDebug() << "Start sorting col["<< column << "] : " << QDateTime::currentDateTime();
//        this->quickSort(0, scrapResults.size() - 1, column, order);
////        qDebug() << "End sorting col : " << QDateTime::currentDateTime();
//    this->scrollBarValueChanged(_currentScrollBarOffset);
//        emit enableCursor();
}

void scrapTable::customTableSorting(int column, int sorting)
{
    // We put values into qMap Keys in order to auto-sort them
    // Qvariant in key (for sorting purpose) and original index into values ;)
    QMap<QVariant,int> sortedValues;
    QMap<int,QVariantList> sortedQMap;
    QMapIterator<int,QVariantList> scrapResultIterator(scrapResults);
    int newQmapIndex;

    beginResetModel();

    scrapResultIterator.toFront();
    while(scrapResultIterator.hasNext())
    {
        scrapResultIterator.next();
        sortedValues.insertMulti(scrapResultIterator.value().at(column), scrapResultIterator.key());
    }

    // Now we rebuild the scrapresults
    QMapIterator<QVariant,int> sortedMapIterator(sortedValues);
    if(sorting == 1) // Ascending
    {
        sortedMapIterator.toFront();
        for(newQmapIndex = 0;sortedMapIterator.hasNext();newQmapIndex++)
        {
            sortedMapIterator.next();
            sortedQMap.insert(newQmapIndex, scrapResults.value(sortedMapIterator.value()));
        }

    }
    else // descending
    {
        sortedMapIterator.toBack();
        for(newQmapIndex = 0;sortedMapIterator.hasPrevious();newQmapIndex++)
        {
            sortedMapIterator.previous();
            sortedQMap.insert(newQmapIndex, scrapResults.value(sortedMapIterator.value()));
        }
    }

    scrapResults.clear();
    scrapResults = sortedQMap;
    endResetModel();
}

void scrapTable::quickSort(int left, int right, int column, int sorting)
{
    if(right == -1)
        return;

    int i = left, j = right;
    QVariant pivot = scrapResults.value((left + right) / 2).at(column);

    while (i <= j)
    {
        if(sorting)
        {
            while (scrapResults.value(i).at(column) < pivot)
                i++;
            while (scrapResults.value(j).at(column) > pivot)
                j--;
        }
        else
        {
            while (scrapResults.value(i).at(column) > pivot)
                i++;
            while (scrapResults.value(j).at(column) < pivot)
                j--;
        }

        if (i <= j)
        {
            QVariantList temp = scrapResults.value(i);
            scrapResults.insert(i, scrapResults.value(j));
            scrapResults.insert(j,temp);
            i++;
            j--;
        }
    };
    if (left < j)
    {
        this->quickSort(left, j, column, sorting);
    }
    if (i < right)
    {
        this->quickSort(i, right, column, sorting);
    }
}

void scrapTable::scrollBarValueChanged(int value)
{
    _currentScrollBarOffset = value;
    _subScrapResults.clear();

    if(!this->scrapResults.size())
        return;

    int i,j;
    int endValue = qMin(value + visibleRows, this->scrapResults.size());
    emit layoutAboutToBeChanged();
    for(j=0,i = value; i<endValue;i++,j++)
    {
        _subScrapResults.insert(j,scrapResults.value(i));
    }
    emit layoutChanged();
}
