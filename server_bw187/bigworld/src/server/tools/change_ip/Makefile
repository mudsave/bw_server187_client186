ifndef MF_CONFIG
	MF_CONFIG=Hybrid
endif

BIN = change_ip
SRCS = main

ifndef MF_ROOT
export MF_ROOT := $(subst /bigworld/src/server/tools/$(BIN),,$(CURDIR))
endif

# INSTALL_DIR = $(MF_ROOT)/bigworld/tools/server

MF_LIBS =

include $(MF_ROOT)/bigworld/src/server/common/common.mak
