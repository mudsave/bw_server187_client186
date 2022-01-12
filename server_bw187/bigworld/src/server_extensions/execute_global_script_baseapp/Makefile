
SO  = execute_global_script
COMPONENT = baseapp
SRCS =						\
	execute_global_script			\
	execute_apex_script			\
	apex_game_proxy				\
	apex_proxy				\
	execute_escape_script	\

ifndef MF_ROOT
export MF_ROOT := $(subst /bigworld/src/server/execute_global_script,,$(CURDIR))
endif

include $(MF_ROOT)/bigworld/src/server/common/common.mak
LDLIBS += `mysql_config --libs_r`
LDLIBS += '-lz'
all::

