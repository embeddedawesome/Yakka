

# Recursively find wildcard directories/files
# $(1) the starting directory path
# $(2) optional pattern(s) to search
# $(3) optional exclude pattern(s)
#
# Example:
# To find all the C files in the current directory:
# $(call rwildcard, , *.c)
#
# To find all the .c and .h files in src:
# $(call rwildcard, src/, *.c *.h)
rwildcard=$(foreach d,$(wildcard $1*),$(call rwildcard,$d/,$2,$3) $(filter-out $(subst *,%,$3),$(filter $(subst *,%,$2),$d)))


# Filter out all entries that contain supplied string
# Only values in the haystack that do NOT contain the supplied string are returned
# $(1) - needle, string to search
# $(2) - haystack, list of strings to search and filter
FILTER_OUT = $(foreach v,$(2),$(if $(findstring $(1),$(v)),,$(v)))

# Filter all entries that contain supplied string
# Only values in the haystack that DO contain the supplied string are returned
# $(1) - needle, string to search
# $(2) - haystack, list of strings to search and filter
FILTER = $(foreach v,$(2),$(if $(findstring $(1),$(v)),$(v)))

# Normalize the file path relative to SDK root directory
# $(1) - path to be normalized
NORMALIZE=$(patsubst ./%,%,$(subst $(abspath $(1)),.,$(abspath $(2))))

ABSOLUTE=$(subst $(BOB_ABSOLUTE_SUBST),,$(abspath $(1)))

# Return unique entries in list WITHOUT sorting
# $(1) - list of entries with possible duplicates
# Non-sorted list of unique entries
UNIQUE=$(eval _seen :=)\
       $(foreach entry,$1,$(if $(filter $(entry),$(_seen)),,$(eval _seen +=$(entry))))\
       $(strip $(_seen))
       
       
# Print newline
define NEWLINE


endef

COMMA :=,

# Convert variable to lowercase
# $(1) - variables to convert to lowercase
TOLOWER=$(subst A,a,$(subst B,b,$(subst C,c,$(subst D,d,$(subst E,e,$(subst F,f,$(subst G,g,$(subst H,h,$(subst I,i,$(subst J,j,$(subst K,k,$(subst L,l,$(subst M,m,$(subst N,n,$(subst O,o,$(subst P,p,$(subst Q,q,$(subst R,r,$(subst S,s,$(subst T,t,$(subst U,u,$(subst V,v,$(subst W,w,$(subst X,x,$(subst Y,y,$(subst Z,z,$1))))))))))))))))))))))))))

# Convert variable to uppercase
# $(1) - variables to convert to uppercase
TO_UPPER=$(subst a,A,$(subst b,B,$(subst c,C,$(subst d,D,$(subst e,E,$(subst f,F,$(subst g,G,$(subst h,H,$(subst i,I,$(subst j,J,$(subst k,K,$(subst l,L,$(subst m,M,$(subst n,N,$(subst o,O,$(subst p,P,$(subst q,Q,$(subst r,R,$(subst s,S,$(subst t,T,$(subst u,U,$(subst v,V,$(subst w,W,$(subst x,X,$(subst y,Y,$(subst z,Z,$1))))))))))))))))))))))))))


###################################################################################################
# Directory creation rules
%/.d:
	$(if $(wildcard $(dir $@)),,$(QUIET)$(call MKDIR,$(dir $@)))
	
%.directory:
	$(if $(wildcard $(dir $@)),,$(QUIET)$(call MKDIR,$(dir $@)))

###################################################################################################
# INCLUDE_TOOLCHAIN_MAKEFILES
# Includes the relevant toolchain makefiles for a given command
# $(1) is the name of the command
define INCLUDE_TOOLCHAIN_MAKEFILES
$(foreach makefile,$(BOB_$(1)_TOOLCHAIN_MAKEFILES), $(eval $(call INCLUDE_ADDITIONAL_MAKEFILE,$(makefile))))
endef

###################################################################################################
# INCLUDE_ADDITIONAL_MAKEFILE
# Includes the relevant makefiles
# $(1) is the name of the makefile
define INCLUDE_ADDITIONAL_MAKEFILE
$(if $(filter $(1),$(INCLUDED_ADDITIONAL_MAKEFILES)),,\
	$(eval CURDIR :=$(dir $(1)))\
	$(eval INCLUDED_ADDITIONAL_MAKEFILES += $(1))\
	$(eval include $(1))\
	$(eval CURDIR :=./)\
)
endef