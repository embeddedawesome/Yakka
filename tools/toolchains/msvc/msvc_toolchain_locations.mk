# Standard directories on Windows
WINDOWS_SDK_DIR   := C:/Program\ Files\ (x86)/Windows\ Kits/10
VISUAL_STUDIO_DIR := C:/Program\ Files\ (x86)/Microsoft\ Visual\ Studio/2019
VISUAL_STUDIO_TYPE := $(notdir $(lastword $(wildcard $(VISUAL_STUDIO_DIR)/* )))
VISUAL_STUDIO_TOOL_DIR := $(VISUAL_STUDIO_DIR)/$(VISUAL_STUDIO_TYPE)/VC/Tools/MSVC


# Determine latest version of Windows SDK by list directories in the Include path and getting the first one (should be the one with the biggest number)
WINDOWS_SDK_VERSION         :=$(firstword $(filter 10.%,$(subst /, ,$(wildcard $(WINDOWS_SDK_DIR)/Include/* ))))
# Determine latest version of Visual Studio tools
VISUAL_STUDIO_TOOLS_VERSION := $(notdir $(lastword $(wildcard $(VISUAL_STUDIO_TOOL_DIR)/*)))

# Verify we have some kind of value for the versions
ifeq ($(WINDOWS_SDK_VERSION),)
$(error Couldn't find version of Windows SDK. Confirm Visual Studio Build Tools are installed)
endif
ifeq ($(VISUAL_STUDIO_TOOLS_VERSION),)
$(error Couldn't find version of Visual Studio Build Tools. Confirm Visual Studio Build Tools are installed)
endif

VISUAL_STUDIO_TOOLS     := $(VISUAL_STUDIO_TOOL_DIR)/$(VISUAL_STUDIO_TOOLS_VERSION)
WINDOWS_SDK_INCLUDE_DIR := "$(WINDOWS_SDK_DIR)/Include/$(WINDOWS_SDK_VERSION)"
