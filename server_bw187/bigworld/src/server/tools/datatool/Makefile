
ifndef MF_ROOT
export MF_ROOT := $(subst /bigworld/src/server/tools,,$(CURDIR))
endif

all all_config clean realclean install::
	$(MAKE) -f Makefile_math $@
	$(MAKE) -f Makefile_pyscript $@
	$(MAKE) -f Makefile_server $@
	$(MAKE) -f Makefile_cstdmf $@
	$(MAKE) -f Makefile_entitydef $@
	$(MAKE) -f Makefile_network $@
	$(MAKE) -f Makefile_resmgr $@
	$(MAKE) -f Makefile_entitydata $@

