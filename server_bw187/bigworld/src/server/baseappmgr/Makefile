#
# Makefile for baseappmgr
#

BIN = baseappmgr
SRCS = main							\
	baseapp							\
	baseappmgr \
	../baseapp/baseapp_int_interface \
	../loginapp/login_int_interface \
	../dbmgr/db_interface \
	../updater/updater_interface \
	../../common/doc_watcher

MY_LIBS = server
BUILD_TIME_FILE = main

ifndef MF_ROOT
export MF_ROOT := $(subst /bigworld/src/server/$(BIN),,$(CURDIR))
endif

include $(MF_ROOT)/bigworld/src/server/common/common.mak


