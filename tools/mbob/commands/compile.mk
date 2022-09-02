.SECONDARY:

PROJECT_LIBRARIES :=
EXTENSIONS := c cpp s S nim

compile:: $(BOB_PROJECT_SUMMARY_FILE) $(BOB_OUTPUT_DIRECTORY)/compile_summary.mk $(BOB_PROJECT).library_files

# Only use one shell
.ONESHELL:

include $(BOB_PROJECT_SUMMARY_FILE)

# Include toolchain component makefiles
$(call INCLUDE_TOOLCHAIN_MAKEFILES,compile)


# Include any additional component makefiles
$(foreach component,$(BOB_COMPONENT_NAMES),\
	$(foreach makefile,$($(component)_ADDITIONAL_MAKEFILES),\
		$(call INCLUDE_ADDITIONAL_MAKEFILE,$($(component)_DIRECTORY)$(makefile))\
	)\
)

# Define the dependency for all generated files and the generated output directory 
$(if $(BOB_GENERATED_COMPILE_FILES)$(BOB_GENERATED_LINK_FILES),\
$(BOB_GENERATED_COMPILE_FILES) $(BOB_GENERATED_LINK_FILES): | $(BOB_GENERATED_FILE_DIRECTORY)/.d \
)


# General build rules for all kinds of option files
output/%.ar_options: $(BOB_PROJECT_SUMMARY_FILE) | $(BOB_PROJECT).directories
	$(eval component := $(lastword $(subst /, ,$(@:.ar_options=)) ))
	$(file >$@, $(addprefix output/$(BOB_PROJECT)/components/$(component)/, $(addsuffix .o,$($(component)_SOURCES) $($(component)_GENERATED_SOURCES) $(foreach group,$($(component)_REQUIRED_GROUPS),$($(component)_$(group)_GROUP_SOURCES)))))

output/%.c_options: $(BOB_PROJECT_SUMMARY_FILE) | $(BOB_PROJECT).directories
	$(eval component := $(lastword $(subst /, ,$(@:.c_options=)) ))
	$(eval $(component)_c_COMPILE_OPTIONS := $(TOOLCHAIN_NO_LINKING_INDICATOR) $(BOB_GLOBAL_CFLAGS) $($(component)_CFLAGS) $(addprefix -I,$(BOB_GLOBAL_INCLUDES) $($(component)_REFERENCED_INCLUDES) $(addprefix $($(component)_DIRECTORY),$($(component)_INCLUDES) $($(component)_PRIVATE_INCLUDES))) $(addprefix -D,$(BOB_GLOBAL_DEFINES) $($(component)_DEFINES)))
	$(file >$@,$($(component)_c_COMPILE_OPTIONS))

output/%.cpp_options: $(BOB_PROJECT_SUMMARY_FILE) | $(BOB_PROJECT).directories
	$(eval component := $(lastword $(subst /, ,$(@:.cpp_options=)) ))
	$(eval $(component)_cpp_COMPILE_OPTIONS := $(TOOLCHAIN_NO_LINKING_INDICATOR) $(BOB_GLOBAL_CPPFLAGS) $($(component)_CPPFLAGS) $(addprefix -I,$(BOB_GLOBAL_INCLUDES) $($(component)_REFERENCED_INCLUDES) $(addprefix $($(component)_DIRECTORY),$($(component)_INCLUDES) $($(component)_PRIVATE_INCLUDES))) $(addprefix -D,$(BOB_GLOBAL_DEFINES) $($(component)_DEFINES)))
	$(file >$@,$($(component)_cpp_COMPILE_OPTIONS))

output/%.s_options: $(BOB_PROJECT_SUMMARY_FILE) | $(BOB_PROJECT).directories
	$(eval component := $(lastword $(subst /, ,$(@:.s_options=)) ))
	$(eval $(component)_s_COMPILE_OPTIONS := $(TOOLCHAIN_NO_LINKING_INDICATOR) $(BOB_GLOBAL_ASMFLAGS) $($(component)_ASMFLAGS) $(addprefix -I,$(BOB_GLOBAL_INCLUDES) $($(component)_REFERENCED_INCLUDES) $(addprefix $($(component)_DIRECTORY),$($(component)_INCLUDES) $($(component)_PRIVATE_INCLUDES))) $(addprefix -D,$(BOB_GLOBAL_DEFINES) $($(component)_DEFINES)))
	$(file >$@,$($(component)_s_COMPILE_OPTIONS))

output/%.S_options: $(BOB_PROJECT_SUMMARY_FILE) | $(BOB_PROJECT).directories
	$(eval component := $(lastword $(subst /, ,$(@:.S_options=)) ))
	$(eval $(component)_S_COMPILE_OPTIONS := $(TOOLCHAIN_NO_LINKING_INDICATOR) $(BOB_GLOBAL_CFLAGS) $($(component)_CFLAGS) $(addprefix -I,$(BOB_GLOBAL_INCLUDES) $($(component)_REFERENCED_INCLUDES) $(addprefix $($(component)_DIRECTORY),$($(component)_INCLUDES) $($(component)_PRIVATE_INCLUDES))) $(addprefix -D,$(BOB_GLOBAL_DEFINES) $($(component)_DEFINES)))
	$(file >$@,$($(component)_S_COMPILE_OPTIONS))

output/%.group_c_options: $(BOB_PROJECT_SUMMARY_FILE) | $(BOB_PROJECT).directories
	$(eval component_group := $(lastword $(subst /, ,$(@:.group_c_options=)) ))
	$(eval group := $(lastword $(subst ., ,$(component_group))))
	$(eval component := $(component_group:.$(group)=))
	$(eval $(component)_$(group)_GROUP_COMPILE_OPTIONS := $($(component)_$(group)_GROUP_CFLAGS) $(addprefix -I,$(addprefix $($(component)_DIRECTORY),$($(component)_$(group)_GROUP_INCLUDES) $($(component)_$(group)_GROUP_PRIVATE_INCLUDES))) $(addprefix -D,$($(component)_$(group)_GROUP_DEFINES)))
	$(file >$@,$($(component)_$(group)_GROUP_COMPILE_OPTIONS))

output/%.group_cpp_options: $(BOB_PROJECT_SUMMARY_FILE) | $(BOB_PROJECT).directories
	$(eval component_group := $(lastword $(subst /, ,$(@:.group_cpp_options=)) ))
	$(eval group := $(lastword $(subst ., ,$(component_group))))
	$(eval component := $(component_group:.$(group)=))
	$(eval $(component)_$(group)_GROUP_COMPILE_OPTIONS := $($(component)_$(group)_GROUP_CPPFLAGS) $(addprefix -I,$(addprefix $($(component)_DIRECTORY),$($(component)_$(group)_GROUP_INCLUDES) $($(component)_$(group)_GROUP_PRIVATE_INCLUDES))) $(addprefix -D,$($(component)_$(group)_GROUP_DEFINES)))
	$(file >$@,$($(component)_$(group)_GROUP_COMPILE_OPTIONS))

# Build rule for creating all directories required for a build
$(BOB_PROJECT).directories:
	$(eval component_directories := $(sort $(foreach component,$(BOB_COMPONENT_NAMES),$(foreach source,$($(component)_SOURCES) $($(component)_GENERATED_SOURCES) $(foreach group,$($(component)_REQUIRED_GROUPS),$($(component)_$(group)_GROUP_SOURCES)),$(dir $(addprefix output/$(BOB_PROJECT)/components/$(component)/,$(source)))))))
	$(eval component_directories := $(filter-out $(wildcard $(component_directories)),$(component_directories))) 
	$(if $(component_directories), $(call MKDIR, $(component_directories)))



# # Verify all source files exist: $(info $(filter-out $(wildcard $(addprefix $($(1)_DIRECTORY)/,$($(1)_SOURCES))), $(addprefix $($(1)_DIRECTORY)/,$($(1)_SOURCES))))
# Make component build rules
# Creates component specific build rules
# $(1) is component name
define MAKE_COMPONENT_BUILD_RULES
# Check for missing source files
$(eval MISSING_SOURCE_FILES := $(filter-out $(wildcard $(addprefix $($(1)_DIRECTORY),$($(1)_SOURCES) $(foreach group,$($(1)_REQUIRED_GROUPS),$($(1)_$(group)_GROUP_SOURCES)))), $(addprefix $($(1)_DIRECTORY),$($(1)_SOURCES) $(foreach group,$($(1)_REQUIRED_GROUPS),$($(1)_$(group)_GROUP_SOURCES))))) 
$(if $(MISSING_SOURCE_FILES), $(error Component "$(1)" has missing source files: $(foreach file,$(MISSING_SOURCE_FILES),
 - $(file))))

# Add component library to the list of libraries in this project
$(eval PROJECT_LIBRARIES += $(BOB_OUTPUT_COMPONENT_DIRECTORY)/$(1)/$(1).a )
$(eval PROJECT_OBJECTS += $(addprefix output/$(BOB_PROJECT)/components/$(1)/,$(addsuffix .o,$(filter $(addprefix %.,$(EXTENSIONS)),$($(1)_SOURCES) $($(1)_GENERATED_SOURCES) $(foreach group,$($(1)_REQUIRED_GROUPS),$($(1)_$(group)_GROUP_SOURCES))))) )

# Define build rule for all object files
# Note: % is the source filename including extension
$(BOB_OUTPUT_COMPONENT_DIRECTORY)/$(1)/%.nim.o: $($(1)_DIRECTORY)/%.nim output/$(BOB_PROJECT)/components/$(1)/$(1).c_options
	$$(info Processing Nim file: $$<)
	$$(nim_COMPILER) $$(BOB_GLOBAL_NIMFLAGS) $$(addprefix --cincludes:,$$(BOB_GLOBAL_INCLUDES) ./components/libc/embedded_artistry/include ) $$<
	$$(c_COMPILER) $(if $(TOOLCHAIN_OPTION_FILE_INDICATOR),$(TOOLCHAIN_OPTION_FILE_INDICATOR)output/$(BOB_PROJECT)/components/$(1)/$(1).c_options,$$($(1)_c_COMPILE_OPTIONS)) $(TOOLCHAIN_DEPENDENCY_INDICATOR) -o $$@ $(BOB_GENERATED_FILE_DIRECTORY)/nimcache/$$(notdir $$<).c

$(BOB_OUTPUT_COMPONENT_DIRECTORY)/$(1)/%.o:  $($(1)_DIRECTORY)/% output/$(BOB_PROJECT)/components/$(1)/$(1).options $(BOB_GENERATED_COMPILE_FILES)
	$$(eval filetype := $$(lastword $$(subst ., ,$$(@:.o=))))
	$$(eval group_option :=$$(foreach group,$$($(1)_REQUIRED_GROUPS),$$(if $$(findstring $$<, $$(addprefix $$($(1)_DIRECTORY)/,$$($(1)_$$(group)_GROUP_SOURCES))),$(TOOLCHAIN_OPTION_FILE_INDICATOR)output/$(BOB_PROJECT)/components/$(1)/$(1).$$(group).group_$$(filetype)_options,)))
	$$(info Compiling $$@)
	$$($$(filetype)_COMPILER) $(if $(TOOLCHAIN_OPTION_FILE_INDICATOR),$(TOOLCHAIN_OPTION_FILE_INDICATOR)output/$(BOB_PROJECT)/components/$(1)/$(1).$$(filetype)_options $$(group_option),$$($(1)_$$(filetype)_COMPILE_OPTIONS)) $(TOOLCHAIN_DEPENDENCY_INDICATOR) $(TOOLCHAIN_COMPILE_OUTPUT_FLAG)$$@ $$<
	
$(BOB_OUTPUT_BASE_DIRECTORY)/%/$(1).a: output/%/$(1).ar_options $(PROJECT_OBJECTS)
	$$(info Building $$@)
	$$(RM) -f $$@
	$$(eval component := $$(lastword $$(subst /, ,$$(@:.a=)) ))
	$(ARCHIVER) $(BOB_GLOBAL_ARFLAGS) $(TOOLCHAIN_CREATE_ARCHIVE_INDICATOR)$$@ $(TOOLCHAIN_OPTION_FILE_INDICATOR)$$<

$(BOB_OUTPUT_BASE_DIRECTORY)/%/$(1).options: $(foreach ext,$(EXTENSIONS),$(if $(filter %.$(ext),$($(1)_SOURCES) $(foreach group,$($(1)_REQUIRED_GROUPS),$($(1)_$(group)_GROUP_SOURCES))),output/$(BOB_PROJECT)/components/$(1)/$(1).$(ext)_options)) $(foreach ext,$(EXTENSIONS),$(foreach group,$($(1)_REQUIRED_GROUPS),$(if $(filter %.$(ext),$($(1)_$(group)_GROUP_SOURCES)),output/$(BOB_PROJECT)/components/$(1)/$(1).$(group).group_$(ext)_options)))
	@:

# Include all existing .d dependency files
$(eval include $(wildcard output/$(BOB_PROJECT)/components/$(1)/*.d))

endef


$(BOB_OUTPUT_DIRECTORY)/compile_summary.mk: $(BOB_OUTPUT_DIRECTORY)/build_summary.mk
	$(call CREATE_FILE,    $@,BOB_GENERATED_OBJECTS   := $(strip $(foreach component,$(BOB_COMPONENT_NAMES),$(addprefix output/$(BOB_PROJECT)/components/$(component)/,$(addsuffix .o,$($(component)_SOURCES) $($(component)_GENERATED_SOURCES) $(foreach group,$($(component)_REQUIRED_GROUPS),$($(component)_$(group)_GROUP_SOURCES))))) ))
	$(call APPEND_TO_FILE, $@,BOB_GENERATED_LIBRARIES := $(strip $(sort $(PROJECT_LIBRARIES))) ) 
	$(call APPEND_TO_FILE, $@,BOB_COMPONENT_LIBRARIES := $(strip $(foreach component,$(BOB_COMPONENT_NAMES),$(foreach library,$($(component)_LIBRARIES),$(if $(findstring $(BOB_GENERATED_FILE_DIRECTORY),$(library)),$(library),$(addprefix $($(component)_DIRECTORY),$(library)))) )))
	$(call APPEND_TO_FILE, $@,BOB_FORCE_KEEP_SYMBOLS  := $(strip $(foreach component,$(BOB_COMPONENT_NAMES),$($(component)_FORCE_KEEP_SYMBOLS))) )


###################################################################################################
# Generate the rules for all components in this build
$(foreach component,$(BOB_COMPONENT_NAMES),\
	$(if $(strip $($(component)_SOURCES) $($(component)_GENERATED_SOURCES) $(foreach group,$($(component)_REQUIRED_GROUPS),$($(component)_$(group)_GROUP_SOURCES))),\
		$(if $(wildcard $($(component)_DIRECTORY)/prebuilt_$(component).a),\
			$(eval $(component)_LIBRARIES += prebuilt_$(component).a),\
			$(eval $(call MAKE_COMPONENT_BUILD_RULES,$(component)))\
		)\
	)\
)


# TODO: Update this to use a calculated list of libraries. The current code can't deal with prebuilt libraries
$(BOB_PROJECT).library_files: $(if $(TOOLCHAIN_OBJECTS_ONLY),$(PROJECT_OBJECTS),$(sort $(PROJECT_LIBRARIES))) | $(BOB_PROJECT).directories
	@:
