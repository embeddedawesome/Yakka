NAME := GCC_toolchain

ifeq ($(filter windows osx linux64,$(HOST_OS)),)
$(error $(HOST_OS) not supported)
else
$(NAME)_PROVIDED_FEATURES += $(HOST_OS)
endif

$(NAME)_SUPPORTED_FEATURES := BOB_TOOLCHAIN
                              
$(NAME)_PROVIDED_FEATURES += gcc_compiler

debug_OPTIMIZATION_FLAG   := -Og
release_OPTIMIZATION_FLAG := -O3
COMMON_LINKER_FLAGS  := $($(BUILD_TYPE)_OPTIMIZATION_FLAG)
debug_LINKER_FLAGS   := $(COMMON_LINKER_FLAGS) 
release_LINKER_FLAGS := $(COMMON_LINKER_FLAGS) 

$(NAME)_GLOBAL_CFLAGS   := $$($(BUILD_TYPE)_OPTIMIZATION_FLAG) 
$(NAME)_GLOBAL_CPPFLAGS := $$($(BUILD_TYPE)_OPTIMIZATION_FLAG) -std=c++17 -std=gnu++0x
$(NAME)_GLOBAL_LDFLAGS  := $$($(BUILD_TYPE)_OPTIMIZATION_FLAG) 


ifeq (windows,$(HOST_OS))
#$(NAME)_PROVIDED_FEATURES += windows
include $(CURDIR)/../msvc/msvc_toolchain_locations.mk

#$(NAME)_GLOBAL_DEFINES += _HAVE_STDC
$(NAME)_GLOBAL_INCLUDES := .

#export LIB=C:\Program Files (x86)\Microsoft Visual Studio\2017\Community\VC\Tools\MSVC\14.10.25017\lib\x64;C:\Program Files (x86)\Windows Kits\10\lib\10.0.10240.0\ucrt\x64;C:\Program Files (x86)\Windows Kits\8.1\lib\winv6.3\um\x64;
#$(NAME)_GLOBAL_LDFLAGS += -lShlwapi

$(NAME)_GLOBAL_LDFLAGS  := $(DEBUG_LINK_FLAGS)
#$(NAME)_GLOBAL_LDFLAGS  += $(subst \ , ,-LIBPATH:"$(WINDOWS_SDK_DIR)/Lib/$(WINDOWS_SDK_VERSION)/um/x64" -LIBPATH:"$(VISUAL_STUDIO_TOOLS)/lib/onecore/x64" -LIBPATH:"$(WINDOWS_SDK_DIR)/Lib/$(WINDOWS_SDK_VERSION)/ucrt/x64")
endif 
ifeq (osx,$(HOST_OS))

#$(NAME)_GLOBAL_INCLUDES += osx \
                           osx/include \
                           osx/include/c++/v1 \
                           osx/lib/clang/6.0.0/include
endif
ifeq (linux64,$(HOST_OS))
$(NAME)_GLOBAL_LDFLAGS += -lpthread
endif


ANALYSIS_CFLAGS := -B$($(NAME)_DIRECTORY)/$(HOST_OS) -O0 

# Additional CFLAGS to avoid warnings in libc and other libraries
ANALYSIS_CFLAGS += -D__STD_C \
                   -D__STDC__ \
                   -Wno-incompatible-library-redeclaration \
                   -Wno-implicit-function-declaration \
                   -Wno-int-to-pointer-cast \
                   -Wno-parentheses-equality \
                   -Wno-switch \
                   -Wno-pointer-sign \
                   -Wno-unknown-attributes \
                   -Wno-macro-redefined \
                   -Wno-unknown-pragmas

$(NAME)_compile_TOOLCHAIN_MAKEFILES := gcc_toolchain.mk
$(NAME)_link_TOOLCHAIN_MAKEFILES    := gcc_toolchain.mk
