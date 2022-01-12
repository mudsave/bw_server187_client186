LIB = server

SRCS =									\
	add_to_manager_helper				\
	anonymous_channel_client			\
	auto_backup_and_archive				\
	backup_hash							\
	bwconfig							\
	cvs									\
	deem								\
	id_client							\
	plugin_library						\
	python_server						\
	reviver_subject						\
	time_keeper							\
	bwservice

ifndef MF_ROOT
export MF_ROOT := $(subst /src/lib/$(LIB),,$(CURDIR))
endif

include $(MF_ROOT)/src/lib/makestuf/stdmake.mak
