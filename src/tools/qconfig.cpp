#include <qglobal.h>

/* Install paths from configure */
static const char QT_INSTALL_PREFIX [267] = "qt_nstpath=C:\\200\\trunk";
static const char QT_INSTALL_BINS   [267] = "qt_binpath=C:\\200\\trunk\\bin";
static const char QT_INSTALL_DOCS   [267] = "qt_docpath=C:\\200\\trunk\\doc";
static const char QT_INSTALL_HEADERS[267] = "qt_hdrpath=C:\\200\\trunk\\include";
static const char QT_INSTALL_LIBS   [267] = "qt_libpath=C:\\200\\trunk\\lib";
static const char QT_INSTALL_PLUGINS[267] = "qt_plgpath=C:\\200\\trunk\\plugins";
static const char QT_INSTALL_DATA   [267] = "qt_datpath=C:\\200\\trunk";
static const char QT_INSTALL_TRANSLATIONS [267] = "qt_trnpath=C:\\200\\trunk\\translations";

/* strlen( "qt_xxxpath=" ) == 11 */
const char *qInstallPath()        { return QT_INSTALL_PREFIX  + 11; }
const char *qInstallPathDocs()    { return QT_INSTALL_DOCS    + 11; }
const char *qInstallPathHeaders() { return QT_INSTALL_HEADERS + 11; }
const char *qInstallPathLibs()    { return QT_INSTALL_LIBS    + 11; }
const char *qInstallPathBins()    { return QT_INSTALL_BINS    + 11; }
const char *qInstallPathPlugins() { return QT_INSTALL_PLUGINS + 11; }
const char *qInstallPathData()    { return QT_INSTALL_DATA    + 11; }
const char *qInstallPathTranslations() { return QT_INSTALL_TRANSLATIONS + 11; }
const char *qInstallPathSysconf() { return 0; }
