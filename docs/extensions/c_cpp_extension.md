# C/C++ schema

All paths are relative to the component unless otherwise specified.

The term `local` is used to define includes and flags that only apply to files within this component.
The term `global` is used to define includes and flags that apply to every file defined by the project.

## Schema

| Key | Type | Description |
| --- | --- | --- |
| `sources` | List | List of source file paths |
| `includes/local` | List | List of paths to add as 'local' includes |
| `includes/global` | List | List of paths to add as 'global' includes |
| `defines/local` | List | List of symbols defined for local files |
| `defines/global` | List | List of symbols defined for every file |
| `flags/`[`c,cpp,S`]`>/local` | List | Flags used when compiling source file of the particular extension in this component |
| `flags/`[`c,cpp,S`]`>/global` | List | Flags used when compiling every source file of the particular extension |
| `flags/linker` | List | Flags used when linking the binary |
| `libraries` | List | List of precompiled libraries |

## Sample

```
sources:
  - source.c
  - src/extra/file.cpp
  - asm/assembly.S

includes:
  local:
    - private_includes
  global:
    - public_api

defines:
  local:
    - PRIVATE_VALUE=1
  global:
    - MY_COMPONENT_IS_ENABLED

flags:
  c:
    global:
      - -Os # Optimization level used when compiling .c files
  cpp:
    global:
      - -O3 # Optimization level used when compiling .cpp files
  linker:
    - -MD
```