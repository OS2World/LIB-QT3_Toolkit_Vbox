/****************************************************************************
** Form interface generated from reading ui file 'finddialog.ui'
**
** Created: Sun Nov 2 20:39:06 2014
**      by: The User Interface Compiler ($Id: main.cpp 8 2005-11-16 19:36:46Z dmik $)
**
** WARNING! All changes made in this file will be lost!
****************************************************************************/

#ifndef FINDDIALOG_H
#define FINDDIALOG_H

#include <qvariant.h>
#include <qdialog.h>

class QVBoxLayout;
class QHBoxLayout;
class QGridLayout;
class QSpacerItem;
class QButtonGroup;
class QRadioButton;
class QLabel;
class QComboBox;
class QPushButton;
class QCheckBox;
class QTextBrowser;
class QStatusBar;
class MainWindow;

class FindDialog : public QDialog
{
    Q_OBJECT

public:
    FindDialog( QWidget* parent = 0, const char* name = 0, bool modal = FALSE, WFlags fl = 0 );
    ~FindDialog();

    QButtonGroup* ButtonGroup2;
    QRadioButton* radioForward;
    QRadioButton* radioBackward;
    QLabel* TextLabel1;
    QComboBox* comboFind;
    QPushButton* PushButton2;
    QButtonGroup* ButtonGroup1;
    QCheckBox* checkWords;
    QCheckBox* checkCase;
    QPushButton* PushButton1;

    MainWindow * mainWindow();
    bool hasFindExpression();

public slots:
    virtual void doFind();
    virtual void doFind(bool);
    virtual void statusMessage(const QString &message);

protected:
    QStatusBar *sb;
    bool onceFound;
    QString findExpr;
    QTextBrowser *lastBrowser;

    QVBoxLayout* FindDialogLayout;
    QGridLayout* Layout5;
    QSpacerItem* Spacer2;
    QVBoxLayout* ButtonGroup2Layout;
    QHBoxLayout* Layout1;
    QVBoxLayout* ButtonGroup1Layout;

protected slots:
    virtual void languageChange();

    virtual void init();
    virtual void destroy();


};

#endif // FINDDIALOG_H
