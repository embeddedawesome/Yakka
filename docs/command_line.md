# Command line interface

The Yakka CLI operates by executing commands on a list of components and features.
Yakka has a minimal set of built-in commands that are reserved for its use
- build
- register


The first argument provided to Yakka is assumed to be a command, if it is not one of the built-in commands it is interpreted as a command from a component blueprint (see blueprints.md).
The remaining arguments are interpreted as component names but can be features if prefixed with `+` such as `+feature` or suffixed with `!` such as `compile!`.
