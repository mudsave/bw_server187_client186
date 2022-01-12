LIB =	entitydef

SRCS =								\
		data_description			\
		data_types					\
		entity_description			\
		entity_description_debug	\
		entity_description_map		\
		mailbox_base				\
		method_description			\
		method_response				\

ifndef MF_ROOT
export MF_ROOT := $(subst /src/lib/$(LIB),,$(CURDIR))
endif
ifdef DBMGR_USE
CPPFLAGS += -DMF_SERVER_DBMGR
endif

include $(MF_ROOT)/src/lib/makestuf/stdmake.mak
