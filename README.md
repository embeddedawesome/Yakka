# BOB

BOB aims to provide an open-source, standardized set of modern software development tools for a range of development environments while remaining language agnostic.
Many modern programming languages recognize that writing source code is only a small part of the full software development lifecycle and thus strive to include additional functionality required to develop and deploy software projects. This includes test infrastructure, dependency management, component packaging, and software deployment.

## Design
BOB defines a language for software developers to describe a software component and how it relates to other components.
It uses YAML, Jinja templating, and regular expressions to provide a flexible environment to succincly describe each component and the actions to perform transformations on those components.

## Concepts
### Components
A YAML files that describe something. Components could represent specific piece of software or a toolchain or could be a meta component that describes a concept or group of functionality.

Components can have a relationship to other components or features

### Features
A feature is a unique name that represents a concept that is not tied to a specific component. It is an orthogonal concept to components that are represent a specific piece of software

Human readable and relatable

### Data
Data has no particular structure and can be part of the root node of any component

Data can be required but not provided or supported. Provide functionality is done by simply having the data as part of your YAML doc.

### Blueprint
Named set of instructions that perform actions and have their own set of dependencies.
A blueprint can be matched by its name or via a regex.

### Relationships
*Requires*

A "require" relationship is a dependency that must be met. This applies to components, features, blueprints, and data.

*Supports*

A "supports" relationship is a dependency that can take effect if the indicated feature or data is available but is otherwise not an error condition

*Provides*

A "provides" relationship indicates that the required feature will be fulfilled.

### Tools
TBD

## Component Descriptor Specification
Components are defined by a YAML file. BOB has a minimal set of reserved elements/keywords but expects that the development community will define extensions used by tool components such as toolchains.

Each descriptor must have a 'name' element that defines a human readable name. This is used to name build outputs and so is constrained by standard operating system naming restrictions.
The forbidden characters are:

- / (forward slas
- < (less than)
- > (greater than)
- : (colon - sometimes works, but is actually NTFS Alternate Data Streams)
- " (double quot))))e)
- / (forward slash)
- \ (backslash)
- | (vertical bar or pipe)
- ? (question mark)
- * (asterisk)
- 0-31 (ASCII control characters)

Note: Names cannot end in a space or dot.

### Keywords

1. 'requires'

2. 'supports'

3. 'provides'

4. 'blueprints'

5. 'tools'

## Descriptor Extensions
BOB has support for components to define their own standard set of YAML elements for use by their blueprints.

### C/C++

1. 'sources'

2. 'includes'

3. 'flags'
Specifically 'flags/c', 'flags/c++', 'flags/S', 'flags/linker' which comes with '/local' and '/global' sections

4. 'libraries'

## Blueprints
Blueprints objects can have the following entries:
"supports": optional dependencies
"requires": Dependencies that must be fulfilled
"process": Sequence of actions for the blueprint
