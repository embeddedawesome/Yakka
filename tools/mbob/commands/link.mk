ifndef BOB_OUTPUT_BINARY_DIRECTORY
BOB_OUTPUT_BINARY_DIRECTORY := $(BOB_OUTPUT_DIRECTORY)/binary
endif

# Linking depends on the compile command
link:: compile $(BOB_GENERATED_LINK_FILES)

# Specify that the compile_summary.mk is created by the compile command
$(BOB_OUTPUT_DIRECTORY)/compile_summary.mk:: compile

$(call INCLUDE_ADDITIONAL_MAKEFILE,$(BOB_OUTPUT_DIRECTORY)/compile_summary.mk)

# Include toolchain component makefiles
$(call INCLUDE_TOOLCHAIN_MAKEFILES,link)


# Include any additional component makefiles
$(foreach component,$(BOB_COMPONENT_NAMES),\
	$(foreach makefile,$($(component)_ADDITIONAL_MAKEFILES),\
		$(if $(wildcard $($(component)_DIRECTORY)$(makefile)),\
			$(call INCLUDE_ADDITIONAL_MAKEFILE,$($(component)_DIRECTORY)$(makefile)),\
			$(error Cannot find additional makefile: $($(component)_DIRECTORY)$(makefile) defined by $($(component)_MAKEFILE)))\
	)\
)

# Make the incoming project name a phony build target and have it rely on the binary output of the toolchain 
link:: $(BOB_GENERATED_LINK_FILES) $(TOOLCHAIN_BINARY_OUTPUT) $(if $(filter %.map,$(TOOLCHAIN_BINARY_OUTPUT)),$(BOB_OUTPUT_DIRECTORY)/mapfile.json) | $(BOB_OUTPUT_BINARY_DIRECTORY)/.d
	$(QUIET)$(ECHO) Build complete
	
$(BOB_OUTPUT_DIRECTORY)/mapfile.json: $(filter %.map,$(TOOLCHAIN_BINARY_OUTPUT)) | $(BOB_OUTPUT_DIRECTORY)/.d
	$(ECHO) Creating $@
	$(PERL) $(BOB_BUILD_SYSTEM_DIRECTORY)/analysis/mapfile_to_JSON.pl $^ > $@

