# Find locations of the toolchain
include $(Visual_Studio_compiler_DIRECTORY)/msvc_toolchain_locations.mk

TOOLCHAIN_LOCATION :="${subst (,\(,${subst ),\),$(VISUAL_STUDIO_TOOLS)/bin/Hostx64/x64}}"

c_COMPILER   := "$(TOOLCHAIN_LOCATION)/cl.exe"
cpp_COMPILER := "$(TOOLCHAIN_LOCATION)/cl.exe" 
s_COMPILER   := $(c_COMPILER)
ARCHIVER     := "$(TOOLCHAIN_LOCATION)/lib.exe"
LINKER       := "$(TOOLCHAIN_LOCATION)/link.exe"

#$(error $(TOOLCHAIN_LOCATION): $(wildcard $(TOOLCHAIN_LOCATION)/*))

TOOLCHAIN_LINKER_PATH_INDICATOR      :=-LIBPATH:
TOOLCHAIN_CREATE_ARCHIVE_INDICATOR   :=-OUT:
TOOLCHAIN_OPTION_FILE_INDICATOR      :=@
TOOLCHAIN_COMPILE_OUTPUT_FLAG        :=-Fo:
TOOLCHAIN_BINARY_OUTPUT              := $(BOB_OUTPUT_BINARY_DIRECTORY)/$(BOB_PROJECT)$(HOST_EXECUTABLE_SUFFIX)
TOOLCHAIN_OBJECTS_ONLY               := true

# Rule to create the exe file
$(BOB_OUTPUT_BINARY_DIRECTORY)/$(BOB_PROJECT)$(HOST_EXECUTABLE_SUFFIX): $(BOB_OUTPUT_DIRECTORY)/build_summary.mk $(BOB_GENERATED_OBJECTS) | $(BOB_OUTPUT_BINARY_DIRECTORY)
	$(QUIET)$(ECHO) Making executable $@
	$(LINKER) -MAP:$(BOB_OUTPUT_BINARY_DIRECTORY)/$(BOB_PROJECT).map -PDB:"$(BOB_OUTPUT_BINARY_DIRECTORY)/$(BOB_PROJECT).pdb" $(subst /,\\,$(BOB_GENERATED_OBJECTS) $(foreach c,$(BOB_COMPONENT_NAMES),$(addprefix $($(c)_DIRECTORY),$($(c)_LIBRARIES)))) Shlwapi.lib $(BOB_GLOBAL_LDFLAGS) -OUT:$(subst /,\\,$@)  
	
