NAME := BOB

$(NAME)_SOURCES := yakka_cli.cpp \
                   yakka_project.cpp \
                   yakka_component.cpp \
                   yakka_workspace.cpp \
                   component_database.cpp \
                   yakka_blueprint.cpp \
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

