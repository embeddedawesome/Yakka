#ifdef BOB_BASE_MAKEFILE

librarify: $(BOB_PROJECT_SUMMARY_FILE) $(BOB_OUTPUT_DIRECTORY)/compile_summary.mk $(BOB_OUTPUT_DIRECTORY)/librarify_summary.mk
#	$(QUIET)$(MAKE) $(SILENCE_MAKE) -f $(BOB_SDK_DIRECTORY)tools/makefiles/commands/librarify.mk -j 8 PROJECT=$(PROJECT)

BOB_DECLARED_LISTS += library_variant

#$(info $(BOB_DECLARED_LISTS))
#else

# Load project summary
include $(BOB_PROJECT_SUMMARY_FILE)

#.PHONY: $(BOB_OUTPUT_DIRECTORY)/librarify_summary.mk

#all: $(BOB_OUTPUT_DIRECTORY)/librarify_summary.mk
	#$(ECHO) Building library variants

# Create a project name
# $(1) is the list of components
define CREATE_PROJECT_NAME
$(foreach word,$(COMPONENT_LIST),$(word)-)project
endef

SPACE := 
SPACE +=

define PACK_BOB_STRING
$(subst $(SPACE),*,$(strip $1))
endef

define UNPACK_BOB_STRING
$(subst *, ,$(strip $1))
endef

# Generate the list of variants
ifeq ($(BOB_LIST_library_variant),)
LIBRARIFY_VARIANTS := $(call PACK_BOB_STRING,$(BOB_PROCESSED_COMPONENTS))
else
LIBRARIFY_VARIANTS := $(foreach variant,$(BOB_LIST_library_variant),$(eval $(call PACK_BOB_STRING,$(BOB_PROCESSED_COMPONENTS) $(variant))))
endif


# $1 is the name of the variant project
define CREATE_LIBRARY_BOB_RULE
$(info Defining rule for: $(1))
.PHONY: $(1)
$(1):
	$(ECHO) Building: $(1)
endef
#	$(MAKE) $(call UNPACK_BOB_STRING, $(1)) compile
#	$(ECHO) $(CP) $(BOB_OUTPUT_BASE_DIRECTORY)/$(_TEMP)/libraries/*.a $(BOB_OUTPUT_DIRECTORY)
#	$(eval $(call CREATE_PROJECT_NAME,$(BOB_PROCESSED_COMPONENTS) $(1)))

# Generate rules for every variant
#$(error $(LIBRARIFY_VARIANTS))
$(foreach variant,$(LIBRARIFY_VARIANTS),$(eval $(call CREATE_LIBRARY_BOB_RULE,$(variant))))

$(BOB_OUTPUT_DIRECTORY)/librarify_summary.mk: $(BOB_PROJECT_SUMMARY_FILE) $(LIBRARIFY_VARIANTS) $(BOB_SDK_DIRECTORY)tools/makefiles/commands/librarify.mk
	$(call CREATE_FILE,$@,#List of library variants)
	$(foreach variant,$(LIBRARIFY_VARIANTS),$(call APPEND_TO_FILE,$@,$(variant)))

