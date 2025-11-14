#pragma once

#include <stop_token>
#include <vector>

#include "types.hpp"

class RuleEngine {
 public:
  explicit RuleEngine(const Config& config);

  std::vector<Action> generate_plan(
      const fs::path& targetDir,
      std::optional<std::stop_token> stoken = std::nullopt) const;

 private:
  std::optional<Action> generate_action_for_path(
      const fs::path& path, const fs::path& targetDir) const;
  std::optional<std::string> get_exif_date(const fs::path& path) const;

  const Config& m_config;
};