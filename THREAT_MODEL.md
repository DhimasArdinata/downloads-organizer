# Threat Model for Folder Organizer

This document outlines the potential security threats to the Folder Organizer application and the mitigations in place.

## 1. Assets

- **User Files:** The primary asset is the user's files located in the target directory (e.g., `Downloads`).
- **System Integrity:** The stability of the user's machine.

## 2. Attack Surface

- **File System:** The application reads files and directory structures. This is the primary vector for attacks.
- **Configuration File:** The `config.json` file is parsed.
- **Dependencies:** Third-party libraries (`Exiv2`, `nlohmann_json`, `ftxui`) process external data.

## 3. Potential Threats & Mitigations

### Threat: Denial of Service (DoS)

- **Scenario 1: Malformed Image File.** An attacker crafts a corrupt image file (e.g., `.jpg`, `.cr2`) that causes a crash or infinite loop in the `Exiv2` library when parsing metadata.

  - **Mitigation:** All `Exiv2` calls are wrapped in a `try...catch` block to handle exceptions gracefully. The `g_exiv2_mutex` prevents thread-related corruption within the library. The application will log the error and skip the problematic file.

- **Scenario 2: Filesystem Race Conditions.** Multiple threads performing string conversions on file paths could cause a crash due to non-thread-safe C-locale dependencies.

  - **Mitigation:** **(FIXED)** All parallel code now uses thread-safe methods (`fs::path::native()`, `fs::path::generic_u8string()`) for path manipulation and string conversion, eliminating this race condition.

- **Scenario 3: Recursive Directory Structure.** An attacker creates a deeply nested or cyclically linked directory structure to cause a stack overflow or infinite loop.
  - **Mitigation:** The application uses `fs::directory_iterator`, which is a non-recursive iterator, preventing this category of attack.

### Threat: Information Disclosure

- **Scenario: Incorrect File Categorization.** A bug in the rule engine causes a sensitive personal file to be moved to an incorrect or less private location (though still within the target directory).
  - **Mitigation:** The application is a TUI that requires user confirmation before any action is taken. The user can review and deselect any proposed move, providing a manual check and final control.

### Threat: Elevation of Privilege

- **Scenario: Path Traversal.** An attacker crafts a file or category name like `../../../../Windows/System32/malicious.dll` to trick the application into writing files outside the intended target directory.
  - **Mitigation:** The application uses `std::filesystem`, which correctly composes paths. The destination path is always constructed as `targetDir / categoryName / filename`, which prevents traversal attacks. All file operations remain within the user's own permission context.
