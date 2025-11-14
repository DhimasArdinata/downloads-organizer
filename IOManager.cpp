#include "IOManager.hpp"

#include <chrono>
#include <fstream>
#include <functional>
#include <mutex>
#include <print>

#ifdef _WIN32
#include <Shlobj.h>
#else
#include <cstdlib>  // For getenv
#endif

namespace {
std::ofstream& get_log_stream() {
  static std::ofstream log_file("organizer.log", std::ios_base::app);
  return log_file;
}

std::mutex log_mutex;

std::function<void(std::string_view)> g_log_handler = nullptr;

}  // namespace

void IOManager::initialize_logger() { get_log_stream(); }

void IOManager::set_log_handler(std::function<void(std::string_view)> handler) {
  std::scoped_lock lock(log_mutex);
  g_log_handler = handler;
}

void IOManager::log(std::string_view message) {
  std::scoped_lock lock(log_mutex);

  auto now = std::chrono::system_clock::now();
  auto time_str = std::format("{:%Y-%m-%d %H:%M:%S}", now);
  std::string full_message = std::format("{} | {}", time_str, message);

  if (g_log_handler) {
    g_log_handler(full_message);
  }

  auto& log_stream = get_log_stream();
  if (log_stream.is_open()) {
    log_stream << full_message << "\n" << std::flush;
  }
}

std::optional<fs::path> IOManager::get_downloads_folder_path() {
#ifdef _WIN32
  PWSTR path_raw = nullptr;
  if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Downloads, 0, NULL, &path_raw))) {
    fs::path result(path_raw);
    CoTaskMemFree(path_raw);
    return result;
  }
#else
  const char* home_dir = getenv("HOME");
  if (!home_dir) return std::nullopt;

  const char* xdg_download_dir_env = getenv("XDG_DOWNLOAD_DIR");
  if (xdg_download_dir_env && *xdg_download_dir_env) {
    fs::path p(xdg_download_dir_env);
    if (p.string().starts_with("$HOME")) {
      return fs::path(home_dir) / p.string().substr(6);
    }
    if (p.is_absolute()) return p;
  }

  fs::path user_dirs_file = fs::path(home_dir) / ".config/user-dirs.dirs";
  if (fs::exists(user_dirs_file)) {
    std::ifstream file(user_dirs_file);
    std::string line;
    while (std::getline(file, line)) {
      if (line.empty() || line[0] == '#') continue;

      if (line.starts_with("XDG_DOWNLOAD_DIR=")) {
        auto first_quote = line.find('"');
        if (first_quote == std::string::npos) continue;
        auto last_quote = line.rfind('"');
        if (last_quote == std::string::npos || last_quote <= first_quote)
          continue;

        std::string path_str =
            line.substr(first_quote + 1, last_quote - first_quote - 1);
        if (path_str.starts_with("$HOME")) {
          path_str.replace(0, 5, home_dir);
        }
        // Modern C++20/23 fix for deprecated u8path
        return fs::path(reinterpret_cast<const char8_t*>(path_str.c_str()));
      }
    }
  }
  return fs::path(home_dir) / "Downloads";
#endif
  return std::nullopt;
}

std::optional<Config> IOManager::load_config(const fs::path& configPath) {
  if (!fs::exists(configPath)) {
    log(std::format("Error: Config file not found at {}", configPath.string()));
    return std::nullopt;
  }
  std::ifstream configFile(configPath);
  try {
    json configJson = json::parse(configFile);
    Config config;
    for (const auto& [category, extensions] :
         configJson["categories"].items()) {
      for (const auto& ext : extensions)
        config.categories[ext.get<std::string>()] = category;
    }
    config.rules = configJson["rules"].get<std::vector<Rule>>();
    std::sort(
        config.rules.begin(), config.rules.end(),
        [](const auto& a, const auto& b) { return a.priority < b.priority; });
    return config;
  } catch (const json::exception& e) {
    log(std::format("Error parsing config.json: {}", e.what()));
    return std::nullopt;
  }
}

void IOManager::run_undo(const fs::path& journalPath) {
  if (!fs::exists(journalPath)) {
    log("No journal file found. Nothing to undo.");
    return;
  }
  std::ifstream journalFile(journalPath);
  json j = json::parse(journalFile);
  auto journal = j.get<std::vector<JournalEntry>>();
  std::reverse(journal.begin(), journal.end());

  log("Starting undo operation...");
  for (const auto& entry : journal) {
    if (entry.action == ActionType::MOVE) {
      log(std::format("Undoing move: '{}' -> '{}'", entry.to.string(),
                      entry.from.string()));
      try {
        if (entry.from.has_parent_path() &&
            !fs::exists(entry.from.parent_path())) {
          fs::create_directories(entry.from.parent_path());
        }
        fs::rename(entry.to, entry.from);
      } catch (const fs::filesystem_error& e) {
        log(std::format("   Error undoing move: {}", e.what()));
      }
    }
  }
  fs::remove(journalPath);
  log("Undo complete. Journal file removed.");
}

void IOManager::save_journal(const fs::path& journalPath,
                             const std::vector<JournalEntry>& journal) {
  if (!journal.empty()) {
    std::ofstream j_file(journalPath);
    j_file << json(journal).dump(2);
    log(std::format("Journal saved with {} actions. Use TUI to undo.",
                    journal.size()));
  }
}