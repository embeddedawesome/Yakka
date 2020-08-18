NAME := boost

$(NAME)_GLOBAL_INCLUDES := boost/config/include \
                           boost/system/include \
                           boost/core/include \
                           .

$(NAME)_GLOBAL_DEFINES := BOOST_ERROR_CODE_HEADER_ONLY \
                          BOOST_FILESYSTEM_SOURCE \
                          BOOST_SYSTEM_SOURCE
                           
$(NAME)_INCLUDES := .

$(NAME)_SUPPORTED_FEATURES := boost_filesystem

$(NAME)_boost_filesystem_SOURCES := boost/filesystem/src/operations.cpp \
                                    boost/filesystem/src/path.cpp \
                                    boost/filesystem/src/utf8_codecvt_facet.cpp \
                                    boost/filesystem/src/windows_file_codecvt.cpp \
                                    boost/filesystem/src/path_traits.cpp \
                                    boost/filesystem/src/codecvt_error_category.cpp

