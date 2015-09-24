TEMPLATE   = app
CONFIG	  -= qt
TARGET	   = qm
TARGET_EXT = .target

include(translations.pri)

qm_target.target = make_qm_files
qm_target.commands = $(QTDIR)\bin\lrelease translations.pro

QMAKE_LINK = @echo > nul
QMAKE_EXTRA_TARGETS += qm_target
PRE_TARGETDEPS = $$qm_target.target

