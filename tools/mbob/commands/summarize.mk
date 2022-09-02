include $(BOB_MAKEFILE_DIRECTORY)/component_processing.mk
include $(BOB_MAKEFILE_DIRECTORY)/variables.mk

summarize:: $(BOB_PROJECT_SUMMARY_FILE)

# Reset all the BOB variables to ensure they are not polluted
$(foreach variable,$(BOB_NAMED_VARIABLES),$(eval BOB_$(variable):= ))

# Set initial required component list
$(eval BOB_REQUIRED_COMPONENTS += $(COMPONENT_LIST) $(BOB_DEFAULT_COMPONENTS))

# Set initial required features list
$(eval BOB_REQUIRED_FEATURES := $(FEATURE_LIST))


ifeq ($(wildcard $(BOB_PROJECT_SUMMARY_FILE)),)
# Process component list
$(eval $(call DO_COMPONENT_PROCESSING))

# Appends the component related info to the provided file name
# $(1) is the component name, $(2) is the file
define LOG_COMPONENT_INFO
$(foreach var,$(COMPONENT_NAMED_VARIABLES),$(if $(strip $($(1)_$(var))),$(call APPEND_TO_FILE, $(2),$(1)_$(var) := $(strip $($(1)_$(var))))))
$(foreach group,$($(1)_REQUIRED_GROUPS),\
	$(foreach var,$(COMPONENT_GROUP_NAMED_VARIABLES),\
		$(if $($(1)_$(group)_GROUP_$(var)),$(call APPEND_TO_FILE, $(2),$(1)_$(group)_GROUP_$(var) := $(strip $($(1)_$(group)_GROUP_$(var)))))))
endef

define REPORT_MISSING_MAKEFILE
$(if $(BOB_MISSED_MAKEFILES),$(error Cannot find component "$(firstword $(BOB_MISSED_MAKEFILES))" makefile. 
References found in: \
$(foreach component,$(BOB_PROCESSED_FULL_COMPONENTS) $(BOB_REFERENCED_FULL_COMPONENTS),$(if $(filter %$(firstword $(BOB_MISSED_MAKEFILES)),$($($(component)_NAME)_REQUIRED_COMPONENTS) $($($(component)_NAME)_REFERENCED_COMPONENTS)),
- $(component)))
))
endef

# build_summary rule
$(BOB_PROJECT_SUMMARY_FILE): Makefile $(BOB_MAKEFILE_DIRECTORY)/commands/summarize.mk $(foreach component,$(BOB_COMPONENT_NAMES),$($(component)_MAKEFILE)) $(wildcard $(BOB_MAKEFILE_DIRECTORY)/*.mk) | $(BOB_OUTPUT_DIRECTORY)/.d
	$(info Updating build_summary.mk)
	$(call CREATE_FILE,$@,# Build summary for $(BOB_PROJECT) )
	$(call APPEND_TO_FILE,$@,PROJECT :=$(BOB_PROJECT))
	$(call APPEND_TO_FILE,$@,BUILD_TYPE :=$(BUILD_TYPE))
	$(foreach var,      $(BOB_NAMED_VARIABLES), $(call APPEND_TO_FILE,$@,BOB_$(var) :=$(strip $(BOB_$(var)))))
	$(foreach list,     $(BOB_DECLARED_LISTS),  $(call APPEND_TO_FILE,$@,BOB_LIST_$(list)              :=$(strip $(BOB_LIST_$(list)))) \
	                                            $(call APPEND_TO_FILE,$@,BOB_LIST_DEPENDENCIES_$(list) :=$(strip $(BOB_LIST_DEPENDENCIES_$(list)))))
	$(foreach command,  $(SUPPORTED_COMMANDS),  $(if $(BOB_$(command)_TOOLCHAIN_MAKEFILES),\
	                                              $(call APPEND_TO_FILE,$@,BOB_$(command)_TOOLCHAIN_MAKEFILES :=$(strip $(BOB_$(command)_TOOLCHAIN_MAKEFILES)))))
	$(foreach command,  $(SUPPORTED_COMMANDS),  $(if $(BOB_$(command)_COMMAND_TARGETS),\
	                                              $(call APPEND_TO_FILE,$@,BOB_$(command)_COMMAND_TARGETS :=$(strip $(BOB_$(command)_COMMAND_TARGETS)))\
	                                              $(foreach target,$(BOB_$(command)_COMMAND_TARGETS),\
	                                              	$(foreach var,$(COMMAND_TARGET_NAMED_VARIABLES),\
	                                              		$(if $(BOB_$(target)_$(command)_TARGET_$(var)),\
	                                              			$(call APPEND_TO_FILE,$@,BOB_$(target)_$(command)_TARGET_$(var) :=$(strip $(BOB_$(target)_$(command)_TARGET_$(var))))\
                                              			)\
                                          			)\
                                      			)))
	$(foreach component,$(BOB_COMPONENT_NAMES), $(call LOG_COMPONENT_INFO,$(component),$@))
	
	$(eval $(call REPORT_MISSING_MAKEFILE))

else
include $(BOB_PROJECT_SUMMARY_FILE)
$(BOB_PROJECT_SUMMARY_FILE): Makefile $(BOB_MAKEFILE_DIRECTORY)/commands/summarize.mk $(foreach component,$(BOB_COMPONENT_NAMES),$($(component)_MAKEFILE)) $(wildcard $(BOB_MAKEFILE_DIRECTORY)/*.mk) | $(BOB_OUTPUT_DIRECTORY)/.d
	$(info Rebuilding build_summary.mk)
	$(QUIET)$(RM) $(BOB_PROJECT_SUMMARY_FILE)
	$(QUIET)$(MAKE) $(SILENCE_MAKE) -j $(JOBS) $(COMPONENT_LIST) $(addprefix +,$(FEATURE_LIST)) BOB_COMMAND=summarize BOB_PROJECT=$(BOB_PROJECT) BOB_SDK_DIRECTORY=$(BOB_SDK_DIRECTORY) 
endif

$(BOB_OUTPUT_DIRECTORY):
	$(call MKDIR,$@)


