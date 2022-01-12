COMMON=../../../common

BIN  = desTool
SRCS = main			\
	$(COMMON)/des

ifndef MF_ROOT
export MF_ROOT := $(subst /bigworld/src/server/tools/$(BIN),,$(CURDIR))
endif

INSTALL_DIR = $(CURDIR)

MY_LIBS = 

include $(MF_ROOT)/bigworld/src/server/common/common.mak