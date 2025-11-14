#pragma once

#include <atomic>
#include <deque>
#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <memory>
#include <mutex>
#include <stop_token>
#include <string>
#include <thread>
#include <vector>

#include "RuleEngine.hpp"

class UI : public std::enable_shared_from_this<UI> {
 public:
  UI(const Config& config, const fs::path& targetDir);
  void run();

 private:
  void execute_plan();
  void update_ui_from_plan();
  void cleanup_finished_threads();

  void AddLogMessage(std::string_view message);
  std::mutex m_log_mutex;
  std::deque<std::string> m_log_messages;

  ftxui::ScreenInteractive m_screen;
  Config m_config;
  const fs::path m_targetDir;
  RuleEngine m_engine;

  std::vector<Action> m_plan;
  std::vector<std::string> m_plan_entries;
  std::vector<bool> m_plan_selections;
  std::string m_status_text;
  int m_selected_action = 0;
  std::vector<std::jthread> m_worker_threads;
  std::atomic<bool> m_is_operation_in_progress = false;

  std::string m_scan_button_label;
  std::string m_apply_button_label;

  ftxui::Component m_plan_component;
  ftxui::Component m_log_component;
  ftxui::Component m_main_container;
};