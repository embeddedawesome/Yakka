
# Appends the file related info to the provided file name
# $(1) is the source file with path, $(2) is the output file, $(3) is the component
define LOG_FILE_COMPILE_INFO
$(call APPEND_TO_FILE,$(2),{)
$(call APPEND_TO_FILE,$(2),"directory" : "$($(3)_DIRECTORY)"$(COMMA))
$(call APPEND_TO_FILE,$(2),"command" : "clang @$(BOB_OUTPUT_DIRECTORY)/$(PROJECT)/analyze/$(3).c_analysis_option -o $(BOB_OUTPUT_DIRECTORY)/$(PROJECT)/analyze/$(1).out $(1)"$(COMMA))
$(call APPEND_TO_FILE,$(2),"file" : "$(1)"$(COMMA))
$(call APPEND_TO_FILE,$(2),}$(COMMA))
endef

# Appends the component related info to the provided file name
# $(1) is the component name, $(2) is the output file
define LOG_COMPONENT_COMPILE_INFO
$(foreach file,$($(1)_SOURCES),$(call LOG_FILE_COMPILE_INFO,$(file),$(2),$(1)))
endef

# build_summary rule
$(BOB_OUTPUT_DIRECTORY)/compile_database.json: Makefile tools/makefiles/compile_database.mk $(foreach component,$(BOB_COMPONENT_NAMES),$($(component)_MAKEFILE)) | $(BOB_OUTPUT_DIRECTORY)
	$(call CREATE_FILE,$@)
	$(call APPEND_TO_FILE,$@,[)
	$(foreach component,$(BOB_COMPONENT_NAMES), $(call LOG_COMPONENT_COMPILE_INFO,$(component),$@))
	$(call APPEND_TO_FILE,$@,])
