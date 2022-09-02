
# Prints the name and value of every variable that starts with "BOB_"
# This macro optionally takes an argument to print an additional header message
define PRINT_BOB_VARIABLES
$(if $(1),$(info $(1)))
$(foreach v,$(filter BOB_%,$(.VARIABLES)),$(info $(v) = $($(v))))
endef
