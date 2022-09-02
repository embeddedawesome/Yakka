
HOST_EXECUTABLE_SUFFIX  :=

SLASH_QUOTE_START :=\"
SLASH_QUOTE_END   :=\"

ESC_QUOTE         :=\"
ESC_SPACE         :=\$(SPACE)

CAT               := cat
ECHO              := echo
QUOTES_FOR_ECHO   :=
TOUCH             := touch
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

CREATE_FILE     =$(file >$(1),$(2))
APPEND_TO_FILE  =$(file >>$(1),$(2))

MAKE := make -R