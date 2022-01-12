
ifndef MF_ROOT
export MF_ROOT := $(subst /bigworld/src/server/tools,,$(CURDIR))
endif

all all_config clean realclean install::
	@cd bwmachined && $(MAKE) $@
	@cd bots && $(MAKE) $@
	@cd eload && $(MAKE) $@
	@cd message_logger && $(MAKE) $@
	@cd runscript && $(MAKE) $@
	@cd watcher && $(MAKE) $@
	@cd mls && $(MAKE) $@
	@cd redist && $(MAKE) $@

#   Don't build updater stuff for now
#	@cd launchupdate && $(MAKE) $@
#	@cd calcmd5 && $(MAKE) $@
#	@cd bdiff && $(MAKE) $@
#	@cd bundiff && $(MAKE) $@

#end
