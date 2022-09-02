
define USAGE_TEXT
Usage: make <components> <commands> <+features>

  <components>
    A list of space separated components

  <commands>
    A list of space separate commands
    
  <+features>
    A list of features, each starting with +

endef

help:
	$(info $(USAGE_TEXT))