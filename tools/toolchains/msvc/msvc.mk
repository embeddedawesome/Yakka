NAME := Visual_Studio_compiler

ifneq ($(filter windows,$(HOST_OS)),)
$(NAME)_REQUIRED_FEATURES += windows
endif

include $(CURDIR)msvc_toolchain_locations.mk

# Tell BOB that this component is a toolchain
$(NAME)_SUPPORTED_FEATURES := BOB_TOOLCHAIN

# Add other supported features
$(NAME)_SUPPORTED_FEATURES += networking \
                              USB
                              
$(NAME)_PROVIDED_FEATURES := msvc_compiler

#$(NAME)_networking_SOURCES        := networking_support.cpp
$(NAME)_networking_GLOBAL_LDFLAGS := -lws2_32
$(NAME)_USB_GLOBAL_LDFLAGS        := -lsetupapi


#$(NAME)_GLOBAL_DEFINES := INCLUDE_NEXT_NOT_SUPPORTED
DEBUG_LINK_FLAGS := -DEBUG:FULL -OPT:NOICF

$(NAME)_GLOBAL_CFLAGS   := -Fm -c -Zi -FS -MT
$(NAME)_GLOBAL_CPPFLAGS := -Fm -c -EHsc -O2 -Zi -FS -MT /std:c++20 -DEBUG:FULL /analyze
$(NAME)_GLOBAL_LDFLAGS  := $(DEBUG_LINK_FLAGS) -SUBSYSTEM:CONSOLE -MACHINE:x64 -OPT:REF -DEBUG:FULL
# -PDBSTRIPPED

$(NAME)_compile_TOOLCHAIN_MAKEFILES := msvc_toolchain.mk
$(NAME)_link_TOOLCHAIN_MAKEFILES    := msvc_toolchain.mk

$(NAME)_GLOBAL_CPPFLAGS += $(subst \ , ,-I"$(VISUAL_STUDIO_TOOLS)/include" -I$(WINDOWS_SDK_INCLUDE_DIR)/um -I$(WINDOWS_SDK_INCLUDE_DIR)/shared -I$(WINDOWS_SDK_INCLUDE_DIR)/ucrt)
$(NAME)_GLOBAL_LDFLAGS += $(subst \ , ,-LIBPATH:"$(WINDOWS_SDK_DIR)/Lib/$(WINDOWS_SDK_VERSION)/um/x64" -LIBPATH:"$(VISUAL_STUDIO_TOOLS)/lib/onecore/x64" -LIBPATH:"$(WINDOWS_SDK_DIR)/Lib/$(WINDOWS_SDK_VERSION)/ucrt/x64")
#-L"C:\\Program Files (x86)\\Microsoft Visual Studio 14.0\VC\lib;C:\Program Files (x86)\Windows Kits\8.1\Lib\winv6.3\um\x86;C:\Program Files (x86)\Windows Kits\10\Lib\10.0.10240.0\ucrt\x86
