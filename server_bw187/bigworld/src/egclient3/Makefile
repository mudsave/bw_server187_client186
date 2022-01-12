
BIN  = egclient3
SRCS =										\
	../egclient2/main						\
	../egclient2/entity						\
	../egclient2/entity_type				\
	main_loop_action						\
	../common/servconn						\
	../common/login_interface

ifndef MF_ROOT
export MF_ROOT := $(subst /bigworld/src/$(BIN),,$(CURDIR))
endif

INSTALL_DIR = $(MF_ROOT)/bigworld/src/egclient3

ASMS =

MY_LIBS = entitydef pyscript

USE_PYTHON = 1

USE_OPENSSL = 1

include $(MF_ROOT)/bigworld/src/server/common/common.mak

all::

