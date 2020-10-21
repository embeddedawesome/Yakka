NAME := xcode_toolchain

ifeq ($(filter macos,$(HOST_OS)),)
$(error $(HOST_OS) not supported)
endif

$(NAME)_SUPPORTED_FEATURES := BOB_TOOLCHAIN
                              
$(NAME)_PROVIDED_FEATURES := clang_compiler osx

$(NAME)_GLOBAL_CFLAGS   :=
$(NAME)_GLOBAL_CPPFLAGS := -std=c++17 -g

debug_OPTIMIZATION_FLAG   := -Og
release_OPTIMIZATION_FLAG := -O3
COMMON_LINKER_FLAGS  := $($(BUILD_TYPE)_OPTIMIZATION_FLAG)
debug_LINKER_FLAGS   := $(COMMON_LINKER_FLAGS) 
release_LINKER_FLAGS := $(COMMON_LINKER_FLAGS) 

#-Wc++11-extensions

# $(NAME)_GLOBAL_INCLUDES += .


#ANALYSIS_CFLAGS := -B$($(NAME)_DIRECTORY)/$(HOST_OS) -O0 

# Additional CFLAGS to avoid warnings in libc and other libraries
#ANALYSIS_CFLAGS += -D__STD_C \
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

$(NAME)_compile_TOOLCHAIN_MAKEFILES := xcode_toolchain.mk
$(NAME)_link_TOOLCHAIN_MAKEFILES    := xcode_toolchain.mk
