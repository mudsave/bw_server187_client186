LIB = physics2

SRCS =			\
	bsp			\
	hulltree	\
	quad_tree	\
	worldpoly	\
	worldtri	\

ifndef MF_ROOT
export MF_ROOT := $(subst /src/lib/$(LIB),,$(CURDIR))
endif

include $(MF_ROOT)/src/lib/makestuf/stdmake.mak
