-include $(BOB_SDK_DIRECTORY)configuration.mk

include $(BOB_MAKEFILE_DIRECTORY)/component_features.mk
include $(BOB_MAKEFILE_DIRECTORY)/component_options.mk
include $(BOB_MAKEFILE_DIRECTORY)/variables.mk
include $(BOB_MAKEFILE_DIRECTORY)/build_system_tools.mk

                          
# Separate target string into components
BOB_COMPONENTS := $(subst -, ,$(MAKECMDGOALS))

###################################################################################################
# PROCESS_RELATIVE_ADDRESSING
# Process an item ($1) and fix the relative addressing to a given component name ($2)
define PROCESS_RELATIVE_ADDRESSING
$(if $(subst .,,$(firstword $(subst .,. ,$(1)))),$(1),$($(2)_FULL_NAME)$(1))
endef

###################################################################################################
# DO_COMPONENT_PROCESSING
# The entry point for component processing.
define DO_COMPONENT_PROCESSING
# Do any pre-processing activity
$(eval MORE_PROCESSING:=)

# Process the component list. Note: PROCESS_COMPONENT_LIST calls itself recursively
$(eval UNPROCESSED_COMPONENTS := $(filter-out $(BOB_PROCESSED_COMPONENTS),$(BOB_REQUIRED_COMPONENTS)))
$(if $(UNPROCESSED_COMPONENTS),$(eval $(call PROCESS_COMPONENT_LIST,$(UNPROCESSED_COMPONENTS))))

# Process the features
$(call PROCESS_FEATURE_LIST,$(filter-out $(BOB_PROCESSED_FEATURES),$(BOB_REQUIRED_FEATURES) $(BOB_PROVIDED_FEATURES)))
$(eval UNPROCESSED_FEATURES := $(filter-out $(BOB_PROCESSED_FEATURES),$(BOB_REQUIRED_FEATURES) $(BOB_PROVIDED_FEATURES)))

# If there are any unprocessed components, process them now
$(if $(filter-out $(BOB_PROCESSED_COMPONENTS),$(BOB_REQUIRED_COMPONENTS)),$(eval MORE_PROCESSING:=yes))

# If there are any unprocessed features, process them now, otherwise check for default options 
$(if $(MORE_PROCESSING)$(UNPROCESSED_FEATURES),$(eval MORE_PROCESSING:=yes), $(eval $(call CHECK_FOR_DEFAULT_OPTIONS)))

# Update unprocessed feature list after processing default options 
$(eval UNPROCESSED_FEATURES := $(filter-out $(BOB_PROCESSED_FEATURES),$(BOB_REQUIRED_FEATURES) $(BOB_PROVIDED_FEATURES)))

# If there are any unprocessed features, process them now, otherwise all recursive processing is complete. Do final post-processing activities. 
$(if $(MORE_PROCESSING)$(UNPROCESSED_FEATURES),$(eval $(call DO_COMPONENT_PROCESSING)), 
	# Do post-processing activities.
	$(eval $(call PROCESS_REQUIRED_FEATURES_LIST))
	$(eval $(call PROCESS_REFERENCED_COMPONENT_LIST))
	$(eval $(call PROCESS_DECLARED_LISTS))
	$(eval $(call PROCESS_BOB_GLOBAL_VARIABLES))
	$(eval $(call PROCESS_REFERENCED_INCLUDES))
	$(eval $(call PROCESS_COMMAND_VARIABLES))
	$(eval BOB_COMPONENT_PROCESSING_COMPLETE:=true)
)

endef

###################################################################################################
# FIND_COMPONENT_MAKEFILE_WITHOUT_DOT_NOTATION
# Try find a component by its name assuming there is no dot-notation and therefore no directories.
# This will generate: COMPONENT_DIRECTORY, COMPONENT_NAME and COMPONENT_MAKEFILE
# $(1) is the name of the component 
define FIND_COMPONENT_MAKEFILE_WITHOUT_DOT_NOTATION
# Convert dot notation to component directory and component name
$(eval COMPONENT_DIRECTORY := )
$(eval COMPONENT_NAME      :=$(strip $(1)))

# Find component
$(eval COMPONENT_MAKEFILE :=$(strip $(wildcard $(foreach directory, $(BOB_COMPONENT_DIRECTORIES), $(directory)/$(COMPONENT_NAME)/$(COMPONENT_NAME).mk))))
endef 

###################################################################################################
# FIND_COMPONENT_MAKEFILE
# Try find a component by its dot-notation name.
# This will generate: COMPONENT_DIRECTORY, COMPONENT_NAME and COMPONENT_MAKEFILE
# $(1) is the name of the component in dot-notation
define FIND_COMPONENT_MAKEFILE
# Convert dot notation to component directory and component name
$(eval COMPONENT_DIRECTORY :=$(call NORMALIZE,.,$(subst .,/,$(strip $(1)))))
$(eval COMPONENT_NAME      :=$(notdir $(COMPONENT_DIRECTORY)))

# Find component
$(eval COMPONENT_MAKEFILE :=$(call NORMALIZE,$(BOB_SDK_DIRECTORY),$(strip $(wildcard $(foreach directory, $(BOB_COMPONENT_DIRECTORIES), $(BOB_SDK_DIRECTORY)/$(directory)/$(COMPONENT_DIRECTORY)/$(COMPONENT_NAME).mk)))))

# Verify component exists otherwise try without dot notation
$(if $(COMPONENT_MAKEFILE),,$(eval $(call FIND_COMPONENT_MAKEFILE_WITHOUT_DOT_NOTATION,$(1))))

# Prepend the SDK directory path or check if the component is the project in a remote directory
$(if $(COMPONENT_MAKEFILE),
	# Prepend the SDK directory path
	$(eval COMPONENT_MAKEFILE :=$(BOB_SDK_DIRECTORY)$(COMPONENT_MAKEFILE)),
	# Look for component in the project path
	$(eval COMPONENT_MAKEFILE :=$(call NORMALIZE,.,$(strip $(wildcard ./$(COMPONENT_NAME).mk))))
)

# Verify component exists after second search
$(if $(COMPONENT_MAKEFILE),\
	# Set the full dot-notation name of the component which is also the component path
	$(eval COMPONENT_FULL_NAME :=$(subst /,.,$(call CLEAN_DIRECTORY,$(COMPONENT_MAKEFILE))))
	$(eval COMPONENT_PATH      :=$(COMPONENT_FULL_NAME))
	$(eval COMPONENT_DIRECTORY :=$(dir $(COMPONENT_MAKEFILE)))
	# Create a mapping from the string used to find the component to its full name
	$(eval $(1)_TO_FULL_NAME :=$(COMPONENT_FULL_NAME))
	,
	# Couldn't find makefile
	$(eval COMPONENT_FULL_NAME :=__UNKNOWN__)
	$(eval COMPONENT_PATH      :=)
	$(eval COMPONENT_DIRECTORY :=)
	$(eval BOB_MISSED_MAKEFILES += $(1)))
endef

###################################################################################################
# PROCESS_COMPONENT_LIST
# Recursively processes a list of components stored in dot notation. 
# $(1) is the list of components that can grow while processing individual components
define PROCESS_COMPONENT_LIST
$(foreach component,$(1),
    # Find component makefile
    $(eval $(call FIND_COMPONENT_MAKEFILE,$(component)))
    
    $(if $(filter $(COMPONENT_FULL_NAME),__UNKNOWN__),$(error Couldn't find component $(1)))
    
    # Process the file if it isn't already processed
    $(if $(filter $(COMPONENT_FULL_NAME),$(BOB_PROCESSED_FULL_COMPONENTS)),,
    	$(call PROCESS_COMPONENT_MAKEFILE,$(component),$(COMPONENT_MAKEFILE))
        $(eval BOB_PROCESSED_FULL_COMPONENTS +=$(COMPONENT_FULL_NAME))
    
    	# Fix relative component addressing for required and referenced components
    	$(eval $(NAME)_REQUIRED_COMPONENTS   :=$(foreach item,$($(NAME)_REQUIRED_COMPONENTS),$(call PROCESS_RELATIVE_ADDRESSING,$(item),$(NAME))))
    	$(eval $(NAME)_REFERENCED_COMPONENTS :=$(foreach item,$($(NAME)_REFERENCED_COMPONENTS),$(call PROCESS_RELATIVE_ADDRESSING,$(item),$(NAME))))
    
    	# Add the component required components to the list but downgrading excluded components as referenced
    	$(eval BOB_EXCLUDED_COMPONENTS       := $(sort $(BOB_EXCLUDED_COMPONENTS) $($(NAME)_EXCLUDED_COMPONENTS)))
    	$(eval BOB_REQUIRED_COMPONENTS       += $(filter-out $(BOB_EXCLUDED_COMPONENTS),$($(NAME)_REQUIRED_COMPONENTS)))
    	$(eval $(NAME)_REFERENCED_COMPONENTS += $(filter $(BOB_EXCLUDED_COMPONENTS),$($(NAME)_REQUIRED_COMPONENTS)))
    )
    $(eval $(component)_COMPONENT_NAME := $($(COMPONENT_FULL_NAME)_NAME))
    $(eval BOB_PROCESSED_COMPONENTS :=$(sort $(BOB_PROCESSED_COMPONENTS) $(component)))
)
endef
    # Add each new dependency to the list filtering out already processed ones
    #$(eval CURRENT_LIST :=$(strip $(filter-out $(BOB_PROCESSED_COMPONENTS),$(CURRENT_LIST) $($(NAME)_REQUIRED_COMPONENTS))))

###################################################################################################
# PROCESS_COMPONENT_MAKEFILE
# Processes an individual component accepted as an argument
# $(1) is the component
# $(2) is the path to the component makefile
define PROCESS_COMPONENT_MAKEFILE
# Parse component makefile
$(eval NAME :=__UNKNOWN__)

# It is assumed that the component will define NAME
$(eval $(call INCLUDE_ADDITIONAL_MAKEFILE,$(2)))

$(if $(filter $(NAME),__UNKNOWN__),$(error Component '$(1)' with descriptor file '$(2)' doesn't have a NAME))

# Create the mapping of the full dot-notation name to component name
$(eval $(COMPONENT_FULL_NAME)_NAME := $(NAME))

# Create additional component variables
$(eval $(NAME)_DIRECTORY :=$(COMPONENT_DIRECTORY))
$(eval $(NAME)_MAKEFILE  :=$(COMPONENT_MAKEFILE))
$(eval $(NAME)_FULL_NAME :=$(COMPONENT_FULL_NAME))

# Extract generated sources and store them separately 
$(eval $(NAME)_GENERATED_SOURCES :=$(filter $(BOB_GENERATED_FILE_DIRECTORY)/%,$($(NAME)_SOURCES)))
$(eval $(NAME)_SOURCES :=$(filter-out $($(NAME)_GENERATED_SOURCES),$($(NAME)_SOURCES)))

# Process special BOB variables
$(if $(filter BOB_TOOLCHAIN,$($(NAME)_SUPPORTED_FEATURES)),$(eval BOB_TOOLCHAIN_COMPONENTS += $(NAME)))
$(eval BOB_DECLARED_LISTS += $($(NAME)_DECLARED_LISTS))
$(eval BOB_GENERATED_COMPILE_FILES += $(addprefix $(BOB_GENERATED_FILE_DIRECTORY)/,$($(NAME)_GENERATED_COMPILE_FILES)))
$(eval BOB_GENERATED_LINK_FILES += $(addprefix $(BOB_GENERATED_FILE_DIRECTORY)/,$($(NAME)_GENERATED_LINK_FILES)))

# Process component features and options
$(foreach variable,$(BOB_FEATURE_VARIABLES),$(eval BOB_$(variable) += $($(NAME)_$(variable))))
$(eval BOB_OPTIONS += $($(NAME)_OPTIONS))
$(eval $(NAME)_OPTION_LIST += $($(NAME)_OPTIONS))
$(eval $(NAME)_OPTION_FEATURES +=$(sort $(foreach option,$($(NAME)_OPTIONS),$($(NAME)_$(option)_OPTION_FEATURES))))

# Add component name to the list
$(eval BOB_COMPONENT_NAMES := $(sort $(BOB_COMPONENT_NAMES) $(NAME)))
endef


###################################################################################################
# PROCESS_REFERENCED_COMPONENT
# Extract the GLOBAL_INCLUDES of a specific component  
# $(1) is the referenced component
define PROCESS_REFERENCED_COMPONENT
# Find component makefile
$(eval $(call FIND_COMPONENT_MAKEFILE,$(1)))

# Parse component makefile
$(if $(filter $(COMPONENT_FULL_NAME),$(BOB_REFERENCED_FULL_COMPONENTS) $(BOB_PROCESSED_FULL_COMPONENTS)),,
	$(eval NAME :=__UNKNOWN__)
	# It is assumed that the component will define NAME
	$(eval $(call INCLUDE_ADDITIONAL_MAKEFILE,$(COMPONENT_MAKEFILE)))
	$(if $(filter $(NAME),__UNKNOWN__),$(error Component '$(COMPONENT_FULL_NAME)' with descriptor file '$(COMPONENT_MAKEFILE)' doesn't have a NAME))
	# Create the mapping of the full dot-notation name to component name
	$(eval $(COMPONENT_FULL_NAME)_NAME := $(NAME))
	
	# Create additional component variables
	$(eval $(1)_COMPONENT_NAME := $(NAME))
	$(eval $(NAME)_DIRECTORY :=$(COMPONENT_DIRECTORY))
	$(eval $(NAME)_MAKEFILE  :=$(COMPONENT_MAKEFILE))
	$(eval $(NAME)_FULL_NAME :=$(COMPONENT_FULL_NAME))
	
	# Fix relative component addressing for required and referenced components
	$(eval $(NAME)_REQUIRED_COMPONENTS   :=$(foreach item,$($(NAME)_REQUIRED_COMPONENTS),$(call PROCESS_RELATIVE_ADDRESSING,$(item),$(NAME))))
	$(eval $(NAME)_REFERENCED_COMPONENTS :=$(foreach item,$($(NAME)_REFERENCED_COMPONENTS),$(call PROCESS_RELATIVE_ADDRESSING,$(item),$(NAME))))

	# Add new GLOBAL_INCLUDES to BUILD variable
	$(eval BOB_GLOBAL_INCLUDES += $(addprefix $(COMPONENT_DIRECTORY),$($(NAME)_GLOBAL_INCLUDES)))
	
	$(eval BOB_GLOBAL_DEFINES += $($(NAME)_GLOBAL_DEFINES) $(foreach feature,$(filter $(BOB_REQUIRED_FEATURES) $(BOB_PROVIDED_FEATURES),$($(NAME)_SUPPORTED_FEATURES)),$($(NAME)_$(feature)_GLOBAL_DEFINES)))

	# Add required components to the referenced component list
	$(eval BOB_REFERENCED_COMPONENTS := $(sort $(filter-out $(BOB_PROCESSED_COMPONENTS),$(BOB_REFERENCED_COMPONENTS)\
		$(foreach item, $($(NAME)_REQUIRED_COMPONENTS) $($(NAME)_REFERENCED_COMPONENTS) $(foreach feature,$(filter $(BOB_REQUIRED_FEATURES) $(BOB_PROVIDED_FEATURES),$($(NAME)_SUPPORTED_FEATURES)),$($(NAME)_$(feature)_REQUIRED_COMPONENTS)),\
			$(call PROCESS_RELATIVE_ADDRESSING,$(item),$(NAME)))))
	)
	# Add component to the list
	$(eval BOB_REFERENCED_FULL_COMPONENTS += $(COMPONENT_FULL_NAME))
)
endef

###################################################################################################
# REFERENCED_COMPONENT_LIST_LOOP
# Iterate through the list of referenced components analyzing only those not already processed as 
# required components and extract the GLOBAL_INCLUDES
# $(1) is the list of referenced components
define REFERENCED_COMPONENT_LIST_LOOP
# Find the first reference and process it
$(eval TEMP := $(firstword $(1)))
$(if $(TEMP),$(eval $(call PROCESS_REFERENCED_COMPONENT,$(TEMP))))

# Update the list of already processed references, calculate new list of unprocessed ones and recurse
$(eval PROCESSED_REFERENCES += $(TEMP))
$(eval TEMP_LIST := $(sort $(filter-out $(PROCESSED_REFERENCES),$(BOB_REFERENCED_COMPONENTS))))
$(if $(TEMP_LIST),$(eval $(call REFERENCED_COMPONENT_LIST_LOOP,$(TEMP_LIST))))
endef

###################################################################################################
# PROCESS_REFERENCED_COMPONENT_LIST
# Iterate through the list of referenced components analyzing only those not already processed as 
# required components and extract the GLOBAL_INCLUDES
# This macro doesn't take arguments
define PROCESS_REFERENCED_COMPONENT_LIST
# Create the initial list 
$(eval BOB_REFERENCED_COMPONENTS := $(sort $(foreach component,$(BOB_COMPONENT_NAMES),$($(component)_REFERENCED_COMPONENTS))))
# Filter out components that have already been fully processed
$(eval BOB_REFERENCED_COMPONENTS := $(sort $(filter-out $(BOB_PROCESSED_COMPONENTS),$(BOB_REFERENCED_COMPONENTS))))
$(eval $(call REFERENCED_COMPONENT_LIST_LOOP, $(BOB_REFERENCED_COMPONENTS)))
endef

###################################################################################################
# PROCESS_REFERENCED_INCLUDES
# Iterate through the list of components and create the list of referenced includes
# This macro doesn't take arguments
define PROCESS_REFERENCED_INCLUDES
$(foreach component, $(BOB_COMPONENT_NAMES),\
	$(eval $(component)_REFERENCED_INCLUDES += $(foreach reference, $(sort $($(component)_REFERENCED_COMPONENTS) $($(component)_REQUIRED_COMPONENTS)),\
		$(if $($(reference)_TO_FULL_NAME),,$(error $(reference) doesn't have a full name mapping))\
		$(addprefix $($($($(reference)_TO_FULL_NAME)_NAME)_DIRECTORY),$($($($(reference)_TO_FULL_NAME)_NAME)_INCLUDES)))))
endef

###################################################################################################
# PROCESS_DECLARED_LISTS
# Iterate through the list of declared lists collating all the component entries into a single 
# variable for each list
# This macro doesn't take arguments
define PROCESS_DECLARED_LISTS
$(foreach list,$(BOB_DECLARED_LISTS),$(foreach component, $(BOB_COMPONENT_NAMES),$(if $($(component)_$(list)_LIST),\
     $(eval BOB_LIST_$(list) += $($(component)_$(list)_LIST)) \
     $(eval BOB_LIST_DEPENDENCIES_$(list) += $($(component)_MAKEFILE)))))
endef


###################################################################################################
# PROCESS_BOB_GLOBAL_VARIABLES
# Iterate through the list of components transferring their global variables into the BOB equivalents
# This macro doesn't take arguments
define PROCESS_BOB_GLOBAL_VARIABLES
# Add new component GLOBAL variables to BUILD GLOBAL variables
$(foreach component, $(BOB_COMPONENT_NAMES),\
	$(eval $(component)_GLOBAL_INCLUDES      := $(addprefix $($(component)_DIRECTORY),$($(component)_GLOBAL_INCLUDES)))\
	$(eval $(component)_GLOBAL_LINKER_SCRIPT := $(addprefix $($(component)_DIRECTORY),$($(component)_GLOBAL_LINKER_SCRIPT)))\
	$(foreach var,$(BOB_GLOBAL_VARIABLES),$(eval BOB_GLOBAL_$(var) +=$($(component)_GLOBAL_$(var))))\
	)
$(if $(BOB_GENERATED_COMPILE_FILES)$(BOB_GENERATED_LINK_FILES),\
	$(eval BOB_GLOBAL_INCLUDES += $(BOB_GENERATED_FILE_DIRECTORY))
)
endef

###################################################################################################
# PROCESS_COMMAND_VARIABLES
# Iterate through the list of components transferring the command variables into the BOB equivalents
# This macro doesn't take arguments
define PROCESS_COMMAND_VARIABLES
$(foreach command,$(SUPPORTED_COMMANDS),\
	$(eval BOB_$(command)_TOOLCHAIN_MAKEFILES :=\
		$(foreach component,$(BOB_COMPONENT_NAMES),\
			$(if $($(component)_$(command)_TOOLCHAIN_MAKEFILES),$(addprefix $($(component)_DIRECTORY),$($(component)_$(command)_TOOLCHAIN_MAKEFILES)))\
		)\
	)\
	$(eval BOB_$(command)_COMMAND_TARGETS :=\
		$(foreach component,$(BOB_COMPONENT_NAMES),\
			$(if $($(component)_$(command)_COMMAND_TARGETS),$($(component)_$(command)_COMMAND_TARGETS))\
		)\
	)\
	$(foreach target,$(BOB_$(command)_COMMAND_TARGETS),\
		$(foreach variable,$(COMMAND_TARGET_NAMED_VARIABLES),\
			$(eval BOB_$(target)_$(command)_TARGET_$(variable) :=\
				$(sort $(foreach component,$(BOB_COMPONENT_NAMES),$($(component)_$(target)_$(command)_TARGET_$(variable))))\
			)\
		)\
	)\
)
endef

