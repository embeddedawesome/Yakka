# Command line interface

The Yakka CLI operates by executing commands on a list of components and features.
Yakka has a minimal set of built-in commands that are reserved for it internal use
- `build`
- `register`
- `list`
- `update`


The first argument provided to Yakka is assumed to be a command unless it ends with a `!` to indicate that is references a [blueprint](blueprints).
The remaining arguments are interpreted as component names but can be features if prefixed with `+` such as `+feature` or can be blueprints if suffixed with `!` such as `compile!`.
There can be any number of features or blueprints provided via the command line.


