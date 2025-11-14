#include "UI.hpp"

#include <algorithm>
#include <execution>
#include <ftxui/component/component.hpp>
#include <ftxui/dom/elements.hpp>
#include <future>
#include <mutex>
#include <utility>

#include "IOManager.hpp"
#include "utils.hpp"

using namespace ftxui;

UI::UI(const Config& config, const fs::path& targetDir)
    : m_screen(ScreenInteractive::Fullscreen()),
      m_config(config),
      m_targetDir(targetDir),
      m_engine(m_config),
      m_status_text("Ready. Press 'Scan' to begin."),
      m_scan_button_label("  Scan  "),
      m_apply_button_label(" Apply Selected (Enter) ") {
  IOManager::log("Initializing UI components...");

  try {
    m_plan_component =
        Renderer([&] {
          if (m_plan_entries.empty()) {
            return text(m_status_text) | center;
          }
          Elements elements;
          for (size_t i = 0; i < m_plan_entries.size(); ++i) {
            Element entry = text((m_plan_selections[i] ? "[X] " : "[ ] ") +
                                 m_plan_entries[i]);
            if ((int)i == m_selected_action) {
              entry = entry | inverted | focus;
            }
            elements.push_back(entry);
          }
          return vbox(elements) | vscroll_indicator | frame;
        }) |
        CatchEvent([&](Event event) {
          if (m_is_operation_in_progress) {
            return false;
          }
          if (event.is_mouse()) return false;
          if (event == Event::ArrowUp && m_selected_action > 0) {
            m_selected_action--;
          } else if (event == Event::ArrowDown &&
                     m_selected_action < (int)m_plan_entries.size() - 1) {
            m_selected_action++;
          } else if (event == Event::Character(' ') &&
                     !m_plan_selections.empty()) {
            m_plan_selections[m_selected_action] =
                !m_plan_selections[m_selected_action];
          } else if (event == Event::Return && !m_plan.empty()) {
            execute_plan();
          } else {
            return false;
          }
          return true;
        });

    m_log_component = Renderer([&] {
      Elements logs;
      {
        std::scoped_lock lock(m_log_mutex);
        for (const auto& msg : m_log_messages) {
          logs.push_back(text(msg));
        }
      }
      return vbox(logs) | vscroll_indicator | frame | flex;
    });

  } catch (const std::exception& e) {
    IOManager::log(std::format(
        "CRITICAL: Failed to initialize UI components: {}", e.what()));
    throw;
  }
}

void UI::AddLogMessage(std::string_view message) {
  {
    std::scoped_lock lock(m_log_mutex);
    m_log_messages.push_back(std::string(message));
    if (m_log_messages.size() > 100) {
      m_log_messages.pop_front();
    }
  }
  m_screen.Post(Event::Custom);
}

void UI::cleanup_finished_threads() {
  std::erase_if(m_worker_threads,
                [](const std::jthread& t) { return !t.joinable(); });
}

void UI::update_ui_from_plan() {
  m_plan_entries.clear();
  m_plan_selections.clear();
  m_selected_action = 0;
  for (const auto& action : m_plan) {
    m_plan_entries.push_back(std::format(
        "Move '{}' to '{}'", safe_path_to_string(action.from.filename()),
        safe_path_to_string(action.to.parent_path())));
    m_plan_selections.push_back(true);
  }
  if (m_plan.empty()) {
    m_status_text = "Scan complete. No actions proposed.";
  } else {
    m_status_text = std::format(
        "{} actions proposed. Use Arrow Keys and Space to select. Press Enter "
        "to apply.",
        m_plan.size());
  }
}

void UI::execute_plan() {
  if (m_is_operation_in_progress) return;

  std::vector<Action> actions_to_execute;
  for (size_t i = 0; i < m_plan.size(); ++i) {
    if (m_plan_selections[i]) {
      actions_to_execute.push_back(m_plan[i]);
    }
  }

  if (actions_to_execute.empty()) {
    m_status_text = "Nothing selected to apply.";
    return;
  }

  cleanup_finished_threads();
  IOManager::log("Executing plan...");
  m_status_text = "Execution in progress...";
  m_is_operation_in_progress = true;

  m_worker_threads.emplace_back([self = shared_from_this(),
                                 actions = std::move(actions_to_execute)](
                                    const std::stop_token& stoken) {
    try {
      std::mutex journal_mutex;
      std::vector<JournalEntry> final_journal;
      final_journal.reserve(actions.size());

      // Sequential I/O loop to avoid thrashing
      for (const auto& action : actions) {
        if (stoken.stop_requested()) {
          IOManager::log("Execution cancelled by user.");
          break;
        }

        fs::path from_path = action.from;
        fs::path to_path = action.to;
        fs::path parent_dir = to_path.parent_path();

        try {
          if (!fs::exists(parent_dir)) {
            std::error_code ec;
            fs::create_directories(parent_dir, ec);
            if (ec) {
              IOManager::log(
                  std::format("[DIR] Failed to create directory '{}': {}",
                              safe_path_to_string(parent_dir), ec.message()));
              continue;
            }
            IOManager::log(std::format("[DIR] Creating directory: '{}'",
                                       safe_path_to_string(parent_dir)));
          }

          fs::path final_to_path = generate_unique_path(to_path);

          IOManager::log(std::format("Moving '{}' -> '{}'",
                                     safe_path_to_string(from_path),
                                     safe_path_to_string(final_to_path)));
          fs::rename(from_path, final_to_path);

          std::scoped_lock lock(journal_mutex);
          final_journal.push_back({ActionType::MOVE, from_path, final_to_path});

        } catch (const fs::filesystem_error& e) {
          IOManager::log(std::format("ERROR moving file {}: {}",
                                     safe_path_to_string(from_path), e.what()));
        }
      }

      IOManager::save_journal("organizer_journal.json", final_journal);
      IOManager::log("Execution complete.");

      self->m_screen.Post([self] {
        self->m_plan.clear();
        self->update_ui_from_plan();
        self->m_status_text = "Plan executed! Scan again to find more files.";
      });
    } catch (const std::exception& e) {
      IOManager::log(
          std::format("CRITICAL ERROR in execute_plan: {}", e.what()));
      self->m_screen.Post([self, error_msg = std::string(e.what())] {
        self->m_status_text = "Execution failed: " + error_msg;
      });
    } catch (...) {
      IOManager::log("CRITICAL ERROR in execute_plan: Unknown exception");
      self->m_screen.Post(
          [self] { self->m_status_text = "Execution failed: Unknown error"; });
    }
    self->m_is_operation_in_progress = false;
  });
}

void UI::run() {
  try {
    IOManager::set_log_handler(
        [this](std::string_view message) { this->AddLogMessage(message); });

    m_scan_button_label = "  Scan  ";
    m_apply_button_label = " Apply Selected (Enter) ";

    auto scan_button = Button(&m_scan_button_label, [this] {
      if (m_is_operation_in_progress) {
        return;
      }
      m_is_operation_in_progress = true;
      cleanup_finished_threads();
      m_status_text = "Scanning in background... Please wait.";

      m_worker_threads.emplace_back([self = shared_from_this()](
                                        const std::stop_token& stoken) {
        try {
          IOManager::log("Starting scan...");
          std::vector<Action> plan_result =
              self->m_engine.generate_plan(self->m_targetDir, stoken);

          if (stoken.stop_requested()) {
            IOManager::log("Scan was cancelled, UI will not be updated.");
            self->m_screen.Post(
                [self] { self->m_status_text = "Scan cancelled. Ready."; });
            self->m_is_operation_in_progress = false;
            return;
          }

          IOManager::log(std::format("Scan completed. Found {} actions.",
                                     plan_result.size()));

          self->m_screen.Post([self,
                               plan_result = std::move(plan_result)]() mutable {
            try {
              IOManager::log("Updating UI with scan results...");

              self->m_plan = std::move(plan_result);
              self->update_ui_from_plan();

              IOManager::log("UI updated successfully.");
            } catch (const std::exception& e) {
              IOManager::log(std::format("ERROR updating UI: {}", e.what()));
              self->m_status_text =
                  "Error updating UI: " + std::string(e.what());
            }
          });

        } catch (const std::exception& e) {
          IOManager::log(std::format("ERROR during scan: {}", e.what()));
          self->m_screen.Post([self, error_msg = std::string(e.what())] {
            self->m_status_text = "Scan failed: " + error_msg;
          });
        } catch (...) {
          IOManager::log("ERROR during scan: Unknown exception");
          self->m_screen.Post(
              [self] { self->m_status_text = "Scan failed: Unknown error"; });
        }
        self->m_is_operation_in_progress = false;
      });
    });

    auto quit_button = Button("  Quit  ", [this] {
      IOManager::log("Quit requested. Stopping worker threads...");
      for (auto& t : m_worker_threads) {
        t.request_stop();
      }
      m_screen.Exit();
    });

    auto apply_button = Button(&m_apply_button_label, [&] {
      if (m_is_operation_in_progress || m_plan.empty()) {
        return;
      }
      execute_plan();
    });

    auto top_menu = Container::Horizontal({scan_button, quit_button});

    auto main_layout =
        Container::Vertical({top_menu, m_plan_component, apply_button});

    m_main_container = Container::Vertical({
        main_layout,
        m_log_component,
    });

    auto final_renderer = Renderer(m_main_container, [&] {
      bool is_busy = m_is_operation_in_progress;
      bool can_apply = !m_plan.empty() && !is_busy;

      m_scan_button_label = is_busy ? "  Busy...  " : "  Scan  ";
      m_apply_button_label =
          is_busy ? "  Busy...  " : " Apply Selected (Enter) ";

      Element apply_button_element = apply_button->Render();

      if (!can_apply) {
        apply_button_element = apply_button_element | dim;
      }

      auto top_pane = vbox(
          {hbox({text(" Folder Organizer v2.0 ") | bold, filler(),
                 text(safe_path_to_string(m_targetDir))}) |
               color(Color::White) | bgcolor(Color::Blue),
           top_menu->Render(), separator(), m_plan_component->Render() | flex,
           separator(),
           hbox({text(" " + m_status_text), filler(), apply_button_element})});

      auto log_pane =
          vbox({text("Log Output") | bold, m_log_component->Render() | flex});

      return vbox({top_pane | flex_grow, separator(),
                   log_pane | size(HEIGHT, EQUAL, 10)}) |
             border;
    });

    IOManager::log("Starting UI event loop...");
    m_screen.Loop(final_renderer);
    IOManager::log("UI event loop exited. Waiting for threads to join...");

    cleanup_finished_threads();
    m_worker_threads.clear();

    IOManager::log("All threads joined. Exiting.");
    IOManager::set_log_handler(nullptr);

  } catch (const std::exception& e) {
    IOManager::log(
        std::format("CRITICAL: Exception in UI::run(): {}", e.what()));
    throw;
  } catch (...) {
    IOManager::log("CRITICAL: Unknown exception in UI::run()");
    throw;
  }
}