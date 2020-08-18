NAME := Clang_toolchain

ifeq ($(filter windows osx,$(HOST_OS)),)
$(error $(HOST_OS) not supported)
endif

$(NAME)_SUPPORTED_FEATURES := BOB_TOOLCHAIN
                              
$(NAME)_PROVIDED_FEATURES := clang_compiler


$(NAME)_GLOBAL_CFLAGS   := $$($(BUILD_TYPE)_OPTIMIZATION_FLAG) 
$(NAME)_GLOBAL_CPPFLAGS := $$($(BUILD_TYPE)_OPTIMIZATION_FLAG) -std=c++17
$(NAME)_GLOBAL_LDFLAGS  := $$($(BUILD_TYPE)_OPTIMIZATION_FLAG) 


ifeq (windows,$(HOST_OS))
$(NAME)_PROVIDED_FEATURES += windows
include $(CURDIR)/../msvc/msvc_toolchain_locations.mk

#$(NAME)_GLOBAL_DEFINES += _HAVE_STDC
$(NAME)_GLOBAL_INCLUDES := .

#export LIB=C:\Program Files (x86)\Microsoft Visual Studio\2017\Community\VC\Tools\MSVC\14.10.25017\lib\x64;C:\Program Files (x86)\Windows Kits\10\lib\10.0.10240.0\ucrt\x64;C:\Program Files (x86)\Windows Kits\8.1\lib\winv6.3\um\x64;
#$(NAME)_GLOBAL_LDFLAGS += -lShlwapi

$(NAME)_GLOBAL_LDFLAGS  := $(DEBUG_LINK_FLAGS)
$(NAME)_GLOBAL_LDFLAGS  += $(subst \ , ,-LIBPATH:"$(WINDOWS_SDK_DIR)/Lib/$(WINDOWS_SDK_VERSION)/um/x64" -LIBPATH:"$(VISUAL_STUDIO_TOOLS)/lib/onecore/x64" -LIBPATH:"$(WINDOWS_SDK_DIR)/Lib/$(WINDOWS_SDK_VERSION)/ucrt/x64")

else
ifeq (osx,$(HOST_OS))

#$(NAME)_GLOBAL_INCLUDES += osx \
                           osx/include \
                           osx/include/c++/v1 \
                           osx/lib/clang/6.0.0/include
endif
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

$(NAME)_compile_TOOLCHAIN_MAKEFILES := clang_toolchain.mk
$(NAME)_link_TOOLCHAIN_MAKEFILES    := clang_toolchain.mk
