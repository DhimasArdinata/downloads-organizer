#pragma once

#include <filesystem>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

namespace fs = std::filesystem;
using json = nlohmann::json;

struct Condition {
  std::string type;
  std::vector<std::string> values;
  double threshold = 0.0;
};

inline void from_json(const json& j, Condition& c) {
  j.at("type").get_to(c.type);
  if (j.contains("values")) {
    j.at("values").get_to(c.values);
  }
  if (j.contains("threshold")) {
    j.at("threshold").get_to(c.threshold);
  }
}

struct Rule {
  std::string category;
  int priority;
  std::vector<Condition> conditions;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Rule, category, priority, conditions);

struct Config {
  std::unordered_map<std::string, std::string> categories;
  std::vector<Rule> rules;
};

enum class ActionType { MOVE };
NLOHMANN_JSON_SERIALIZE_ENUM(ActionType, {{ActionType::MOVE, "MOVE"}});
struct JournalEntry {
  ActionType action;
  fs::path from;
  fs::path to;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(JournalEntry, action, from, to);

struct Action {
  fs::path from;
  fs::path to;
  std::string reason;
};