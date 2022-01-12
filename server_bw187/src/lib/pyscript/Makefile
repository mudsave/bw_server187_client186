LIB =	pyscript

SRCS =					\
	pickler				\
	py_data_section		\
	pyobject_plus		\
	py_output_writer	\
	py_patrolpath		\
	py_to_stl			\
	pywatcher			\
	res_mgr_script		\
	resource_table		\
	script				\
	script_math			\
	stl_to_py			\

ifndef MF_ROOT
export MF_ROOT := $(subst /src/lib/$(LIB),,$(CURDIR))
endif

include $(MF_ROOT)/src/lib/makestuf/stdmake.mak
