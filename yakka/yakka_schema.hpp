#include "yaml-cpp/yaml.h"
#include <nlohmann/json-schema.hpp>
#include "spdlog.h"
#include "slcc_schema.hpp"

namespace yakka {

class schema_validator {
  nlohmann::json yakka_schema;
  nlohmann::json slcc_schema;
  nlohmann::json_schema::json_validator yakka_validator;
  nlohmann::json_schema::json_validator slcc_validator;

  // clang-format off
  const std::string yakka_component_schema_yaml = R"(
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
          uniqueItems: true
          items:
            type: string
        components:
          type: [array, null]
          description: Collection of components
          uniqueItems: true
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
    choices:
      type: object
      description: Choices
      propertyNames:
        pattern: "^[A-Za-z0-9_.]*$"
      patternProperties:
        '.*':
          type: object
          additionalProperties: false
          minProperties: 1
          required:
            - description
          properties:
            description:
              type: string
            features:
              type: array
              items:
                type: string
            components:
              type: array
              items:
                type: string
            default:
              type: object
              oneOf:
                - properties:
                    feature:
                      type: string
                  required:
                    - feature
                - properties:
                    component:
                      type: string
                  required:
                    - component

  required: 
    - name
  )";
  // clang-format on

  class custom_error_handler : public nlohmann::json_schema::basic_error_handler {
  public:
    yakka::component *component;
    void error(const nlohmann::json::json_pointer &ptr, const nlohmann::json &instance, const std::string &message) override
    {
      nlohmann::json_schema::basic_error_handler::error(ptr, instance, message);
      spdlog::error("Validation error in '{}': {} - {} : - {}", component->file_path.generic_string(), ptr.to_string(), instance.dump(3), message);
    }
  };

public:
  static schema_validator &get()
  {
    static schema_validator the_validator;
    return the_validator;
  }

private:
  schema_validator() : yakka_validator(nullptr, nlohmann::json_schema::default_string_format_check), slcc_validator(nullptr, nlohmann::json_schema::default_string_format_check)
  {
    // This should be straight JSON without conversion
    yakka_schema = YAML::Load(yakka_component_schema_yaml).as<nlohmann::json>();
    slcc_schema  = YAML::Load(slcc_schema_yaml).as<nlohmann::json>();
    yakka_validator.set_root_schema(yakka_schema);
    slcc_validator.set_root_schema(slcc_schema);
  }

public:
  schema_validator(schema_validator const &) = delete;
  void operator=(schema_validator const &)   = delete;

  bool validate(yakka::component *component)
  {
    custom_error_handler err;
    err.component = component;
    if (component->type == component::YAKKA_FILE)
      auto patch = yakka_validator.validate(component->json, err);
    else if (component->type == component::SLCC_FILE)
      auto patch = slcc_validator.validate(component->json, err);
    if (err) {
      return false;
    } else {
      return true;
    }
  }
};

schema_validator yakka_validator();
} // namespace yakka