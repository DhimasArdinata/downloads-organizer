# Software Bill of Materials (SBOM) for Folder Organizer

This document lists the third-party software components used in this project. All dependencies are managed and acquired via the [vcpkg](https://github.com/microsoft/vcpkg) package manager.

## 1. nlohmann/json

- **Purpose:** High-performance JSON parsing and serialization for `config.json` and the undo journal.
- **vcpkg Port:** `nlohmann-json`
- **License:** MIT License
- **Origin:** [https://github.com/nlohmann/json](https://github.com/nlohmann/json)

## 2. Exiv2

- **Purpose:** Reading and writing image metadata (EXIF, XMP, etc.) for sorting photos by date.
- **vcpkg Port:** `exiv2`
- **License:** GPL-2.0-or-later
- **Origin:** [https://github.com/Exiv2/exiv2](https://github.com/Exiv2/exiv2)

## 3. FTXUI (Flexible TUI)

- **Purpose:** A functional library for creating interactive Terminal User Interfaces (TUI).
- **vcpkg Port:** `ftxui`
- **License:** MIT License
- **Origin:** [https://github.com/ArthurSonzogni/FTXUI](https://github.com/ArthurSonzogni/FTXUI)

## 4. GoogleTest (Build/Test Dependency)

- **Purpose:** A C++ testing framework for writing unit and regression tests.
- **vcpkg Port:** `gtest`
- **License:** BSD 3-Clause "New" or "Revised" License
- **Origin:** [https://github.com/google/googletest](https://github.com/google/googletest)
