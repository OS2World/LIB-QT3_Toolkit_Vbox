/****************************************************************************
** Form interface generated from reading ui file 'pixmapfunction.ui'
**
** Created: Sun Nov 2 20:32:32 2014
**      by: The User Interface Compiler ($Id: main.cpp 8 2005-11-16 19:36:46Z dmik $)
**
** WARNING! All changes made in this file will be lost!
****************************************************************************/

#ifndef PIXMAPFUNCTION_H
#define PIXMAPFUNCTION_H

#include <qvariant.h>
#include <qdialog.h>

class QVBoxLayout;
class QHBoxLayout;
class QGridLayout;
class QSpacerItem;
class QLabel;
class QFrame;
class QLineEdit;
class QPushButton;

class PixmapFunction : public QDialog
{
    Q_OBJECT

public:
    PixmapFunction( QWidget* parent = 0, const char* name = 0, bool modal = FALSE, WFlags fl = 0 );
    ~PixmapFunction();

    QLabel* TextLabel3;
    QFrame* Frame9;
    QLabel* labelFunction;
    QLineEdit* editArguments;
    QLabel* TextLabel5;
    QPushButton* helpButton;
    QPushButton* buttonOk;
    QPushButton* buttonCancel;

protected:
    QVBoxLayout* PixmapFunctionLayout;
    QSpacerItem* Spacer1;
    QHBoxLayout* Frame9Layout;
    QHBoxLayout* Layout3;
    QSpacerItem* Horizontal_Spacing2;

protected slots:
    virtual void languageChange();

};

#endif // PIXMAPFUNCTION_H
