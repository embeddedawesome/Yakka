# Design
Yakka defines a specification that is used to describe a software component and how it relates to other components in a system.

It uses YAML, Jinja-like templating, and regular expressions to provide a flexible environment to succincly describe each component and the actions to perform transformations on those components.

## Key Concepts
### Components
A component is an abstract concept that contains data and describes its relationship to other components or features. It may represent a specific piece of software or a library, it may be a toolchain with binaries and blueprints on how to perform transforms, or it may be a meta component that describes a concept or group of functionality.

A component may contain any YAML data and Yakka defines a minimal set of entries to be considered a valid Yakka component.

The definition and structure of additional YAML data beyond the Yakka specification is to be provided by agreed standards. For example, the C/C++ language extension defines the use of "sources", "includes", and "defines" as a way to describe common C/C++ software component.

### Features
A feature is a unique name that represents a concept that may apply to one or many components.
Features provide a mechanism for a component to alter its behaviour depending on the requirements of the system around it. This removes the need to sub-divide a single component into multiple sub-components to support different aspects of functionality.

If a supported feature is applied to a component, the YAML data of that feature is merged into the parent component.

Feature names are arbitrary and aim to be human readable and relatable.

See [Features](docs/features.md) for further details and reference code.

### Blueprint
Blueprints are similar in concept to Makefile recipes where a target is related to its dependencies and describes a list of shell commands that are to be executed to update the target.
Yakka blueprint targets can be a simple string, a templated string, or a complicated regex with capture groups. They can depend on other blueprints, files, or specific data in any component.

See [Blueprints](docs/blueprints.md) for further details and reference code.

### Relationships

There are three relationships a component can have with another component or feature; requires, supports, and provides.

*Requires*

A "requires" relationship is a dependency that must be met. This applies to components, features, blueprints, and data.

*Supports*

A "supports" relationship is an optional dependency that will take effect if the indicated feature, component, or data is available. but is otherwise not an error condition

*Provides*

A "provides" relationship indicates that the required feature will be fulfilled.

### Tools
TBD

## Component Descriptor Specification
Components are defined using YAML. Yakka has a minimal set of reserved elements/keywords but expects that the development community will define extensions used by tool components such as toolchains.

Each descriptor must have a 'name' element that defines a human readable name. This is used to name build outputs and so is constrained by standard operating system naming restrictions.
The forbidden characters are:

- / (forward slas
- < (less than)
- > (greater than)
- : (colon)
- " (double quote)
- / (forward slash)
- \ (backslash)
- | (vertical bar or pipe)
- ? (question mark)
- * (asterisk)
- 0-31 (ASCII control characters)

Note: Names cannot end in a space or dot.

### Keywords

1. 'name'
2. 'requires'
3. 'supports'
4. 'provides'
5. 'blueprints'
6. 'tools'
7. 'choices'
8. 'replaces'

## Descriptor Extensions
Yakka has support for components to define their own schema for use by their blueprints.

[C/C++](docs/extensions/c_cpp_extension)
[Doxygen](docs/extensions/doxygen_extension)