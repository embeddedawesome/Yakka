# Check the version of Make
$(if $(filter 4,$(firstword $(subst ., ,$(MAKE_VERSION)))),,$(error You're doing it wrong. Must run make version 4.0+. This is version $(MAKE_VERSION)))

HOST_EXECUTABLE_SUFFIX  := .exe
TOOLS_DIRECTORY :=tools/windows/

SLASH_QUOTE_START :="\"
SLASH_QUOTE_END   :=\""

ESC_QUOTE:="
ESC_SPACE:=$(SPACE)

COMMA             :=,
ECHO              :=$(TOOLS_DIRECTORY)echo
CP                :=$(TOOLS_DIRECTORY)cp
TOUCH             :=$(TOOLS_DIRECTORY)touch
QUIET             :=@
RM                :=$(TOOLS_DIRECTORY)rm
SILENCE_MAKE      :=-s
CAT               :=$(TOOLS_DIRECTORY)cat
GIT               :=$(TOOLS_DIRECTORY)git

DEV_NULL          :=NUL

# Helper util to clean up a directory. $(1) is the directory or even component makefile
define CLEAN_DIRECTORY
$(subst $(BOB_SDK_PATH)/,,$(realpath $(dir $(1))))
endef

# $(1) is the directory name
define MKDIR
$(TOOLS_DIRECTORY)mkdir -p $(1)
endef

CREATE_FILE     =$(file >$(1),$(2))
APPEND_TO_FILE  =$(file >>$(1),$(2))

export SHELL =$(TOOLS_DIRECTORY)dash
