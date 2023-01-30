# Blueprints
Blueprints are akin to Makefile recipes that map targets to dependencies and the process that performs the actions required by that target.
Targets can be simple strings, template strings, or complicated regex strings with capture groups.

```
blueprints:
  # Simple string example
  compile:
    depends: ...
    process: ...

  # Template string example
  '{{project_output}}/{{project_name}}':
    depends: ...
    process: ...

  # Regex example
  object_files:
    regex: .+/components/([^/]*)/(.*)\.(cpp|c)\.o
    depends: ...
    process: ...
```

A blueprint may contain a `depends` sequence that lists all the dependencies and/or a `process` sequence that lists the steps to execute to apply the blueprint.

## Targets
A simple string target is matched to a command or dependency by a string comparison.
These targets are typically used for commands such as `compile`, `link`, or `analyze`.

## Dependencies
The `depends` sequence is a list of dependencies that are matched to other blueprints or files in the filesystem.
Each entry is expanded as a template string and there is support for entries that expand to a YAML sequence by wrapping the dependency string in `[` `]`. Note in the example below that the expansion tolerates a trailing comma to simplify the declaration.

*List expansion example*
```
# The blueprint to build a binary executable of a project
'{{project_output}}/{{project_name}}':
    depends:
      # Depends on the object files for every source file for every component
      # Note: 'components' is a map that returns key-value pairs, in this instance, 'name' and 'component'
      - '[{% for name, component in components %}{% for source in component.sources %}{{project_output}}/components/{{name}}/{{source}}.o, {% endfor %}{% endfor %}]'
```

Blueprints can also depend on specific data within component files by defining a data dependency. Data dependencies can apply to a specific component or can use a wildcard "*" to depend on a data path in every component in the project. During blueprint evaluation Yakka will determine if those specific data entries have been modified since the previous run.

*Data dependency examples*
```
# This blueprint generates a file that contains all the GCC options that affect every source file
'{{project_output}}/{{project_name}}.global_c_options':
    depends:
      - data:
          - '*.flags.c.global'   # This depends on global C flags defined in any component
          - '*.includes.global'
          - '*.defines.global'
    process:
      - inja: "{% for name,component in components %}
        {% for flag in component.flags.c.global %}{{flag}} {% endfor %}
        {% for include in component.includes.global %}-I{{component.directory}}/{{include}} {% endfor %}
        {% for define in component.defines.global %}-D{{define}} {% endfor %}
        {%endfor%}
        {% for feature in features %}-D{{feature}}_FEATURE_REQUIRED {% endfor %}
        -I{{project_output}}"
      - save:
```
```
# This blueprint generates a file that contains all the GCC options that are specific for a particular component
compiler_option_files:
    regex: '.+/components/([^/]*)/\1\.c_options' # This regex matches to .../components/X/X.c_options where X is the name of a component which is captured
    depends:
      - data:
          - '{{$(1)}}.flags.c.local'  # This template entry references capture group 1 in the regex to reference the flags in the particular component
          - '{{$(1)}}.includes.local'
          - '{{$(1)}}.defines.local'
    process:
      - create_directory: '{{$(0)}}'
      - inja: "{% set component = at(components,$(1)) -%}
        {% for flag in component.flags.c.local) %}{{flag}} {% endfor %}
        {% for include in component.includes.local %}-I{{component.directory}}/{{include}} {% endfor %}
        {% for define in component.defines.local %}-D{{define}} {% endfor %}"
      - save:
```

## Processes