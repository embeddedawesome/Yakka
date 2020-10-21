ifeq ($(filter macos,$(HOST_OS)),)
$(error $(HOST_OS) not supported)
endif

ifeq ($(link_TARGET),)
LINKED_BINARY_OUTPUT_DIRECTORY := $(BOB_OUTPUT_BINARY_DIRECTORY)
else
LINKED_BINARY_OUTPUT_DIRECTORY := $(BOB_OUTPUT_BINARY_DIRECTORY)/$(link_TARGET)
endif

include $(BOB_OUTPUT_DIRECTORY)/compile_summary.mk

c_COMPILER   := clang
cpp_COMPILER := clang++
s_COMPILER   := $(c_COMPILER)
ARCHIVER     := ar

# Internally used executables
OBJCOPY   := objcopy
OBJDUMP   := objdump
STRIP_ELF := strip
LINKER    := clang++

# Internally used variables
debug_OPTIMIZATION_FLAG   := -Og
release_OPTIMIZATION_FLAG := -O3
COMMON_LINKER_FLAGS  :=
debug_LINKER_FLAGS   := $(COMMON_LINKER_FLAGS) 
release_LINKER_FLAGS := $(COMMON_LINKER_FLAGS) $($(BUILD_TYPE)_OPTIMIZATION_FLAG)
LINKER_SHARED_LIBRARY_OPTION :=-shared
LINKER_PARTIAL_LINK_OPTION   :=-Ur

# Toolchain specific variables
TOOLCHAIN_COMPILE_OUTPUT_FLAG        :=-o 
TOOLCHAIN_OPTION_FILE_INDICATOR      :=@
TOOLCHAIN_NO_LINKING_INDICATOR       :=-c
TOOLCHAIN_CREATE_ARCHIVE_INDICATOR   :=rs
TOOLCHAIN_BINARY_OUTPUT              := $(BOB_OUTPUT_BINARY_DIRECTORY)/$(BOB_PROJECT)$(HOST_EXECUTABLE_SUFFIX)
TOOLCHAIN_OBJECTS_ONLY :=true

# Rule to create the elf file
$(BOB_OUTPUT_BINARY_DIRECTORY)/$(BOB_PROJECT)$(HOST_EXECUTABLE_SUFFIX): $(BOB_OUTPUT_DIRECTORY)/build_summary.mk $(BOB_GLOBAL_LINKER_SCRIPT) $(BOB_GENERATED_LIBRARIES) | $(BOB_OUTPUT_BINARY_DIRECTORY)
	$(ECHO) Making executable file
	$(LINKER) $(BOB_GLOBAL_LDFLAGS) -o $@ $(BOB_GENERATED_OBJECTS)
	
$(BOB_OUTPUT_BINARY_DIRECTORY)/$(BOB_PROJECT).fuzz: 
	$(c_COMPILER) -fsanitize=address -fsanitize-coverage=trace-pc-guard
	