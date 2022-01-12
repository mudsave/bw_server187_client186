BIN  = cellapp
SRCS =										\
	ack_cell_app_death_helper				\
	buffered_ghost_message					\
	buffered_ghost_message_factory			\
	buffered_ghost_message_queue			\
	buffered_ghost_messages					\
	buffered_ghost_messages_for_entity		\
	cell									\
	cellapp									\
	cellappmgr								\
	cellapp_death_listener					\
	cell_app_channel						\
	cell_chunk								\
	cell_range_list							\
	cell_viewer_connection					\
	cell_viewer_server						\
	controller								\
	entity									\
	entity_cache							\
	entity_extra							\
	entity_navigate							\
	entity_population						\
	entity_type								\
	entity_vision							\
	external_interfaces						\
	face_entity_controller					\
	gateway									\
	gateway_controller						\
	history_event							\
	mailbox									\
	main									\
	move_controller							\
	navigation_controller					\
	passenger_controller					\
	passenger_extra							\
	passengers								\
	portal_config_controller				\
	profile									\
	proximity_controller					\
	py_client								\
	py_entities								\
	range_list_node							\
	real_entity								\
	scan_vision_controller					\
	space									\
	timer_controller						\
	turn_controller							\
	visibility_controller					\
	vision_controller						\
	witness									\
	../../common/shared_data				\
	../../common/doc_watcher				\
	../../common/chunk_portal				\
	../dbmgr/db_interface_utils				\

ASMS =

MY_LIBS = pyscript entitydef server waypoint chunk physics2 moo speedtree

BUILD_TIME_FILE = main
USE_PYTHON = 1
#HAS_PCH = 1

ifndef MF_ROOT
export MF_ROOT := $(subst /bigworld/src/server/$(BIN),,$(CURDIR))
endif

include $(MF_ROOT)/bigworld/src/server/common/common.mak

all::

ifeq ($(wildcard cell_range_list.cp*),cell_range_list.cpp)
# this file doesn't want to be optimised ... for some reason
$(MF_CONFIG)/cell_range_list.o: cell_range_list.cpp
ifdef QUIET_BUILD
	test -e $(MSG_FILE) && cat $(MSG_FILE); rm -f $(MSG_FILE)
	@echo $<
endif
	$(COMPILE.cc) -O0 $< $(OUTPUT_OPTION)
	@if test -e cell_range_list.d; then 						\
		echo -n $(MF_CONFIG)/ > $(MF_CONFIG)/cell_range_list.d;	\
		cat cell_range_list.d >> $(MF_CONFIG)/cell_range_list.d;\
		rm cell_range_list.d;									\
	fi
endif

