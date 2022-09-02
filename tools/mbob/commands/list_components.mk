list_components:
	@:

include configuration.mk
include $(BOB_SDK_DIRECTORY)tools/makefiles/component_processing.mk

JSON_FILENAME := components.json
COMMA :=,
STANDARD_COMPONENT_VARIABLES := $(filter-out OPTIONS,$(COMPONENT_NAMED_VARIABLES))
STANDARD_FEATURE_VARIABLES   := $(filter-out OPTIONS,$(FEATURE_NAMED_VARIABLES))
PROCESSED_FEATURES :=
SLASH_QUOTE_START := \"
SLASH_QUOTE_END   := \"
ESC_QUOTE         := \"

# $(1) is the list of directories to search
define SEARCH_FOR_COMPONENTS
$(foreach directory,$(1),
	# Check for a component
	$(eval MAKEFILE:=$(wildcard $(directory)/$(lastword $(subst /, ,$(directory))).mk))
	$(if $(MAKEFILE),$(info - $(subst /,.,$(directory)))$(eval ALL_COMPONENTS += $(subst /,.,$(directory))))
	# Search through sub-directories
	$(eval SUB_DIRECTORIES :=$(subst /.,,$(wildcard $(directory)/*/.)))
	$(if $(SUB_DIRECTORIES),$(eval $(call SEARCH_FOR_COMPONENTS,$(SUB_DIRECTORIES))))
)
endef

define FIND_ALL_COMPONENTS
$(foreach component_directory,$(sort $(filter-out .,$(BOB_COMPONENT_DIRECTORIES))),\
    $(info )$(info Components under $(component_directory): )\
    $(eval $(call SEARCH_FOR_COMPONENTS,$(sort $(subst ./,,$(subst /.,,$(wildcard $(component_directory)/*/.))))))\
)
endef

# $(1) is the variable
define VARIABLE_TO_JSON
$(eval _TEMP_COMMA :=)$(if $(word 2,$(1)), [ $(foreach v,$(strip $(1)),$(_TEMP_COMMA)"$(v)"$(if $(word 2,$(1)),$(eval _TEMP_COMMA:=,))) ], "$(strip $(1))")
endef

# $(1) is the component
define PRINT_OPTIONS
         "OPTIONS" : {
            $(foreach option,$($(1)_OPTIONS), "$(option)" : { "features" : $(call VARIABLE_TO_JSON,$($(NAME)_$(option)_OPTION_FEATURES))$(if $($(NAME)_$(option)_OPTION_DEFAULT),$(COMMA) "default" : "$($(NAME)_$(option)_OPTION_DEFAULT)")})
         }
endef

# $(1) is the list of features, $(2) is the filename
define PRINT_FEATURES
$(eval _TEMP_FEATURE_COMMA :=)
$(eval CURRENT_FEATURE := $(firstword $(1)))
$(eval PROCESSED_FEATURES += $(CURRENT_FEATURE))
$(eval FEATURE_LIST :=$(filter-out $(PROCESSED_FEATURES),$(1) $($(NAME)_$(CURRENT_FEATURE)_REQUIRED_FEAUTRES) $(foreach option,$($(NAME)_$(CURRENT_FEATURE)_OPTIONS),$($(NAME)_$(option)_OPTION_FEATURES))))
$(call APPEND_TO_FILE,$(2),	          "$(CURRENT_FEATURE)" : { $(strip $(foreach var,$(STANDARD_FEATURE_VARIABLES),\
$(if $($(NAME)_$(CURRENT_FEATURE)_$(var)),$(_TEMP_FEATURE_COMMA) "$(var)" : $(call VARIABLE_TO_JSON,$($(NAME)_$(CURRENT_FEATURE)_$(var)))$(eval _TEMP_FEATURE_COMMA:=,)) )\
$(if $($(NAME)_$(CURRENT_FEATURE)_OPTIONS),$(_TEMP_FEATURE_COMMA)$(call PRINT_OPTIONS,$(NAME)_$(CURRENT_FEATURE)))) }$(if $(FEATURE_LIST),$(COMMA)))
$(if $(FEATURE_LIST),$(call PRINT_FEATURES,$(FEATURE_LIST),$(2)))
endef

# Appends the component related info to the provided file name
# $(1) is the component name, $(2) is the file
define JSON_COMPONENT_INFO
$(foreach var,$(STANDARD_COMPONENT_VARIABLES),$(if $($(NAME)_$(var)),$(call APPEND_TO_FILE,$(2),         "$(var)" : $(call VARIABLE_TO_JSON, $($(NAME)_$(var)))$(COMMA))))
$(foreach group,$($(NAME)_REQUIRED_GROUPS),$(foreach var,$(COMPONENT_GROUP_NAMED_VARIABLES),$(if $($(NAME)_$(group)_GROUP_$(var)),$(call APPEND_TO_FILE,$(2),         "$(NAME)_$(group)_GROUP_$(var)" : $(call VARIABLE_TO_JSON,$($(NAME)_$(group)_GROUP_$(var)))$(COMMA)))))
$(if $($(NAME)_OPTIONS),$(call APPEND_TO_FILE,$(2),$(call PRINT_OPTIONS,$(NAME))$(COMMA)))
$(eval FEATURE_LIST :=$(sort $($(NAME)_SUPPORTED_FEATURES) $(foreach option,$($(NAME)_OPTIONS),$($(NAME)_$(option)_OPTION_FEATURES))))
$(if $(FEATURE_LIST),\
	$(call APPEND_TO_FILE,$(2),         "features" : {)
	$(call PRINT_FEATURES,$(sort $($(NAME)_SUPPORTED_FEATURES) $(foreach option,$($(NAME)_OPTIONS),$($(NAME)_$(option)_OPTION_FEATURES))),$(2))
	$(call APPEND_TO_FILE,$(2),            }$(COMMA))
)
$(call APPEND_TO_FILE,$(2),         "component_name" : "$(NAME)"$(COMMA))
$(call APPEND_TO_FILE,$(2),         "dot_notation_name" : "$(1)")
endef

define GENERATE_JSON
$(eval COMPONENT_START :=)
$(call CREATE_FILE,$(JSON_FILENAME),{)
$(call APPEND_TO_FILE,$(JSON_FILENAME),   "meta" : {
      "version" : "1.0.0"$(COMMA)
      "type" : "components"
   }$(COMMA))
$(call APPEND_TO_FILE,$(JSON_FILENAME),   "data" : [)
$(foreach component,$(ALL_COMPONENTS),\
	$(if $(filter $(component),$(PROCESSED_COMPONENTS)),,\
		$(call PROCESS_COMPONENT_MAKEFILE, $(component))
		$(eval PROCESSED_COMPONENTS += $(component))
		$(if $(COMPONENT_START),$(call APPEND_TO_FILE,$(JSON_FILENAME),      $(COMPONENT_START)))
		$(call APPEND_TO_FILE,$(JSON_FILENAME),      {)
		$(call JSON_COMPONENT_INFO,$(component),$(JSON_FILENAME))
		$(if $(word 2,$(ALL_COMPONENTS)),$(eval COMPONENT_START:=},))
	)
)
$(call APPEND_TO_FILE,$(JSON_FILENAME),      })
$(call APPEND_TO_FILE,$(JSON_FILENAME),   ])
$(call APPEND_TO_FILE,$(JSON_FILENAME),})
endef

# ALL_COMPONENTS := API.micrium_os
$(eval $(call FIND_ALL_COMPONENTS))
$(eval $(call GENERATE_JSON))
