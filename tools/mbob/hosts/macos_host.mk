# Check the version of Make
$(if $(filter 4,$(firstword $(subst ., ,$(MAKE_VERSION)))),,$(error You're doing it wrong, run ./make or have a local copy of make version 4.0+. This is version $(MAKE_VERSION)))

HOST_EXECUTABLE_SUFFIX  :=

SLASH_QUOTE_START :=\"
SLASH_QUOTE_END   :=\"

ESC_QUOTE         :=\"
ESC_SPACE         :=\$(SPACE)

CP                :=cp
RM                :=rm
CAT               :=cat
ECHO              :=echo
TOUCH             :=touch
PERL              :=perl
PYTHON            :=python
SILENCE_MAKE      :=-s
DEV_NULL          := /dev/null
QUIET             :=@

# Helper util to clean up a directory. $(1) is the directory or even component makefile
define CLEAN_DIRECTORY
$(subst $(BOB_SDK_PATH)/,,$(realpath $(dir $(1))))
endef

# $(1) is the directory name
define MKDIR
mkdir -p $(1)

endef

# $(1) is the content, $(2) is the file to print to.
define PRINT
@$(ECHO) '$(1)'>>$(2)

endef

CREATE_FILE     =$(shell $(ECHO) '$(2)' > $(1))
APPEND_TO_FILE  =$(shell $(ECHO) '$(2)' >> $(1))

MAKE := tools/common/osx/remake
