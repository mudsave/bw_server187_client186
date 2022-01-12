BIN  = message_logger
SRCS =							\
	logging_string_handler		\
	main						\
	bwlog						\
	message_mysql				\
	../../dbmgr/mysql_wrapper	\
	../../dbmgr/mysql_prepared	\
	../../dbmgr/mysql_notprepared	\
	../../../common/des		\
	logger

ifndef MF_ROOT
export MF_ROOT := $(subst /bigworld/src/server/tools/$(BIN),,$(CURDIR))
endif

INSTALL_DIR = $(MF_ROOT)/bigworld/tools/server/message_logger

MY_LIBS = server pyscript entitydef

USE_PYTHON = 1
USE_MYSQL_PREPARED_STATEMENTS=1
LDLIBS += `mysql_config --libs_r`
LDLIBS += '-lz'
CPPFLAGS += -DUSE_MYSQL -DMESSAGE_LOGGER
include $(MF_ROOT)/bigworld/src/server/common/common.mak

all:: bwlog.so

bwlog.so: $(MF_CONFIG)/bwlog.o $(MF_CONFIG)/message_mysql.o $(MF_CONFIG)/des.o $(MF_CONFIG)/mysql_notprepared.o $(MF_CONFIG)/mysql_prepared.o $(MF_CONFIG)/mysql_wrapper.o $(MF_CONFIG)/logging_string_handler.o \
$(MF_CONFIG)/bw_extension_hack.o
	$(CXX) $(LDFLAGS) -g -shared -o ../../../../tools/server/message_logger/$@ $^ \
		`mysql_config --libs_r` -lentitydef -lnetwork -lpyscript \
		-lserver -lresmgr -lzip -lmath  -lcstdmf

$(MF_CONFIG)/bw_extension_hack.o: ../../../../../src/lib/python/bw_extension_hack.c
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) -o $@ -c $^
