/****************************************************************************
** Form interface generated from reading ui file 'projectsettings.ui'
**
** Created: Sun Nov 2 20:32:33 2014
**      by: The User Interface Compiler ($Id: main.cpp 8 2005-11-16 19:36:46Z dmik $)
**
** WARNING! All changes made in this file will be lost!
****************************************************************************/

#ifndef PROJECTSETTINGSBASE_H
#define PROJECTSETTINGSBASE_H

#include <qvariant.h>
#include <qdialog.h>

class QVBoxLayout;
class QHBoxLayout;
class QGridLayout;
class QSpacerItem;
class QTabWidget;
class QWidget;
class QLabel;
class QToolButton;
class QLineEdit;
class QComboBox;
class QPushButton;

class ProjectSettingsBase : public QDialog
{
    Q_OBJECT

public:
    ProjectSettingsBase( QWidget* parent = 0, const char* name = 0, bool modal = FALSE, WFlags fl = 0 );
    ~ProjectSettingsBase();

    QTabWidget* tabWidget;
    QWidget* tabSettings;
    QLabel* TextLabel1_2;
    QLabel* TextLabel1_3;
    QToolButton* buttonDatabaseFile_2;
    QLineEdit* editDatabaseFile;
    QLabel* TextLabel1_2_2_2;
    QComboBox* comboLanguage;
    QLineEdit* editProjectFile;
    QToolButton* buttonProject;
    QPushButton* buttonHelp;
    QPushButton* buttonOk;
    QPushButton* buttonCancel;

protected:
    QVBoxLayout* ProjectSettingsBaseLayout;
    QGridLayout* tabSettingsLayout;
    QSpacerItem* Spacer3;
    QHBoxLayout* Layout1;
    QHBoxLayout* Layout4;
    QSpacerItem* Horizontal_Spacing2;

protected slots:
    virtual void languageChange();

    virtual void chooseDatabaseFile();
    virtual void chooseProjectFile();
    virtual void destroy();
    virtual void helpClicked();
    virtual void init();
    virtual void languageChanged( const QString & );
    virtual void okClicked();


};

#endif // PROJECTSETTINGSBASE_H
