ifneq ($(wildcard cellappmgr/Makefile),cellappmgr/Makefile)
	LICENSE_SERVER=1
endif

ifndef MF_ROOT
export MF_ROOT := $(subst /bigworld/src/server,,$(CURDIR))
endif

all clean realclean all_config install::
ifneq ($(CURDIR),$(MF_ROOT)/bigworld/src/server)
ifneq ($(CURDIR),$(MF_ROOT)bigworld/src/server)
	@echo 'Error: MF_ROOT=$(MF_ROOT)'
	@echo '       is not the root of $(CURDIR)'
	@echo 'Remove the MF_ROOT environment variable.'
	@false
endif
endif

	@cd ../../../src/lib/chunk && $(MAKE) $@
	@cd ../../../src/lib/cstdmf && $(MAKE) $@
	@cd ../../../src/lib/entitydef && $(MAKE) $@
	@cd ../../../src/lib/math && $(MAKE) $@
	@cd ../../../src/lib/moo && $(MAKE) $@
	@cd ../../../src/lib/network && $(MAKE) $@
	@cd ../../../src/lib/physics2 && $(MAKE) $@
	@cd ../../../src/lib/pyscript && $(MAKE) $@
	@cd ../../../src/lib/resmgr && $(MAKE) $@
	@cd ../../../src/lib/server && $(MAKE) $@
	@cd ../../../src/lib/speedtree && $(MAKE) $@
	@cd ../../../src/lib/waypoint && $(MAKE) $@
	@cd ../../../src/lib/zip && $(MAKE) $@
	@cd baseapp && $(MAKE) $@
	@cd baseappmgr && $(MAKE) $@
	@cd cellapp && $(MAKE) $@
ifndef LICENSE_SERVER
	@cd cellappmgr && $(MAKE) $@
endif
	@cd dbmgr && $(MAKE) $@
	@cd loginapp && $(MAKE) $@
	@cd reviver && $(MAKE) $@
	@cd tools && $(MAKE) $@
ifdef MF_CONFIG
	@echo completed $@ \(MF_CONFIG = $(MF_CONFIG)\)
else
	@echo completed $@
endif
	$(MAKE) done

clean realclean::
# Explicitly clean out Python library
	@cd ../../../src/lib/python && $(MAKE) $@

done:
ifdef DO_NOT_BELL
else
	echo -n 
endif

server:
	$(MAKE) all

#end
