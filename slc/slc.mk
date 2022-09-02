NAME := SLC

$(NAME)_SOURCES:= slc.cpp \
                  slc_project.cpp

$(NAME)_REQUIRED_COMPONENTS := yaml-cpp \
                               json \
                               indicators \
                               cxxopts \
							   spdlog
