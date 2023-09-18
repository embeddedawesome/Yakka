#include "nlohmann/json.hpp"

namespace yakka {

// clang-format off
static const nlohmann::json component_schema = R"({
   "$schema": "https://json-schema.org/draft/2020-12/schema",
   "title": "Yakka file",
   "type": "object",
   "properties": {
      "blueprints": {
         "description": "Blueprints",
         "patternProperties": {
            ".*": {
               "additionalProperties": false,
               "minProperties": 1,
               "properties": {
                  "depends": {
                     "type": "array"
                  },
                  "process": {
                     "items": {
                        "type": "object"
                     },
                     "type": "array"
                  }
               },
               "type": "object"
            }
         },
         "propertyNames": {
            "pattern": "^[A-Za-z_][A-Za-z0-9_]*$"
         },
         "type": "object"
      },
      "name": {
         "description": "Name",
         "type": "string"
      },
      "requires": {
         "description": "Requires relationships",
         "properties": {
            "components": {
               "description": "Collection of components",
               "items": {
                  "type": "string"
               },
               "type": "array"
            },
            "features": {
               "description": "Collection of features",
               "items": {
                  "type": "string"
               },
               "type": "array"
            }
         },
         "type": "object"
      },
      "supports": {
         "description": "Supporting relationships",
         "properties": {
            "components": {
               "description": "Collection of components",
               "patternProperties": {
                  ".*": {
                     "type": "object"
                  }
               },
               "type": "object"
            },
            "features": {
               "description": "Collection of features",
               "patternProperties": {
                  ".*": {
                     "type": "object"
                  }
               },
               "type": "object"
            }
         },
         "type": "object"
      }
   },
   "required": [ "name" ]
})"_json;


static const std::string component_schema_yaml = R"(
title: Yakka file
type: object
properties:
  name:
    description: Name
    type: string

  requires:
    type: object
    description: Requires relationships
    properties:
      features:
        type: [array, null]
        description: Collection of features
        items:
          type: string
      components:
        type: [array, null]
        description: Collection of components
        items:
          type: string

  supports:
    type: object
    description: Supporting relationships
    properties:
      features:
        type: object
        description: Collection of features
        patternProperties:
          '.*':
            type: object
      components:
        type: object
        description: Collection of components
        patternProperties:
          '.*':
            type: object

  blueprints:
    type: object
    description: Blueprints
    propertyNames:
      pattern: "^[A-Za-z_.{][A-Za-z0-9.{}/\\\\_]*$"
    patternProperties:
      '.*':
        type: object
        additionalProperties: false
        minProperties: 1
        properties:
          regex:
            type: string
          depends:
            type: array
          process:
            type: array
            items:
              type: object

required: 
  - name
)";
// clang-format on
} // namespace yakka