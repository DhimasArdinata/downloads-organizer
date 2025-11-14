#pragma once

#include <functional>
#include <optional>

#include "types.hpp"

namespace IOManager {
void initialize_logger();

void set_log_handler(std::function<void(std::string_view)> handler);

void log(std::string_view message);
std::optional<fs::path> get_downloads_folder_path();
std::optional<Config> load_config(const fs::path& configPath);
void run_undo(const fs::path& journalPath);
void save_journal(const fs::path& journalPath,
                  const std::vector<JournalEntry>& journal);
}  // namespace IOManager