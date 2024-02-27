#include "yakka_component.hpp"
#include "yakka_schema.hpp"
#include "spdlog/spdlog.h"
#include "semver.hpp"

namespace yakka {
yakka_status component::parse_file(fs::path file_path, fs::path package_path)
{
  this->file_path         = file_path;
  std::string path_string = file_path.generic_string();
  spdlog::info("Parsing '{}'", path_string);

  try {
    json = YAML::LoadFile(path_string).as<nlohmann::json>();
  } catch (std::exception &e) {
    spdlog::error("Failed to load file: '{}'\n{}\n", path_string, e.what());
    std::cerr << "Failed to parse: " << path_string << "\n" << e.what() << "\n";
    return yakka_status::FAIL;
  }

  if (file_path.filename().extension() == slcc_component_extension) {
    this->type = SLCC_FILE;
    convert_to_yakka(package_path);
  } else if (file_path.filename().extension() == slcp_component_extension) {
    this->type = SLCP_FILE;
    convert_to_yakka(package_path);
  } else {
    this->type = YAKKA_FILE;
    // Validate basic Yakka data
    bool result = yakka::schema_validator::get().validate(this);
    if (!result) {
      return yakka_status::FAIL;
    }

    if (file_path.has_parent_path())
      path_string = file_path.parent_path().generic_string();
    else
      path_string = ".";
    json["directory"] = path_string;

    // Set version
    if (json.contains("version")) {
      this->version = semver::from_string_noexcept(json["version"].get<std::string>()).value();
    } else {
      this->version = { 0, 0, 0 };
    }

    // Ensure certain nodes are sequences
    if (json["requires"]["components"].is_string()) {
      std::string value              = json["requires"]["components"].get<std::string>();
      json["requires"]["components"] = nlohmann::json::array({ value });
    }

    if (json["requires"]["features"].is_string()) {
      std::string value            = json["requires"]["features"].get<std::string>();
      json["requires"]["features"] = nlohmann::json::array({ value });
    }

    // Fix relative component addressing
    for (auto n: json["requires"]["components"])
      if (n.get<std::string>().front() == '.')
        n = n.get<std::string>().insert(0, path_string);

    if (json.contains("supports")) {
      if (json["supports"].contains("features")) {
        for (auto f: json["supports"]["features"])
          for (auto n: f["requires"]["components"])
            if (n.get<std::string>().front() == '.')
              n = n.get<std::string>().insert(0, path_string);
      }
      if (json["supports"].contains("components")) {
        for (auto c: json["supports"]["components"])
          for (auto n: c["requires"]["components"])
            if (n.get<std::string>().front() == '.')
              n = n.get<std::string>().insert(0, path_string);
      }
    }
  }

  // Add known information
  this->id           = file_path.stem().string();
  json["yakka_file"] = file_path.string();
  return yakka_status::SUCCESS;
}

void component::convert_to_yakka(fs::path package_path)
{
  // Check if SLCC file is an omap. Convert it to a map
  if (json.is_array()) {
    nlohmann::json new_format;
    for (const auto &item: json.items())
      for (const auto &[key, value]: item.value().items())
        new_format[key] = value;
    json = new_format;
  }

  // Set basic data such as directory and name
  json["name"] = json["label"];

  if (json.contains("component_root_path")) {
    std::string temp_path;
    if (!package_path.empty())
      temp_path = package_path.string() + "/";
    json["directory"] = temp_path + json["component_root_path"].get<std::string>();
  } else if (json.contains("root_path")) {
    std::string temp_path;
    if (!package_path.empty())
      temp_path = package_path.string() + "/";
    json["directory"] = temp_path + json["root_path"].get<std::string>();
  } else {
    json["directory"] = package_path.string(); // file_path.parent_path().string();
  }

  // Process 'provides'
  if (json.contains("provides")) {
    nlohmann::json provides;
    for (const auto &p: json["provides"]) {
      if (p.contains("condition"))
        json[create_condition_pointer(p["condition"])]["provides"]["features"].push_back(p["name"]);
      else
        provides["features"].push_back(p["name"]);
    }
    json["provides"] = provides;
  }

  // Process 'requires'
  if (json.contains("requires")) {
    nlohmann::json temp;
    for (const auto &p: json["requires"]) {
      if (p.contains("condition"))
        json[create_condition_pointer(p["condition"])]["requires"]["features"] = p["name"];
      else
        temp["features"].push_back(p["name"]);
    }
    json["requires"] = temp;
  }

  // Process 'source'
  if (json.contains("source")) {
    nlohmann::json sources;
    for (const auto &p: json["source"]) {
      if (!p.contains("path"))
        continue;
      if (p.contains("condition"))
        json[create_condition_pointer(p["condition"])]["sources"] = p["path"];
      else
        sources.push_back(p["path"]);
    }
    json["sources"] = sources;
    json.erase("source");
  }

  // Process 'include'
  if (json.contains("include")) {
    nlohmann::json includes;
    for (const auto &p: json["include"]) {
      if (p.contains("condition"))
        json[create_condition_pointer(p["condition"])]["includes"]["global"] = p["path"];
      else
        includes["global"].push_back(p["path"]);
    }
    json["includes"] = includes;
    json.erase("include");
  }

  // Process 'component' (only available for slcp)
  if (json.contains("component")) {
    for (const auto &p: json["component"]) {
      json["requires"]["components"].push_back(p["id"]);
    }
  }

  // Process 'template_file'
  if (json.contains("template_file")) {
    for (const auto &t: json["template_file"]) {
      fs::path template_file = t["path"].get<std::string>();
      fs::path target_file   = template_file.filename();
      target_file.replace_extension();

      const auto target = "{{project_output}}/generated/" + target_file.string();

      auto add_generated_item = [&](nlohmann::json &node) {
        // Create generated items
        if (target_file.extension() == ".c" || target_file.extension() == ".cpp")
          node["generated"]["sources"].push_back(target);
        else if (target_file.extension() == ".h" || target_file.extension() == ".hpp")
          node["generated"]["includes"].push_back(target);
        else
          node["generated"]["files"].push_back(target);
      };

      if (t.contains("condition")) {
        auto pointer = create_condition_pointer(t["condition"]);
        if (!json.contains(pointer))
          json[pointer] = {};

        add_generated_item(json[pointer]);
      } else
        add_generated_item(json);

      // Create blueprints
      nlohmann::json blueprint = { { "process", nullptr } };
      blueprint["process"].push_back({ { "inja", { { "template_file", json["directory"].get<std::string>() + "/" + template_file.string() } } } });
      blueprint["process"].push_back({ { "save", nullptr } });

      json["blueprints"][target] = blueprint;
    }
  }
}

nlohmann::json::json_pointer component::create_condition_pointer(nlohmann::json condition)
{
  nlohmann::json::json_pointer pointer;

  for (const auto &item: condition) {
    pointer /= "/supports/features"_json_pointer;
    pointer /= item.get<std::string>();
  }

  return pointer;
}

} /* namespace yakka */
