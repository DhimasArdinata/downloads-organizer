#include "RuleEngine.hpp"

#include <atomic>
#include <execution>
#include <exiv2/exiv2.hpp>
#include <mutex>
#include <stdexcept>

#include "IOManager.hpp"
#include "utils.hpp"

namespace {
std::mutex g_exiv2_mutex;
}

RuleEngine::RuleEngine(const Config& config) : m_config(config) {}

std::vector<Action> RuleEngine::generate_plan(
    const fs::path& targetDir, std::optional<std::stop_token> stoken) const {
  IOManager::log("Scanning directory for items to process...");

  std::vector<fs::path> paths_to_scan;
  try {
    for (const auto& entry : fs::directory_iterator(
             targetDir, fs::directory_options::skip_permission_denied)) {
      paths_to_scan.push_back(entry.path());
    }
  } catch (const fs::filesystem_error& e) {
    IOManager::log(std::format(
        "Error during initial directory scan of '{}': {}. Aborting.",
        safe_path_to_string(targetDir), e.what()));
    return {};
  }

  IOManager::log(
      std::format("Found {} items. Analyzing...", paths_to_scan.size()));

  std::vector<Action> result_plan;
  std::mutex result_mutex;

  const size_t chunk_size = 128;
  for (size_t i = 0; i < paths_to_scan.size(); i += chunk_size) {
    if (stoken && stoken->stop_requested()) {
      IOManager::log("Scan cancelled by user.");
      break;
    }

    auto first = paths_to_scan.begin() + i;
    auto last =
        paths_to_scan.begin() + std::min(i + chunk_size, paths_to_scan.size());

    std::vector<Action> chunk_plan;
    std::mutex chunk_mutex;

    std::for_each(std::execution::par, first, last, [&](const fs::path& p) {
      if (auto action = generate_action_for_path(p, targetDir)) {
        std::scoped_lock lock(chunk_mutex);
        chunk_plan.push_back(*action);
      }
    });

    if (!chunk_plan.empty()) {
      std::scoped_lock lock(result_mutex);
      result_plan.insert(result_plan.end(),
                         std::make_move_iterator(chunk_plan.begin()),
                         std::make_move_iterator(chunk_plan.end()));
    }
  }

  if (stoken && stoken->stop_requested()) {
    IOManager::log(
        std::format("Analysis cancelled. {} actions found before stop.",
                    result_plan.size()));
  } else {
    IOManager::log(std::format("Analysis complete. Found {} actions.",
                               result_plan.size()));
  }

  return result_plan;
}

std::optional<std::string> RuleEngine::get_exif_date(
    const fs::path& path) const {
  std::scoped_lock lock(g_exiv2_mutex);

  try {
    Exiv2::Image::UniquePtr image =
        Exiv2::ImageFactory::open(safe_path_to_string(path));
    if (!image.get()) return std::nullopt;
    image->readMetadata();
    auto& exifData = image->exifData();
    if (exifData.empty()) return std::nullopt;

    auto datum = exifData["Exif.Photo.DateTimeOriginal"];
    if (datum.count() > 0) {
      std::string date_str = datum.toString();
      if (date_str.length() >= 10) {
        return date_str.substr(0, 10);
      }
    }
  } catch (const Exiv2::Error& e) {
    IOManager::log(std::format("Non-critical Exiv2 error reading '{}': {}",
                               safe_path_to_string(path), e.what()));
  } catch (const std::exception& e) {
    IOManager::log(
        std::format("Non-critical standard exception reading '{}': {}",
                    safe_path_to_string(path), e.what()));
  }
  return std::nullopt;
}

std::optional<Action> RuleEngine::generate_action_for_path(
    const fs::path& path, const fs::path& targetDir) const {
  try {
    const std::string defaultCategory = "Other";
    std::string categoryName;
    fs::path path_to_analyze = path;

    if (fs::is_regular_file(path)) {
      std::string ext = safe_path_to_string(path.extension());
      std::string ext_lower = string_to_lower_ascii(ext);

      static const std::vector<std::string> image_extensions = {
          ".jpg", ".jpeg", ".png", ".webp", ".tiff",
          ".raw", ".cr2",  ".nef", ".arw",  ".dng"};

      bool is_image = false;
      for (const auto& img_ext : image_extensions) {
        if (ext_lower == img_ext) {
          is_image = true;
          break;
        }
      }

      std::optional<std::string> exif_date;
      if (is_image) {
        exif_date = get_exif_date(path);
      }

      if (exif_date) {
        for (const auto& rule : m_config.rules) {
          for (const auto& cond : rule.conditions) {
            if (cond.type == "exif_date_matches") {
              if (cond.values.empty()) continue;
              bool match = true;
              std::string_view pattern = cond.values[0];
              if (pattern.length() == 10 && exif_date->length() == 10) {
                for (int i = 0; i < 10; ++i) {
                  if (pattern[i] != '*' && pattern[i] != (*exif_date)[i]) {
                    match = false;
                    break;
                  }
                }
              } else {
                match = false;
              }

              if (match) {
                categoryName = rule.category;
                std::string year = exif_date->substr(0, 4);
                size_t pos = categoryName.find("{exif_year}");
                if (pos != std::string::npos)
                  categoryName.replace(pos, 11, year);
                break;
              }
            }
          }
          if (!categoryName.empty()) break;
        }
      }

      if (categoryName.empty()) {
        if (auto it = m_config.categories.find(ext_lower);
            it != m_config.categories.end()) {
          categoryName = it->second;
        } else {
          categoryName = defaultCategory;
        }
      }
    } else if (fs::is_directory(path)) {
      bool isCategoryDir = false;
      for (const auto& pair : m_config.categories) {
        if (path.filename() == pair.second) {
          isCategoryDir = true;
          break;
        }
      }
      if (isCategoryDir || path.filename() == defaultCategory)
        return std::nullopt;
      fs::directory_iterator it_check(path_to_analyze);
      if (it_check != fs::directory_iterator{}) {
        const auto& first_entry = *it_check;
        ++it_check;
        if (it_check == fs::directory_iterator{} &&
            first_entry.is_directory()) {
          path_to_analyze = first_entry.path();
        }
      }
      std::vector<std::string> filenames, subdir_names;
      std::unordered_map<std::string, int> category_counts;
      for (const auto& entry : fs::directory_iterator(path_to_analyze)) {
        if (entry.is_directory())
          subdir_names.push_back(safe_path_to_string(entry.path().filename()));
        else if (entry.is_regular_file()) {
          filenames.push_back(safe_path_to_string(entry.path().filename()));
          const std::string ext = string_to_lower_ascii(
              safe_path_to_string(entry.path().extension()));
          if (auto it_cat = m_config.categories.find(ext);
              it_cat != m_config.categories.end())
            category_counts[it_cat->second]++;
        }
      }
      for (const auto& rule : m_config.rules) {
        bool all_conditions_met = true;
        for (const auto& cond : rule.conditions) {
          bool condition_met = false;
          if (cond.type == "contains_filename_pattern") {
            for (const auto& p : cond.values)
              for (const auto& f : filenames)
                if (string_to_lower_ascii(f) == string_to_lower_ascii(p)) {
                  condition_met = true;
                  break;
                }
          } else if (cond.type == "contains_filename") {
            for (const auto& f_find : cond.values)
              for (const auto& f : filenames)
                if (f == f_find) {
                  condition_met = true;
                  break;
                }
          } else if (cond.type == "contains_subdirectory_named") {
            for (const auto& s_find : cond.values)
              for (const auto& s : subdir_names)
                if (string_to_lower_ascii(s) == string_to_lower_ascii(s_find)) {
                  condition_met = true;
                  break;
                }
          } else if (cond.type == "has_no_subdirectories") {
            condition_met = subdir_names.empty();
          } else if (cond.type == "file_category_percentage") {
            int total = 0;
            int category_total = 0;
            for (const auto& pair : category_counts) total += pair.second;
            for (const auto& cat_name : cond.values)
              if (category_counts.count(cat_name))
                category_total += category_counts.at(cat_name);
            if (total > 0 &&
                (static_cast<double>(category_total) / total >= cond.threshold))
              condition_met = true;
          } else if (cond.type == "subfolder_matches_archive") {
            for (const auto& subdir_name : subdir_names) {
              std::string lower_subdir_name =
                  string_to_lower_ascii(subdir_name);
              for (const auto& filename : filenames) {
                std::string lower_filename = string_to_lower_ascii(filename);
                if (lower_filename == lower_subdir_name + ".zip" ||
                    lower_filename == lower_subdir_name + ".rar" ||
                    lower_filename == lower_subdir_name + ".7z") {
                  condition_met = true;
                  break;
                }
              }
              if (condition_met) break;
            }
          }
          if (!condition_met) {
            all_conditions_met = false;
            break;
          }
        }
        if (all_conditions_met) {
          categoryName = rule.category;
          break;
        }
      }
    }

    if (!categoryName.empty()) {
      fs::path destPath = targetDir / categoryName / path.filename();
      if (destPath != path) {
        return Action{path, destPath, categoryName};
      }
    }
  } catch (const fs::filesystem_error& e) {
    IOManager::log(
        std::format("Warning: Filesystem error processing '{}': {}. Skipping.",
                    safe_path_to_string(e.path1()), e.what()));
  } catch (...) {
    IOManager::log(
        std::format("Warning: A critical low-level error occurred while "
                    "processing '{}'. Skipping.",
                    safe_path_to_string(path)));
  }

  return std::nullopt;
}