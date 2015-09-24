/****************************************************************************
** Form interface generated from reading ui file 'topicchooser.ui'
**
** Created: Sun Nov 2 20:39:06 2014
**      by: The User Interface Compiler ($Id: main.cpp 8 2005-11-16 19:36:46Z dmik $)
**
** WARNING! All changes made in this file will be lost!
****************************************************************************/

#ifndef TOPICCHOOSERBASE_H
#define TOPICCHOOSERBASE_H

#include <qvariant.h>
#include <qdialog.h>

class QVBoxLayout;
class QHBoxLayout;
class QGridLayout;
class QSpacerItem;
class QLabel;
class QListBox;
class QListBoxItem;
class QPushButton;

class TopicChooserBase : public QDialog
{
    Q_OBJECT

public:
    TopicChooserBase( QWidget* parent = 0, const char* name = 0, bool modal = FALSE, WFlags fl = 0 );
    ~TopicChooserBase();

    QLabel* label;
    QListBox* listbox;
    QPushButton* buttonDisplay;
    QPushButton* buttonCancel;

protected:
    QVBoxLayout* TopicChooserBaseLayout;
    QHBoxLayout* Layout16;
    QSpacerItem* Horizontal_Spacing2;

protected slots:
    virtual void languageChange();

    virtual void init();
    virtual void destroy();


};

#endif // TOPICCHOOSERBASE_H
