NAME := boost

REQUIRED_BOOST_LIBS := lockfree assert static_assert config core type_traits mpl preprocessor array throw_exception align parameter mp11 utility predef

$(NAME)_GLOBAL_INCLUDES := master/libs/config/include \
                           master/libs/system/include \
                           master/libs/core/include \
                           $(foreach lib,$(REQUIRED_BOOST_LIBS),master/libs/$(lib)/include )

$(NAME)_GLOBAL_DEFINES := BOOST_ERROR_CODE_HEADER_ONLY \
                          BOOST_SYSTEM_SOURCE


# To fetch required components: git submodule update --init --recursive $(REQUIRED_BOOST_LIBS)