COMPONENT_NAMED_VARIABLES := SOURCES \
                             DIRECTORY \
                             MAKEFILE \
                             INCLUDES \
                             PRIVATE_INCLUDES \
                             REFERENCED_INCLUDES \
                             DEFINES \
                             SUPPORTED_FEATURES \
                             REQUIRED_FEATURES \
                             PROVIDED_FEATURES \
                             GENERATED_COMPILE_FILES \
                             GENERATED_LINK_FILES \
                             GENERATED_SOURCES \
                             ADDITIONAL_MAKEFILES \
                             DECLARED_LISTS \
                             FORCE_KEEP_SYMBOLS \
                             REQUIRED_COMPONENTS \
                             REFERENCED_COMPONENTS \
                             EXCLUDED_COMPONENTS \
                             CFLAGS \
                             CXXFLAGS \
                             CPPFLAGS \
                             ASMFLAGS \
                             NIMFLAGS \
                             LIBRARIES \
                             GLOBAL_LDFLAGS \
                             GLOBAL_ARFLAGS \
                             GLOBAL_LINKER_SCRIPT \
                             SUPPORTED_GROUPS \
                             REQUIRED_GROUPS \
                             OPTIONS

BOB_NAMED_VARIABLES := PROCESSED_COMPONENTS \
                       REQUIRED_COMPONENTS \
                       COMPONENT_NAMES \
                       GLOBAL_DEFINES \
                       GLOBAL_INCLUDES \
                       GLOBAL_CFLAGS \
                       GLOBAL_CXXFLAGS \
                       GLOBAL_CPPFLAGS \
                       GLOBAL_ASMFLAGS \
                       GLOBAL_LDFLAGS \
                       GLOBAL_ARFLAGS \
                       GLOBAL_NIMFLAGS \
                       GLOBAL_LINKER_SCRIPT \
                       SUPPORTED_FEATURES \
                       REQUIRED_FEATURES \
                       PROVIDED_FEATURES \
                       TOOLCHAIN_COMPONENTS \
                       GENERATED_BINARIES \
                       REFERENCED_COMPONENTS \
                       EXCLUDED_COMPONENTS \
                       DECLARED_LISTS \
                       GENERATED_COMPILE_FILES \
                       GENERATED_LINK_FILES \
                       ADDITIONAL_MAKEFILES
                         
COMPONENT_GROUP_NAMED_VARIABLES := SOURCES \
                                   INCLUDES \
                                   PRIVATE_INCLUDES \
                                   GENERATED_SOURCES \
                                   DEFINES \
                                   CFLAGS \
                                   CXXFLAGS \
                                   CPPFLAGS \
                                   ASMFLAGS \
                                   NIMFLAGS
                                   
BOB_GLOBAL_VARIABLES := INCLUDES \
                        DEFINES \
                        CFLAGS \
                        CXXFLAGS \
                        CPPFLAGS \
                        ASMFLAGS \
                        LDFLAGS \
                        ARFLAGS \
                        NIMFLAGS \
                        LINKER_SCRIPT

                          
BOB_FEATURE_VARIABLES := SUPPORTED_FEATURES \
                         REQUIRED_FEATURES \
                         PREFERRED_FEATURES \
                         PROVIDED_FEATURES

FEATURE_NAMED_VARIABLES := SOURCES \
                           MAKEFILE \
                           INCLUDES \
                           PRIVATE_INCLUDES \
                           DEFINES \
                           CFLAGS \
                           CXXFLAGS \
                           CPPFLAGS \
                           ASMFLAGS \
                           LIBRARIES \
                           GENERATED_COMPILE_FILES \
                           GENERATED_LINK_FILES \
                           GENERATED_SOURCES \
                           ADDITIONAL_MAKEFILES \
                           DECLARED_LISTS \
                           FORCE_KEEP_SYMBOLS \
                           REQUIRED_COMPONENTS \
                           REFERENCED_COMPONENTS \
                           EXCLUDED_COMPONENTS \
                           REQUIRED_FEATURES \
                           SUPPORTED_FEATURES \
                           PROVIDED_FEATURES \
                           REQUIRED_GROUPS \
                           OPTIONS \
                           GLOBAL_DEFINES \
                           GLOBAL_INCLUDES \
                           GLOBAL_CFLAGS \
                           GLOBAL_CXXFLAGS \
                           GLOBAL_CPPFLAGS \
                           GLOBAL_ASMFLAGS \
                           GLOBAL_LDFLAGS \
                           GLOBAL_ARFLAGS \
                           GLOBAL_LINKER_SCRIPT

COMMAND_TARGET_NAMED_VARIABLES := CFLAGS \
                                  CXXFLAGS \
                                  CPPFLAGS \
                                  ASMFLAGS \
                                  LDFLAGS \
                                  GLOBAL_LINKER_SCRIPT

# Unused but is here for documentation
OPTION_VARIABLES := OPTION_DEFAULT \
                    OPTION_FEATURES
