# -*- makefile -*-
#
# :mode=makefile:tabSize=4:indentSize=4:noTabs=false:
# :folding=explicit:collapseFolds=1:
#
# Main Makefile for building the Qt library, examples and tutorial.
#
# Version for OS/2, Innotek GCC and GNU Make
#

FORCEDEP=
CONTINUEONERROR=-i

HAVE_4OS2 := $(shell echo %@eval[2 + 2])
ifneq (4,$(HAVE_4OS2))
HAVE_4OS2 :=
endif

ECHO_EMPTY := $(if $(HAVE_4OS2),@echo .,@echo.)

.PHONY: private_headers

all: symlinks src-qmake src-moc sub-src sub-tutorial sub-tools sub-examples
	$(ECHO_EMPTY)
	@echo The Qt library is now built in lib
	@echo The Qt examples are built in the directories in examples
	@echo The Qt tutorials are built in the directories in tutorial
	$(ECHO_EMPTY)
	@echo Note: be sure to set the environment variable QTDIR to point
	@echo       to here or to wherever you move these directories.
	$(ECHO_EMPTY)
	@echo Enjoy!   - the Trolltech team
	$(ECHO_EMPTY)

symlinks: .qmake.cache private_headers
#	syncqt

src-qmake: symlinks qmake\Makefile
	$(MAKE) -C qmake

src-moc: src-qmake src\moc\Makefile $(FORCEDEP)
	$(MAKE) -C src\moc

src\moc\Makefile:
	cd src\moc & qmake 	
	
sub-src: src-qmake src-moc .qmake.cache src\Makefile $(FORCEDEP)
	$(MAKE) -C src

src\Makefile:
	cd src & qmake 	
	
sub-tutorial: sub-src tutorial\Makefile $(FORCEDEP)
	$(MAKE) -C tutorial $(CONTINUEONERROR)

tutorial\Makefile:
	cd tutorial & qmake 	
	
sub-examples: sub-src sub-tools examples\Makefile $(FORCEDEP)
	$(MAKE) -C examples $(CONTINUEONERROR)

examples\Makefile:
	cd examples & qmake 	
	
## @todo (dmik): later
#sub-extensions: sub-src $(FORCEDEP)
#	$(MAKE) -C extensions $(CONTINUEONERROR)
#
#sub-plugins: sub-src .qmake.cache $(FORCEDEP)
#	$(MAKE) -C plugins\src
#
#sub-tools: sub-plugins $(FORCEDEP)
#	$(MAKE) -C tools
sub-tools: tools\Makefile $(FORCEDEP)
	$(MAKE) -C tools

tools\Makefile:
	cd tools & qmake 	
	
clean:
	@if exist qmake\Makefile $(MAKE) -C qmake $(CONTINUEONERROR) clean
	@if exist src\moc\Makefile $(MAKE) -C src\moc $(CONTINUEONERROR) clean
	@if exist src\Makefile $(MAKE) -C src $(CONTINUEONERROR) clean
	@if exist tutorial\Makefile $(MAKE) -C tutorial $(CONTINUEONERROR) clean
	@if exist examples\Makefile $(MAKE) -C examples $(CONTINUEONERROR) clean
## @todo (dmik): later
#	@if exist extensions\Makefile $(MAKE) -C extensions $(CONTINUEONERROR) clean
#	@if exist plugins\src\Makefile $(MAKE) -C plugins\src $(CONTINUEONERROR) clean
	@if exist tools\Makefile $(MAKE) -C tools $(CONTINUEONERROR) clean

distclean: clean
	-del .qmake.cache

.qmake.cache:
	$(ECHO_EMPTY)
	@echo   Qt must first be configured using the "configure" script.
	$(ECHO_EMPTY)

FORCE:

# defines and rules to duplicate the private include headers

INCLUDE_PRIVATE 	= include\private
KERNEL_PH	= $(patsubst src/kernel/%,$(INCLUDE_PRIVATE)\\%,$(wildcard src/kernel/*_p.h))
WIDGETS_PH	= $(patsubst src/widgets/%,$(INCLUDE_PRIVATE)\\%,$(wildcard src/widgets/*_p.h))
DIALOGS_PH	= $(patsubst src/dialogs/%,$(INCLUDE_PRIVATE)\\%,$(wildcard src/dialogs/*_p.h))
TOOLS_PH 	= $(patsubst src/tools/%,$(INCLUDE_PRIVATE)\\%,$(wildcard src/tools/*_p.h))
CODECS_PH	= $(patsubst src/codecs/%,$(INCLUDE_PRIVATE)\\%,$(wildcard src/codecs/*_p.h))
STYLES_PH	= $(patsubst src/styles/%,$(INCLUDE_PRIVATE)\\%,$(wildcard src/styles/*_p.h))
NETWORK_PH	= $(patsubst src/network/%,$(INCLUDE_PRIVATE)\\%,$(wildcard src/network/*_p.h))
XML_PH		= $(patsubst src/xml/%,$(INCLUDE_PRIVATE)\\%,$(wildcard src/xml/*_p.h))
SQL_PH		= $(patsubst src/sql/%,$(INCLUDE_PRIVATE)\\%,$(wildcard src/sql/*_p.h))
PRIVATE_HEADERS 	= $(KERNEL_PH) $(WIDGETS_PH) $(DIALOGS_PH) $(TOOLS_PH) $(CODECS_PH) $(STYLES_PH) $(NETWORK_PH) $(XML_PH) $(SQL_PH)

COPY_HEADER = @(if not exist $(INCLUDE_PRIVATE) mkdir $(INCLUDE_PRIVATE)) & \
	(if not exist $@ echo updated > .private_headers) & (echo copy $< $@) & (copy $< $@ >nul 2>&1)

$(KERNEL_PH): $(INCLUDE_PRIVATE)\\%.h: src\kernel\\%.h
	$(COPY_HEADER)
$(WIDGETS_PH): $(INCLUDE_PRIVATE)\\%.h: src\widgets\\%.h
	$(COPY_HEADER)
$(DIALOGS_PH): $(INCLUDE_PRIVATE)\\%.h: src\dialogs\\%.h
	$(COPY_HEADER)
$(TOOLS_PH): $(INCLUDE_PRIVATE)\\%.h: src\tools\\%.h
	$(COPY_HEADER)
$(CODECS_PH): $(INCLUDE_PRIVATE)\\%.h: src\codecs\\%.h
	$(COPY_HEADER)
$(STYLES_PH): $(INCLUDE_PRIVATE)\\%.h: src\styles\\%.h
	$(COPY_HEADER)
$(NETWORK_PH): $(INCLUDE_PRIVATE)\\%.h: src\network\\%.h
	$(COPY_HEADER)
$(XML_PH): $(INCLUDE_PRIVATE)\\%.h: src\xml\\%.h
	$(COPY_HEADER)
$(SQL_PH): $(INCLUDE_PRIVATE)\\%.h: src\sql\\%.h
	$(COPY_HEADER)

private_headers: $(PRIVATE_HEADERS)
	@if not exist .private_headers echo updated > .private_headers
