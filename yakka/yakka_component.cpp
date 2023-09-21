#include "yakka_component.hpp"
#include "yakka_schema.hpp"
#include "blueprint_database.hpp"
#include "spdlog/spdlog.h"

namespace yakka {
yakka_status component::parse_file(fs::path file_path, blueprint_database &database)
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

  // Validate basic Yakka data
  bool result = yakka::schema_validator::get().validate(this);
  if (!result) {
    return yakka_status::FAIL;
  }

  // Add known information
  this->id           = file_path.stem().string();
  json["yakka_file"] = path_string;

  if (file_path.has_parent_path())
    path_string = file_path.parent_path().generic_string();
  else
    path_string = ".";
  json["directory"] = path_string;

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
  return yakka_status::SUCCESS;
}

} /* namespace yakka */
