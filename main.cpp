#include <exception>
#include <exiv2/exiv2.hpp>
#include <iostream>
#include <memory>
#include <print>
#include <vector>

#include "IOManager.hpp"
#include "UI.hpp"
#include "types.hpp"

namespace fs = std::filesystem;

void pause_console() {
#ifdef _WIN32
  std::println("Press any key to exit...");
  system("pause >nul");
#else
  std::println("Press Enter to exit...");
  std::cin.get();
#endif
}

int main(int argc, char* argv[]) {
  Exiv2::XmpParser::initialize();

  try {
    IOManager::initialize_logger();
    IOManager::log("--- Organizer v2.0 Started ---");

    fs::path exePath;
    if (argc > 0) {
      exePath = fs::path(argv[0]).parent_path();
    } else {
      exePath = fs::current_path();
    }

    if (exePath.empty()) {
      exePath = fs::current_path();
    }

    IOManager::log(std::format("Executable directory: {}", exePath.string()));
    IOManager::log(std::format("Current working directory: {}",
                               fs::current_path().string()));

    std::vector<fs::path> configPaths = {exePath / "config.json",
                                         fs::current_path() / "config.json",
                                         exePath.parent_path() / "config.json"};

    std::optional<Config> configOpt;
    fs::path usedConfigPath;

    for (const auto& configPath : configPaths) {
      IOManager::log(
          std::format("Trying config path: {}", configPath.string()));
      if (fs::exists(configPath)) {
        IOManager::log(
            std::format("Found config.json at: {}", configPath.string()));
        configOpt = IOManager::load_config(configPath);
        if (configOpt) {
          usedConfigPath = configPath;
          break;
        }
      }
    }

    if (!configOpt) {
      IOManager::log("CRITICAL: Failed to load configuration from all paths.");
      std::println(stderr, "\n=== ERROR ===");
      std::println(stderr, "Failed to load config.json!\n");
      std::println(stderr, "Searched in:");
      for (const auto& path : configPaths) {
        std::println(stderr, "  - {}", path.string());
      }
      std::println(
          stderr,
          "\nPlease ensure config.json exists in one of these locations.");
      std::println(stderr, "Check organizer.log for details.");
      pause_console();
      Exiv2::XmpParser::terminate();
      return 1;
    }
    IOManager::log(std::format("Configuration loaded successfully from: {}",
                               usedConfigPath.string()));

    auto targetDirOpt = IOManager::get_downloads_folder_path();
    if (!targetDirOpt) {
      IOManager::log("CRITICAL: Could not find Downloads folder.");
      std::println(stderr, "\n=== ERROR ===");
      std::println(stderr, "Could not locate Downloads folder!");
      std::println(stderr, "Check organizer.log for details.");
      pause_console();
      Exiv2::XmpParser::terminate();
      return 1;
    }
    IOManager::log(std::format("Target directory: {}", targetDirOpt->string()));

    if (!fs::exists(*targetDirOpt)) {
      IOManager::log("CRITICAL: Target directory does not exist.");
      std::println(stderr, "\n=== ERROR ===");
      std::println(stderr, "Downloads folder does not exist: {}",
                   targetDirOpt->string());
      pause_console();
      Exiv2::XmpParser::terminate();
      return 1;
    }

    IOManager::log("Initializing UI...");
    auto application = std::make_shared<UI>(*configOpt, *targetDirOpt);
    application->run();

    IOManager::log("--- Organizer Exited Normally ---");

    Exiv2::XmpParser::terminate();
    return 0;

  } catch (const std::exception& e) {
    IOManager::log(std::format("FATAL EXCEPTION: {}", e.what()));
    std::println(stderr, "\n=== FATAL ERROR ===");
    std::println(stderr, "Exception: {}", e.what());
    std::println(stderr, "Check organizer.log for details.");
    pause_console();
    Exiv2::XmpParser::terminate();
    return 1;
  } catch (...) {
    IOManager::log("FATAL: Unknown exception occurred.");
    std::println(stderr, "\n=== FATAL ERROR ===");
    std::println(stderr, "Unknown exception occurred!");
    std::println(stderr, "Check organizer.log for details.");
    pause_console();
    Exiv2::XmpParser::terminate();
    return 1;
  }
}