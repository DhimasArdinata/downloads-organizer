#pragma once

#include <filesystem>
#include <format>
#include <string>

namespace fs = std::filesystem;

// A central, thread-safe utility to convert a std::filesystem::path to a
// UTF-8 encoded std::string, suitable for logging and display.
inline std::string safe_path_to_string(const fs::path& p) {
  // path::u8string() is locale-independent and returns a UTF-8 encoded string.
  // On C++20/23, this returns a std::u8string, which needs to be converted.
  auto u8str = p.u8string();
  return std::string(reinterpret_cast<const char*>(u8str.c_str()),
                     u8str.length());
}

// A simple, locale-independent function to convert a string to lowercase.
// It only handles basic ASCII characters, which is safe and sufficient for
// things like file extensions and common keywords.
inline std::string string_to_lower_ascii(std::string_view sv) {
  std::string result;
  result.reserve(sv.length());
  for (char c : sv) {
    if (c >= 'A' && c <= 'Z') {
      result += static_cast<char>(c + ('a' - 'A'));
    } else {
      result += c;
    }
  }
  return result;
}

// Generates a unique path by appending a number (e.g., "file (1).txt")
// if the target path already exists. This prevents overwriting files.
inline fs::path generate_unique_path(const fs::path& target_path) {
  if (!fs::exists(target_path)) {
    return target_path;
  }

  const fs::path parent_dir = target_path.parent_path();
  const std::string stem_str = safe_path_to_string(target_path.stem());
  const fs::path ext = target_path.extension();

  int counter = 1;
  fs::path new_path;
  do {
    // Construct the new filename from UTF-8 strings
    const std::string new_filename_str =
        std::format("{} ({}){}", stem_str, counter++, safe_path_to_string(ext));
    // Modern C++20/23 way to construct UTF-8 path: cast to char8_t*
    new_path =
        parent_dir /
        fs::path(reinterpret_cast<const char8_t*>(new_filename_str.c_str()));
  } while (fs::exists(new_path));

  return new_path;
}