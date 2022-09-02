
define PRINT_UNSELECTED_OPTION_ERROR
# Find the option features for each unselected option 
$(foreach option,$(UNSELECTED_OPTIONS),
	$(error Unselected option '$(option)'. Please select one of the following features: 
 $(foreach feature,$(foreach component,$(BOB_COMPONENT_NAMES),$($(component)_$(option)_OPTION_FEATURES)),* $(feature)
)))
endef

# $(1) is the option
define PROCESS_OPTION
# Check that a feature has been selected for the option
$(if $(filter $($(CURRENT_OPTION_COMPONENT)_$(1)_OPTION_FEATURES),$(BOB_REQUIRED_FEATURES) $(BOB_PROVIDED_FEATURES)),\
	$(if $(word 2,$(filter $($(CURRENT_OPTION_COMPONENT)_$(1)_OPTION_FEATURES),$(BOB_REQUIRED_FEATURES) $(BOB_PROVIDED_FEATURES))),$(error More than one option selected for '$(1)' option in $($(CURRENT_OPTION_COMPONENT)_MAKEFILE)))\
	$(eval CURRENT_OPTION_LIST := $(filter-out $(1), $(CURRENT_OPTION_LIST)))\
	$(eval CURRENT_OPTION_COMPONENT_LIST := $(filter-out $(CURRENT_OPTION_COMPONENT),$(CURRENT_OPTION_COMPONENT_LIST))),
	# No feature has been selected. Check for a default
	$(if $($(CURRENT_OPTION_COMPONENT)_$(1)_OPTION_DEFAULT),\
		$(eval OPTIONS_WHICH_USE_DEFAULTS += $(1))\
		$(eval $(call PROCESS_COMPONENT_NEW_FEATURE_LIST,$($(CURRENT_OPTION_COMPONENT)_$(1)_OPTION_DEFAULT)))\
		$(eval CURRENT_OPTION_COMPONENT_LIST := $(BOB_COMPONENT_NAMES))\
		$(eval UNSELECTED_OPTIONS :=)\
		$(eval CURRENT_OPTION_LIST:=),\
		# No default, add option to the list of options which don't have a selection yet
		$(eval UNSELECTED_OPTIONS += $(1))\
		$(eval CURRENT_OPTION_LIST := $(filter-out $(1), $(CURRENT_OPTION_LIST)))\
		$(eval CURRENT_OPTION_COMPONENT_LIST := $(filter-out $(CURRENT_OPTION_COMPONENT),$(CURRENT_OPTION_COMPONENT_LIST)))
	)
)
endef

# CURRENT_OPTION_LIST is the list of options to be evaluated for a particular component
define PROCESS_OPTION_FOR_COMPONENT
$(eval $(call PROCESS_OPTION,$(firstword $(CURRENT_OPTION_LIST))))
$(if $(CURRENT_OPTION_LIST),$(eval $(call PROCESS_OPTION_FOR_COMPONENT)))
endef

# PROCESS BUILD OPTIONS
# CURRENT_OPTION_COMPONENT_LIST is component list
define PROCESS_OPTIONS_FOR_EACH_COMPONENT
$(eval CURRENT_OPTION_COMPONENT := $(firstword $(CURRENT_OPTION_COMPONENT_LIST)))
$(eval CURRENT_OPTION_LIST := $($(CURRENT_OPTION_COMPONENT)_OPTIONS) $(foreach feature,$(filter $(BOB_REQUIRED_FEATURES) $(BOB_PROVIDED_FEATURES), $($(CURRENT_OPTION_COMPONENT)_SUPPORTED_FEATURES)),$($(CURRENT_OPTION_COMPONENT)_$(feature)_OPTIONS)))
$(if $(CURRENT_OPTION_LIST),\
	$(eval $(call PROCESS_OPTION_FOR_COMPONENT)),
	$(eval CURRENT_OPTION_COMPONENT_LIST := $(filter-out $(CURRENT_OPTION_COMPONENT),$(CURRENT_OPTION_COMPONENT_LIST)))
)
$(if $(CURRENT_OPTION_COMPONENT_LIST),\
	$(eval $(call PROCESS_OPTIONS_FOR_EACH_COMPONENT)),
	$(if $(UNSELECTED_OPTIONS),$(call PRINT_UNSELECTED_OPTION_ERROR))
)
endef

define PROCESS_OPTION_LIST
$(eval CURRENT_OPTION_COMPONENT_LIST := $(BOB_COMPONENT_NAMES))
$(eval $(call PROCESS_OPTIONS_FOR_EACH_COMPONENT))
endef


