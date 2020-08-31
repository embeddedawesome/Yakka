NAME := BOB

$(NAME)_SOURCES := bob.cpp \
                   bob_project.cpp \
                   bob_component.cpp \
                   component_database.cpp \
                   uc_component.cpp

$(NAME)_SUPPORTED_FEATURES := windows \
                              osx

$(NAME)_windows_SOURCES := windows.cpp
$(NAME)_osx_SOURCES   := posix.cpp

$(NAME)_REQUIRED_COMPONENTS := yaml-cpp \
                               cpp-subprocess \
                               inja \
                               json \
                               CTPL \
                               semver

