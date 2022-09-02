
# Definitions of basic build rules
clean:
	$(if $(PROJECT),@$(RM) -rf $(BOB_OUTPUT_BASE_DIRECTORY)/$(PROJECT),@$(RM) -rf $(BOB_OUTPUT_BASE_DIRECTORY))
