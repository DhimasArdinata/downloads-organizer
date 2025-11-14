# 📂 Downloads Organizer

[![License: GPL v2](https://img.shields.io/badge/License-GPL%20v2-blue.svg)](https://www.gnu.org/licenses/old-licenses/gpl-2.0.html)
[![GitHub release (latest by date)](https://img.shields.io/github/v/release/DhimasArdinata/downloads-organizer)](https://github.com/DhimasArdinata/downloads-organizer/releases)
[![C++23](https://img.shields.io/badge/C%2B%2B-23-blue.svg)](https://isocpp.org/)

A fast, interactive terminal UI for taming your chaotic downloads folder. Stop letting it become a digital junk drawer—`downloads-organizer` uses a flexible rule engine to intelligently sort your files into clean, organized subdirectories.

Built from the ground up in modern C++23 for maximum performance, with a friendly and intuitive Terminal User Interface (TUI) that gives you full control before any files are moved.

## ✨ Key Features

-   **Interactive TUI:** A clean and responsive terminal interface lets you review and select every proposed file operation. No surprises.
-   **Powerful Rule Engine:** Customize how files and folders are sorted using a simple `config.json` file. Rules can be based on file extensions, filenames, directory contents, and even image EXIF data.
-   **Safe by Design:**
    -   **User Confirmation:** No files are moved until you press "Apply".
    -   **No Overwrites:** Automatically renames files to prevent overwriting existing ones (e.g., `image (1).png`).
    -   **Undo Functionality:** Every operation is logged to `organizer_journal.json`, allowing for a potential undo.
-   **Cross-Platform:** Natively supports Windows and Linux.
-   **Modern & Performant:** Written in C++23 for speed and efficiency, capable of scanning thousands of files in seconds.

## 🚀 Getting Started (For Users)

The easiest way to get started is to download the latest pre-compiled release.

1.  Navigate to the [**Releases Page**](https://github.com/YOUR_USERNAME/downloads-organizer/releases).
2.  Download the `.zip` archive for your operating system (e.g., `downloads-organizer-v2.0-windows-x64.zip`).
3.  Extract the contents of the archive to a folder.
4.  Run the executable (`downloads-organizer.exe` on Windows, `./downloads-organizer` on Linux).

## ⌨️ How to Use

Once the application is running, the TUI is simple to navigate:

-   **`Scan`**: Press the "Scan" button (or use `Tab` and `Enter`) to analyze your Downloads folder.
-   **`Arrow Keys`**: Navigate up and down the list of proposed actions.
-   **`Spacebar`**: Toggle whether an action is selected or deselected.
-   **`Enter`**: Executes all *selected* actions.
-   **`Quit`**: Exits the application. Your background tasks will be safely cancelled.

## ⚙️ Configuration

The power of `downloads-organizer` comes from the `config.json` file, which must be in the same directory as the executable.

The configuration has two main parts: `categories` and `rules`.

### 1. Categories

This is a simple map of a file extension to a category name. This is the primary method for sorting individual files.

```json
{
  "categories": {
    "Images": [".jpg", ".jpeg", ".png", ".gif"],
    "Documents": [".pdf", ".doc", ".docx", ".txt"],
    "Archives": [".zip", ".rar", ".7z"]
  },
  "rules": [ ... ]
}
```

### 2. Rules

Rules are used for more complex logic, primarily for sorting **directories**. They are processed in order of `priority` (lower numbers run first). A directory is sorted by the first rule it matches.

A rule consists of a `category`, a `priority`, and a list of `conditions`. All conditions in a rule must be met for the rule to trigger.

**Example Rule:** A folder containing a `package.json` file should be moved to the "Projects" category.

```json
{
  "rules": [
    {
      "category": "Projects",
      "priority": 2,
      "conditions": [
        {
          "type": "contains_filename",
          "values": ["package.json"]
        }
      ]
    }
  ]
}
```

**Available Condition Types:**

| Type                            | Description                                                                                              | Example `values`                      |
| ------------------------------- | -------------------------------------------------------------------------------------------------------- | ------------------------------------- |
| `contains_filename`             | Matches if the directory contains a file with an exact, case-sensitive name.                             | `["Makefile", "index.html"]`          |
| `contains_filename_pattern`     | Matches if the directory contains a file with a case-insensitive name.                                   | `["setup.exe", "installer.exe"]`      |
| `contains_subdirectory_named`   | Matches if the directory has a subfolder with a case-insensitive name.                                   | `["fonts", "assets"]`                 |
| `has_no_subdirectories`         | Matches if the directory contains files but no subfolders.                                               | (values not used)                     |
| `file_category_percentage`      | Matches if a percentage of files belong to a certain category. Requires a `threshold` value (0.0 to 1.0). | `["Images"]` (with `threshold: 0.8`)  |
| `subfolder_matches_archive`     | Matches if a subfolder name matches the name of an archive file in the same directory (e.g., `app/` and `app.zip`). | (values not used)                     |
| `exif_date_matches`             | For **image files only**. Matches a date pattern (`YYYY:MM:DD`). `*` is a wildcard. The category name can contain `{exif_year}`. | `["2023:*:*"]`                        |

## 🛠️ Building From Source

If you want to build the project yourself, you'll need:

-   A C++23 compatible compiler (MSVC 2022, GCC 13+, Clang 16+)
-   CMake (3.20 or higher)
-   [vcpkg](https://github.com/microsoft/vcpkg) for dependency management

#### 1. Install Dependencies

```bash
vcpkg install nlohmann-json exiv2 ftxui gtest
```

#### 2. Configure and Build

**Windows (Visual Studio):**
```bash
cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE=[path/to/vcpkg]/scripts/buildsystems/vcpkg.cmake
cmake --build build --config Release
```

**Linux:**
```bash
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE=[path/to/vcpkg]/scripts/buildsystems/vcpkg.cmake
cmake --build build
```

The final executable will be located in the `build/Release` (Windows) or `build` (Linux) directory.

## 📜 License

This project is licensed under the **GPL-2.0-or-later** license, primarily due to its dependency on the Exiv2 library. See the `LICENSE.txt` file for full details.

## 🙏 Acknowledgments

This project would not be possible without these fantastic open-source libraries:

-   [**FTXUI**](https://github.com/ArthurSonzogni/FTXUI) for the terminal user interface.
-   [**nlohmann/json**](https://github.com/nlohmann/json) for effortless JSON parsing.
-   [**Exiv2**](https://github.com/Exiv2/exiv2) for reading image metadata.
