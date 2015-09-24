TEMPLATE	= lib
CONFIG		+= qt warn_on release plugin
HEADERS		= rc2ui.h
SOURCES		= main.cpp rc2ui.cpp
DESTDIR		= ../../../../plugins/designer
os2:TARGET	= rcplug
else:TARGET	= rcplugin
INCLUDEPATH	+= ../../interfaces


target.path += $$plugins.path/designer
INSTALLS 	+= target
