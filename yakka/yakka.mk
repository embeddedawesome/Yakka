NAME := BOB

$(NAME)_SOURCES := bob_cli.cpp \
                   bob_project.cpp \
                   bob_component.cpp \
                   bob_workspace.cpp \
                   component_database.cpp \
                   bob_blueprint.cpp \
                   utilities.cpp

#$(NAME)_SUPPORTED_FEATURES := windows \
                              osx \
                              linux64

#$(NAME)_windows_SOURCES := windows.cpp
#$(NAME)_osx_SOURCES   := posix.cpp
#$(NAME)_linux64_SOURCES   := posix.cpp

$(NAME)_REQUIRED_COMPONENTS := yaml-cpp \
                               cpp-subprocess \
                               inja \
                               json \
                               semver \
                               indicators \
                               cxxopts \
							   spdlog \
                               taskflow

