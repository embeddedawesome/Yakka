NAME := YAML_Inja

$(NAME)_SOURCES := yinja.cpp

$(NAME)_REQUIRED_COMPONENTS := yaml-cpp \
                               inja \
                               cxxopts