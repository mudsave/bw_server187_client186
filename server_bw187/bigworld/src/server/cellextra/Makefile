#MF_CONFIG = Release
SO  = cellextra
COMPONENT = cellapp
SRCS =							\
	./entity_extras/csolExtra			\
	./entity_extras/monster				\
	./entity_extras/gameobject			\
	./entity_extras/npcobject			\
	./entity_extras/AIInterface			\
	./utils/bigworld_module_extra			\
	./utils/py_array_proxy				\
	./controller/petRangeDetecter		\

ifndef MF_ROOT
export MF_ROOT := $(subst /bigworld/src/server/cellextra,,$(CURDIR))
endif

include $(MF_ROOT)/bigworld/src/server/common/common.mak


all::

