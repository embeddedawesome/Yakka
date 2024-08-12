#include <string>

namespace yakka {

// clang-format off
const std::string slcc_schema_yaml = R"(
title: SLCC file
type: object
properties:
  id:
    description: Represents the component's unique ID
    type: string
    propertyNames:
      pattern: "[-A-Za-z0-9_]+"
  package:
    description: The package key is used to describe which SDK package the component should be part of.
    type: string
  description:
    description: Textual description of the component
    type: string
  category:
    description: Component category to assist with discovery and classification.
    type: string
  quality:
    description: The quality level of the component.
    type: string
    propertyNames:
      pattern: "^production|evaluation|experimental|deprecated|internal$"
  tag:
    description: List of tags that can be used to group and classify components.
    type: array
    items:
      type: string
  label:
    description: Display name for the component in user interfaces.
    type: string
  author:
    description: String used to document the author of a component.
    type: string
  root_path:
    description: The root_path key can be used to append a prefix to all paths specified in the component.
    type: string
  instantiable:
    description: If present, the component is instantiable. Multiple copies of an instantiable component can be added to the project.
    type: object
    properties:
      prefix:
        description: The default prefix used to name instances.
        type: string
    required: [ prefix ]
  config_file:
    description: List of configuration files to be included in the project.
    type: array
    items:
      type: object
      properties:
        path:
          type: string
        directory:
          type: string
        file_id:
          type: string
        override:
          type: object
          properties:
            file_id:
              type: string
            component:
              type: string
            instance:
              type: string
          required: [ file_id, component ]
        condition:
          type: array
          uniqueItems: true
          items:
            type: string
        unless:
          type: [array, null]
          uniqueItems: true
          items:
            type: string
        export:
          type: boolean
      required: [ path ]
  source:
    type: array
    items:
      type: object
      properites:
        path:
          type: string
        project:
          type: object
          properties:
            transfer_ownership:
              type: boolean
            directory:
              type: string
          required: [transfer_ownership]
        condition:
          type: array
          uniqueItems: true
          items:
            type: string
        unless:
          type: [array, null]
          uniqueItems: true
          items:
            type: string
      required: [ path ]
  include:
    type: array
    items:
      type: object
      properites:
        path:
          type: string
        file_list:
          type: array
          items:
            type: object
            properties:
              path:
                type: string
              condition:
                type: array
                uniqueItems: true
                items:
                  type: string
              unless:
                type: [array, null]
                uniqueItems: true
                items:
                  type: string
            required: [ path ]
        condition:
          type: array
          uniqueItems: true
          items:
            type: string
        unless:
          type: [array, null]
          uniqueItems: true
          items:
            type: string
      required: [ path ]
  library:
    type: array
    items:
      anyOf:
        - type: object
          properties:
            system:
              type: string
            condition:
              type: array
              uniqueItems: true
              items:
                type: string
            unless:
              type: [array, null]
              uniqueItems: true
              items:
                type: string
        - type: object
          properties:
              path:
                type: string
              condition:
                type: array
                uniqueItems: true
                items:
                  type: string
              unless:
                type: [array, null]
                uniqueItems: true
                items:
                  type: string
  define:
    type: array
    items:
      type: object
      properties:
        name:
          type: string
        value:
          type: [string, number, boolean]
        condition:
          type: array
          uniqueItems: true
          items:
            type: string
        unless:
          type: [array, null]
          uniqueItems: true
          items:
            type: string
      required: [ name ]
  template_contribution:
    type: [array, null]
    items:
      type: object
      properties:
        name:
          type: string
        value:
          type: [string, number, object, array, boolean]
        priority:
          type: number
        condition:
          type: array
          uniqueItems: true
          items:
            type: string
        unless:
          type: [array, null]
          uniqueItems: true
          items:
            type: string
      required: [ name, value ]
  template_file:
    type: array
    items:
      type: object
      properties:
        path:
          type: string
        condition:
          type: array
          uniqueItems: true
          items:
            type: string
        unless:
          type: [array, null]
          uniqueItems: true
          items:
            type: string
        export:
          type: boolean
      required: [ path ]
  toolchain_settings:
    type: array
    items:
      type: object
      properties:
        option:
          type: string
        value:
          type: [string, number, boolean, array]
        condition:
          type: array
          uniqueItems: true
          items:
            type: string
        unless:
          type: [array, null]
          uniqueItems: true
          items:
            type: string
      required: [ option, value ]
  other_file:
    type: array
    items:
      type: object
      properties:
        path:
          type: string
        project:
          type: object
          properties:
            transfer_ownership:
              type: boolean
            directory:
              type: string
        condition:
          type: array
          uniqueItems: true
          items:
            type: string
        unless:
          type: [array, null]
          uniqueItems: true
          items:
            type: string
      required: [ path ]
  requires:
    type: [array, null]
    items:
      type: object
      properties:
        name:
          type: string
        condition:
          type: array
          uniqueItems: true
          items:
            type: string
      required: [ name ]
  provides:
    type: [array, null]
    items:
      type: object
      properties:
        name:
          type: string
        condition:
          type: array
          uniqueItems: true
          items:
            type: string
        allow_multiple:
          type: boolean
      required: [ name ]
  conflicts:
    type: array
    items:
      type: object
      properties:
        name:
          type: string
        condition:
          type: array
          uniqueItems: true
          items:
            type: string
      required: [ name ]
  validation_helper:
    type: array
    items:
      type: object
      properties:
        path:
          type: string
        condition:
          type: array
          uniqueItems: true
          items:
            type: string
        unless:
          type: [array, null]
          uniqueItems: true
          items:
            type: string
      required: [ path ]
  validation_library:
    type: array
    items:
      type: object
      properties:
        path:
          type: string
        name:
          type: string
        condition:
          type: array
          uniqueItems: true
          items:
            type: string
        unless:
          type: [array, null]
          uniqueItems: true
          items:
            type: string
      required: [ path, name ]
  recommends:
    type: [array, null]
    items:
      type: object
      properties:
        id:
          type: string
        instance:
          type: array
          items:
            type: string
        condition:
          type: array
          uniqueItems: true
          items:
            type: string
      required: [ id ]
  documentation:
    anyOf:
      - type: object
        properties:
          docset:
            type: string
          document:
            anyOf:
              - type: string
              - type: array
                items:
                  type: object
                  properties:
                    page:
                      type: string
                    condition:
                      type: array
                      uniqueItems: true
                      items:
                        type: string
                    unless:
                      type: [array, null]
                      uniqueItems: true
                      items:
                        type: string
                  required: [ page ]
        required: [ docset, document ]
      - type: object
        properties:
          page:
            type: string
        required: [ page ]
      - type: object
        properties:
          url:
            type: string
        required: [ url ]
  ui_hints:
    type: object
    properties:
      visibility:
        type: string
        pattern: "never|basic|advanced"
  clone:
    type: object
    properties:
      component:
        type: string
      sdk:
        type: object
      date:
        type: string
    required: [component, sdk, date]

required: 
  - id
  - package
  - description
  - category
  - quality
  )";
// clang-format on

} // namespace yakka