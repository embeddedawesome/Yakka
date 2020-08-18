# Copyright

JOBS ?= 8

include $(BOB_SDK_DIRECTORY)configuration.mk
BOB_MAKEFILE_DIRECTORY := $(BOB_SDK_DIRECTORY)/tools/mbob

# Include some basic makefile tools 
include $(BOB_MAKEFILE_DIRECTORY)/hosts/identify_host_os.mk
include $(BOB_MAKEFILE_DIRECTORY)/build_system_tools.mk

# Set the default BUILD_TYPE to release
BUILD_TYPE ?= release

# Find the realpath of the SDK
BOB_SDK_PATH := $(realpath $(BOB_SDK_DIRECTORY))

# Get the list of supported commands
SUPPORTED_COMMANDS := $(subst .mk,,$(notdir $(wildcard $(BOB_MAKEFILE_DIRECTORY)/commands/*.mk)))

# Reserved command line names
RESERVED_COMMAND_LINE_TARGETS := $(SUPPORTED_COMMANDS)

# Find the list of feature
FEATURE_CMDGOALS := $(filter $(BOB_FEATURE_IDENTIFIER)%,$(MAKECMDGOALS))

# Find the list of commands
COMMAND_CMDGOALS := $(filter %$(BOB_COMMAND_IDENTIFIER),$(MAKECMDGOALS))

# Find the list of components (everything not a feature or command)
COMPONENT_LIST := $(filter-out $(FEATURE_CMDGOALS) $(COMMAND_CMDGOALS), $(MAKECMDGOALS))

# Separate the command name from the command target and remove the command identifier
COMMAND_LIST   := $(foreach element,$(subst $(BOB_COMMAND_IDENTIFIER),,$(COMMAND_CMDGOALS)),\
    	             $(eval TEMP := $(subst $(BOB_COMMAND_TARGET_SEPARATOR), ,$(element)))\
    	             $(if $(word 2,$(TEMP)),$(eval $(word 1,$(TEMP))_TARGET := $(word 2,$(TEMP))))\
    	             $(word 1,$(TEMP))\
                   )

# Create a list of features without the feature identifier
FEATURE_LIST   := $(subst $(BOB_FEATURE_IDENTIFIER),,$(FEATURE_CMDGOALS))

# Generate a default BOB_PROJECT name if none is provided
# To provide a project name from the command line, add BOB_PROJECT=<name>
ifeq ($(BOB_PROJECT),)
$(foreach word,$(COMPONENT_LIST),$(eval BOB_PROJECT:=$(BOB_PROJECT)$(word)-))
$(if $(BOB_PROJECT),$(eval BOB_PROJECT :=$(BOB_PROJECT)project))
$(foreach word,$(FEATURE_LIST),$(eval BOB_PROJECT:=$(BOB_PROJECT)+$(word)))
endif

BOB_OUTPUT_DIRECTORY     := $(BOB_OUTPUT_BASE_DIRECTORY)/$(BOB_PROJECT)
BOB_PROJECT_SUMMARY_FILE := $(BOB_OUTPUT_DIRECTORY)/build_summary.mk

# Define the 'all' target to prevent make from automatically building the first target it sees
all: $(BOB_PROJECT) $(BOB_COMMAND)

$(BOB_PROJECT):: $(BOB_PROJECT_SUMMARY_FILE) $(BOB_COMMAND) $(COMMAND_LIST)
	@:

$(MAKECMDGOALS):: $(BOB_PROJECT) $(COMMAND_LIST)
	@:

# Check if there is no BOB_COMMAND passed from the command line
ifeq ($(BOB_COMMAND),)
ifeq ($(filter clean,$(COMMAND_LIST)),)
BOB_COMMAND := summarize
else
BOB_COMMAND := clean
endif
else
# Check if the project summary file already exists, if so include it and define the file dependencies
ifneq ($(wildcard $(BOB_PROJECT_SUMMARY_FILE)),)
include $(BOB_PROJECT_SUMMARY_FILE)
endif
endif

###################################################################################################
# GENERATE_COMMAND_MAKE_RULE
# Generates a make rule for a provided command
# $(1) is the name of the command
define GENERATE_COMMAND_MAKE_RULE
$(1)::
	$(QUIET)$(ECHO) Starting '$(1)' command
	$(QUIET)$(MAKE) $(SILENCE_MAKE) -j BOB_COMMAND=$(1) BOB_PROJECT=$(BOB_PROJECT) BOB_SDK_DIRECTORY=$(BOB_SDK_DIRECTORY) $(foreach c,$(SUPPORTED_COMMANDS),$(if $($(c)_FLAGS),$(c)_FLAGS=$($(c)_FLAGS)) $(if $($(c)_TARGET),$(c)_TARGET=$($(c)_TARGET)))
endef

$(foreach command,$(SUPPORTED_COMMANDS),\
	$(if $(filter $(command),$(BOB_COMMAND)),\
		$(eval include $(BOB_MAKEFILE_DIRECTORY)/commands/$(command).mk),\
		$(eval $(call GENERATE_COMMAND_MAKE_RULE,$(command)))\
	)\
)

# Default target to catch all non-command targets
.DEFAULT:
	@:

