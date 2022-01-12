# Need to define the following variables
#
# BIN = Name of the executables to make
#		(e.g. BIN = cellmgr)
# SRCS = Name of the source files WITHOUT the extension
#		(e.g. cellmgr main)

BIN  = loginapp
SRCS = main loginapp 		\
	../dbmgr/db_interface	\
	../dbmgr/worker_thread	\
	../../common/doc_watcher	\
	../../common/login_interface \
	./webserver/soapC \
	./webserver/soapAutoActivateSoap12Proxy\
	./webserver/soapGameAccountBindingProxy
MY_LIBS = server
BUILD_TIME_FILE = main

USE_OPENSSL = 1

# USE_PYTHON = 1
CPPFLAGS += -I $(MF_ROOT)/src/lib/third_party/gsoap-2.7/gsoap
CPPFLAGS += -I $(MF_ROOT)/src/lib/third_party/gsoap-2.7/gsoap/pluginp
LDLIBS += $(MF_ROOT)/src/lib/third_party/gsoap-2.7/gsoap/libgsoap++.a

ifndef MF_ROOT
export MF_ROOT := $(subst /bigworld/src/server/$(BIN),,$(CURDIR))
endif

include $(MF_ROOT)/bigworld/src/server/common/common.mak
