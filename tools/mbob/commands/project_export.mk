# Define link build rule if this is included from the base Makefile
ifdef BOB_BASE_MAKEFILE
project_export: build_summary analyze
	$(MAKE) -f $(BOB_SDK_DIRECTORY)tools/makefiles/commands/project_export.mk PROJECT=$(PROJECT)

else
include configuration.mk
include $(BOB_SDK_DIRECTORY)tools/makefiles/hosts/identify_host_os.mk
include $(BOB_OUTPUT_DIRECTORY)/build_summary.mk

PROJECT_OUTPUT_DIRECTORY := $(BOB_OUTPUT_DIRECTORY)/project

SOURCE_FILES := $(foreach component,$(BOB_COMPONENT_NAMES),$(foreach file,$($(component)_SOURCES),$(addprefix $($(component)_DIRECTORY),$(file))))

HEADER_FILES := $(shell $(PERL) $(BOB_SDK_DIRECTORY)tools/analysis/analysis_to_header_list.pl $(BOB_OUTPUT_DIRECTORY)/analysis_summary.json)

all: $(PROJECT_OUTPUT_DIRECTORY)/Makefile $(addprefix $(PROJECT_OUTPUT_DIRECTORY),$(SOURCE_FILES))  $(addprefix $(PROJECT_OUTPUT_DIRECTORY),$(HEADER_FILES))


$(PROJECT_OUTPUT_DIRECTORY)/Makefile: $(BOB_OUTPUT_DIRECTORY)/build_summary.mk $(BOB_SDK_DIRECTORY)tools/makefiles/commands/project_export.mk | $(PROJECT_OUTPUT_DIRECTORY)/.d
	$(file >$@,SOURCE_FILES := \)
	$(foreach file,$(SOURCE_FILES), $(call APPEND_TO_FILE,$@,   $(file) \))
	$(call APPEND_TOF_FILE,$@,all: )
	$(call APPEND_TOF_FILE,$@,	)

$(BOB_OUTPUT_DIRECTORY)/%.c: %.c
	$(CP) $^ $@

$(BOB_OUTPUT_DIRECTORY)/%.h: %.h
	$(CP) $^ $@

###################################################################################################
# Directory creation rules
%/.d:
	$(QUIET)$(call MKDIR,$(dir $@))

.PHONY: $(PROJECT) $(PROJECT_OUTPUT_DIRECTORY)/Makefile
$(PROJECT): $(PROJECT_OUTPUT_DIRECTORY)/Makefile | $(PROJECT_OUTPUT_DIRECTORY)/.d
	$(ECHO) Done
endif

