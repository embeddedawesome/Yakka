include $(BOB_MAKEFILE_DIRECTORY)/variables.mk

###################################################################################################
# CHECK_FEATURE_NAME
# Checks the validity of the feature name
# $(1) is the feature name
define CHECK_FEATURE_NAME
$(if $(word 2, $(subst ., ,$(1))),$(error Feature $(1) is an illegal name. Must not contain '.'))
endef

###################################################################################################
# PROCESS_REQUIRED_FEATURES_LIST
# Processes the final list of required features updated each components named variables with the feature named variables
# This macro does not accept any arguments 
define PROCESS_REQUIRED_FEATURES_LIST
$(foreach feature,$(sort $(BOB_REQUIRED_FEATURES) $(BOB_PROVIDED_FEATURES)),\
    $(foreach component,$(BOB_COMPONENT_NAMES),\
        $(if $(findstring $(feature),$($(component)_SUPPORTED_FEATURES) $($(component)_OPTION_FEATURES) $($(component)_REQUIRED_FEATURES) $($(component)_PROVIDED_FEATURES) ),\
            $(foreach variable,$(FEATURE_NAMED_VARIABLES),\
                $(if $($(component)_$(feature)_$(variable)),\
                    $(if $(filter $(component)_$(feature),$(BOB_COMPONENT_NAMES)),$(error Component '$(component)' [$($(component)_MAKEFILE)] with feature '$(feature)' has namespace collision with component $(component)_$(feature) [$($(component)_$(feature)_MAKEFILE)]))\
                    $(eval $(component)_$(variable) += $($(component)_$(feature)_$(variable)))\
                 )\
             )\
         )\
     )\
)
$(eval BOB_GLOBAL_DEFINES += $(addsuffix _FEATURE_REQUIRED,$(BOB_REQUIRED_FEATURES) $(BOB_PROVIDED_FEATURES)))
endef

# $(1) is the list of features to process
define PROCESS_FEATURE_LIST
$(foreach component,$(BOB_COMPONENT_NAMES),\
	$(foreach feature,$(filter-out $($(component)_PROCESSED_FEATURES),$(BOB_REQUIRED_FEATURES) $(BOB_PROVIDED_FEATURES)),\
		$(call CHECK_FEATURE_NAME, $(feature))\
		$(eval $(component)_OPTION_LIST     +=$($(component)_$(feature)_OPTIONS))\
		$(eval $(component)_OPTION_FEATURES +=$(sort $(foreach option,$($(component)_$(feature)_OPTIONS),$($(component)_$(option)_OPTION_FEATURES))))\
		$(eval $(component)_$(feature)_REQUIRED_COMPONENTS   :=$(foreach item,$($(component)_$(feature)_REQUIRED_COMPONENTS),$(if $(subst .,,$(firstword $(subst .,. ,$(item)))),$(item),$($(component)_FULL_NAME)$(item))))\
		$(eval $(component)_$(feature)_REFERENCED_COMPONENTS :=$(foreach item,$($(component)_$(feature)_REFERENCED_COMPONENTS),$(if $(subst .,,$(firstword $(subst .,. ,$(item)))),$(item),$($(component)_FULL_NAME)$(item))))\
		$(eval BOB_REQUIRED_COMPONENTS :=$(sort $(BOB_REQUIRED_COMPONENTS) $($(component)_$(feature)_REQUIRED_COMPONENTS)))\
		$(eval BOB_REQUIRED_FEATURES   :=$(sort $(BOB_REQUIRED_FEATURES) $($(component)_$(feature)_REQUIRED_FEATURES)))\
		$(eval BOB_PROVIDED_FEATURES   :=$(sort $(BOB_PROVIDED_FEATURES) $($(component)_$(feature)_PROVIDED_FEATURES)))\
		$(eval $(component)_PROCESSED_FEATURES+=$(feature))\
	)\
)
$(eval BOB_PROCESSED_FEATURES := $(sort $(BOB_PROCESSED_FEATURES) $(1)))
endef


define CHECK_FOR_DEFAULT_OPTIONS
$(foreach component,$(BOB_COMPONENT_NAMES),\
	$(foreach option,$($(component)_OPTION_LIST),\
		$(if $(filter $($(component)_$(option)_OPTION_FEATURES),$(BOB_REQUIRED_FEATURES) $(BOB_PROVIDED_FEATURES) ), ,\
			$(if $($(component)_$(option)_OPTION_DEFAULT),\
				$(eval BOB_REQUIRED_FEATURES +=$($(component)_$(option)_OPTION_DEFAULT)),\
				$(error Error: No default option for $(option) in $(component)))\
		)\
	)\
)
endef
