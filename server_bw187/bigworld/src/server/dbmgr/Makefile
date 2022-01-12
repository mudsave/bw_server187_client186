# MF_CONFIG=Debug
# CPPFLAGS += -DDBMGR_SELFTEST
# CPPFLAGS += -DDBMGR_REMOVE_OLD_TABLES_ANYWAY
# CPPFLAGS += -DWORKERTHREAD_SELFTEST

USE_MYSQL=1
USE_XML=0
BUILD_TIME_FILE = main

BIN = dbmgr
SRCS =	main database custom				\
		entity_recoverer worker_thread		\
		../baseappmgr/baseappmgr_interface	\
		../baseapp/baseapp_int_interface	\
		../updater/updater_interface		\
		../../common/doc_watcher		\
		../../common/login_interface		\
		../../common/des			\
		../../../../src/lib/chunk/station_graph \
		../../../../src/lib/chunk/unique_id

ifeq ($(USE_MYSQL), 1)
SRCS += mysql_wrapper mysql_prepared mysql_notprepared mysql_database \
	mysql_typemapping db_interface_utils
endif

ifeq ($(USE_XML), 1)
SRCS += xml_database
endif

MY_LIBS = server entitydef pyscript

USE_PYTHON = 1

USE_OPENSSL = 1

ifndef MF_ROOT
export MF_ROOT := $(subst /bigworld/src/server/$(BIN),,$(CURDIR))
endif

include $(MF_ROOT)/bigworld/src/server/common/common.mak

ifeq ($(USE_MYSQL), 1)
USE_MYSQL_PREPARED_STATEMENTS=1
LDLIBS += `mysql_config --libs_r`
LDLIBS += '-lz'
CPPFLAGS += -DUSE_MYSQL
CPPFLAGS += -DMF_SERVER_DBMGR
ifeq ($(USE_MYSQL_PREPARED_STATEMENTS), 1)
	CPPFLAGS += -DUSE_MYSQL_PREPARED_STATEMENTS
endif
CPPFLAGS += `mysql_config --cflags`
endif

ifeq ($(USE_XML), 1)
CPPFLAGS += -DUSE_XML
endif
