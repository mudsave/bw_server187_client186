
ifndef MF_ROOT
export MF_ROOT := $(subst /bigworld/src/server/tools/redist,,$(shell pwd))
endif

all all_config clean realclean install::
	@cd MySQL-python-1.2.1_p2 && $(MAKE) $@

#end
