/*
$Id: structureparser.h 2 2005-11-16 15:49:26Z dmik $
*/

#ifndef STRUCTUREPARSER_H
#define STRUCTUREPARSER_H

#include <qxml.h>
#include <qptrstack.h>

class QListView;
class QListViewItem;
class QString;

class StructureParser: public QXmlDefaultHandler
{
public:
    StructureParser( QListView * );
    bool startElement( const QString&, const QString&, const QString& ,
                       const QXmlAttributes& );
    bool endElement( const QString&, const QString&, const QString& );

    void setListView( QListView * );

private:
    QPtrStack<QListViewItem> stack;
    QListView * table;
};

#endif
