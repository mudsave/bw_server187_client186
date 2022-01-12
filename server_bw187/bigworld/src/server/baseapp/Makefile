#
# Makefile for baseapp
#

BIN  = baseapp
SRCS = main								\
	app_timers							\
	base								\
	baseapp								\
	bwtracer							\
	entity_type							\
	external_interfaces					\
	global_bases						\
	loading_thread						\
	mailbox								\
	proxy								\
	py_bases							\
	py_cell_data						\
	rate_limit_message_filter			\
	timers								\
	worker_thread						\
	../../common/shared_data			\
	../../common/doc_watcher			\
	../dbmgr/db_interface				\
	../dbmgr/db_interface_utils			\
	../updater/updater_interface			\
	../../../../src/lib/chunk/station_graph		\
	../../../../src/lib/chunk/unique_id

MY_LIBS = server entitydef pyscript

USE_PYTHON = 1
USE_OPENSSL = 1

BUILD_TIME_FILE = main

ifndef MF_ROOT
export MF_ROOT := $(subst /bigworld/src/server/$(BIN),,$(CURDIR))
endif

include $(MF_ROOT)/bigworld/src/server/common/common.mak
