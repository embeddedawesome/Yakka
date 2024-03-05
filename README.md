# Yakka

Yakka is a modern, data-focused build tool designed to aid embedded software development.

It recognizes that creating and delivering software is more than just compilation or linking and thus makes it easy to define new processes.

Yakka can be utilized as a full build system but also recognizes the value of tooling diversity and thus can integrate as part of an 
existing build system by only providing required functionality

## Why use Yakka

Yakka aims to be descriptive, storing information as data rather than encoding it within a programming language. 
This enables an ecosystem of open tooling and allows the use simple tools such as `jq` or `yq` to analyze components or projects.

It uses standard, commonly used technologies; YAML, JSON, template engine (Jinja*), regular expressions.

Yakka is extremely flexible by defining a minimal data schema and enabling components to create their own schemas and processes.

Data transformation, such as compilation, are defined by 'blueprints' specified in components, such as a toolchain, rather than being built into Yakka.
This allows components to include not only data or binaries, but also the definition of the process to accomplish the transformation.

Everything is a component. This includes source code, tools, documentation, projects, applications, ...
No need for a multitude of file types or special rules making the system easy to understand.

Being data focused enables blueprints to have data dependencies allowing optimized builds and advanced data generation techniques.


## Examples

### Minimal component
```
name: 'my component'
```

### Simple C component
```
name: 'my component'
sources:
  - my_source.c
```


## Usage

### Linking a project
This example defines three components and the `link` blueprint that is part of the `gcc_arm` component.
```
yakka link! my_project platform_1 gcc_arm
```

### 