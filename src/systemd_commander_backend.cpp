#include "systemd_commander/systemd_commander.hpp"

#include <algorithm>
#include <map>

namespace systemd_commander {

namespace {

void merge_unit_file_states(
  std::vector<SystemdUnitSummary> & units,
  const std::vector<SystemdUnitFileSummary> & unit_files)
{
  std::map<std::string, std::size_t> indexes;
  for (std::size_t index = 0; index < units.size(); ++index) {
    indexes[units[index].name] = index;
  }

  for (const auto & unit_file : unit_files) {
    if (unit_file.name.empty()) {
      continue;
    }

    const auto found = indexes.find(unit_file.name);
    if (found != indexes.end()) {
      units[found->second].unit_file_state = unit_file.unit_file_state;
      continue;
    }

    SystemdUnitSummary unit;
    unit.name = unit_file.name;
    unit.load_state = "unit-file";
    unit.active_state = "inactive";
    unit.sub_state = "dead";
    unit.unit_file_state = unit_file.unit_file_state;
    units.push_back(std::move(unit));
    indexes[units.back().name] = units.size() - 1;
  }

  std::sort(
    units.begin(), units.end(),
    [](const SystemdUnitSummary & lhs, const SystemdUnitSummary & rhs) {
      return lhs.name < rhs.name;
    });
}

int find_unit_index(const std::vector<SystemdUnitSummary> & units, const std::string & unit_name) {
  const auto found = std::find_if(
    units.begin(), units.end(),
    [&unit_name](const SystemdUnitSummary & unit) {
      return unit.name == unit_name;
    });
  if (found == units.end()) {
    return -1;
  }
  return static_cast<int>(std::distance(units.begin(), found));
}

}  // namespace

SystemdCommanderBackend::SystemdCommanderBackend(const std::string & initial_unit)
: initial_unit_(initial_unit) {
  std::string theme_error;
  (void)tui::load_theme_from_file(tui::default_theme_config_path(), &theme_error);
  refresh_units();
}

SystemdCommanderBackend::~SystemdCommanderBackend() {
  if (unit_file_refresh_thread_.joinable()) {
    unit_file_refresh_thread_.join();
  }
}

void SystemdCommanderBackend::refresh_units() {
  std::string previous_selection;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!units_.empty() && selected_index_ >= 0 && selected_index_ < static_cast<int>(units_.size())) {
      previous_selection = units_[static_cast<std::size_t>(selected_index_)].name;
    }
  }

  std::string error;
  std::vector<SystemdUnitSummary> units = client_.list_service_units(&error);

  {
    std::lock_guard<std::mutex> lock(mutex_);
    if (units.empty() && !error.empty()) {
      units_.clear();
      selected_unit_details_ = SystemdUnitDetails{};
      selected_details_unit_.clear();
      selected_index_ = 0;
      unit_scroll_ = 0;
      status_line_ = error;
      return;
    }

    merge_unit_file_states(units, unit_files_);
    units_ = std::move(units);
    const std::string preferred_selection = !initial_unit_.empty() ? initial_unit_ : previous_selection;
    if (!preferred_selection.empty()) {
      const int found_index = find_unit_index(units_, preferred_selection);
      if (found_index >= 0) {
        selected_index_ = found_index;
      }
    }

    clamp_selection();
    if (units_.empty()) {
      selected_unit_details_ = SystemdUnitDetails{};
      selected_details_unit_.clear();
      status_line_ = "No systemd service units were reported.";
      return;
    }

    initial_unit_.clear();
    status_line_ = "Loaded " + std::to_string(units_.size()) + " service units.";
  }

  refresh_selected_unit_details();
}

void SystemdCommanderBackend::refresh_unit_file_states_async() {
  {
    std::lock_guard<std::mutex> lock(mutex_);
    if (unit_file_refresh_running_) {
      return;
    }
  }

  if (unit_file_refresh_thread_.joinable()) {
    unit_file_refresh_thread_.join();
  }

  {
    std::lock_guard<std::mutex> lock(mutex_);
    if (unit_file_refresh_running_) {
      return;
    }
    unit_file_refresh_running_ = true;
  }

  unit_file_refresh_thread_ = std::thread(
    [this]() {
      std::string error;
      std::vector<SystemdUnitFileSummary> unit_files = client_.list_service_unit_files(&error);

      std::lock_guard<std::mutex> lock(mutex_);
      unit_file_refresh_running_ = false;
      if (unit_files.empty() && !error.empty()) {
        status_line_ = error;
        return;
      }

      std::string selected_unit;
      if (!units_.empty() && selected_index_ >= 0 && selected_index_ < static_cast<int>(units_.size())) {
        selected_unit = units_[static_cast<std::size_t>(selected_index_)].name;
      }

      unit_files_ = std::move(unit_files);
      merge_unit_file_states(units_, unit_files_);
      if (!selected_unit.empty()) {
        const int found_index = find_unit_index(units_, selected_unit);
        if (found_index >= 0) {
          selected_index_ = found_index;
        }
      }
      clamp_selection();
      status_line_ =
        "Loaded " + std::to_string(units_.size()) + " service units with unit-file states.";
    });
}

void SystemdCommanderBackend::refresh_selected_unit_details() {
  std::string unit_name;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    if (units_.empty() || selected_index_ < 0 || selected_index_ >= static_cast<int>(units_.size())) {
      selected_unit_details_ = SystemdUnitDetails{};
      selected_details_unit_.clear();
      return;
    }
    unit_name = units_[static_cast<std::size_t>(selected_index_)].name;
  }

  SystemdUnitDetails details;
  std::string error;
  const bool ok = client_.show_unit_details(unit_name, details, &error);

  std::lock_guard<std::mutex> lock(mutex_);
  if (units_.empty() || selected_index_ < 0 || selected_index_ >= static_cast<int>(units_.size())) {
    return;
  }
  if (units_[static_cast<std::size_t>(selected_index_)].name != unit_name) {
    return;
  }

  if (!ok) {
    selected_unit_details_ = SystemdUnitDetails{};
    selected_details_unit_.clear();
    status_line_ = error.empty() ? ("Failed to inspect " + unit_name + ".") : error;
    return;
  }

  selected_unit_details_ = std::move(details);
  selected_details_unit_ = unit_name;
  auto & unit = units_[static_cast<std::size_t>(selected_index_)];
  const auto update_field = [&](const std::string & property, std::string & field) {
    const auto found = selected_unit_details_.properties.find(property);
    if (found != selected_unit_details_.properties.end() && !found->second.empty()) {
      field = found->second;
    }
  };
  update_field("Description", unit.description);
  update_field("LoadState", unit.load_state);
  update_field("ActiveState", unit.active_state);
  update_field("SubState", unit.sub_state);
  update_field("UnitFileState", unit.unit_file_state);
  status_line_ = "Loaded details for " + unit_name + ".";
}

void SystemdCommanderBackend::clamp_selection() {
  if (units_.empty()) {
    selected_index_ = 0;
    unit_scroll_ = 0;
    return;
  }

  selected_index_ = std::clamp(selected_index_, 0, static_cast<int>(units_.size()) - 1);
  unit_scroll_ = std::max(0, std::min(unit_scroll_, selected_index_));
}

std::string SystemdCommanderBackend::selected_unit_name() const {
  std::lock_guard<std::mutex> lock(mutex_);
  if (units_.empty() || selected_index_ < 0 || selected_index_ >= static_cast<int>(units_.size())) {
    return "";
  }
  return units_[static_cast<std::size_t>(selected_index_)].name;
}

std::string SystemdCommanderBackend::selected_fragment_path() const {
  std::lock_guard<std::mutex> lock(mutex_);
  if (units_.empty() || selected_index_ < 0 || selected_index_ >= static_cast<int>(units_.size())) {
    return "";
  }

  const auto & unit = units_[static_cast<std::size_t>(selected_index_)];
  if (selected_details_unit_ != unit.name) {
    return "";
  }

  const auto found = selected_unit_details_.properties.find("FragmentPath");
  if (found == selected_unit_details_.properties.end()) {
    return "";
  }
  return found->second;
}

std::string SystemdCommanderBackend::selected_log_namespace() const {
  std::lock_guard<std::mutex> lock(mutex_);
  if (units_.empty() || selected_index_ < 0 || selected_index_ >= static_cast<int>(units_.size())) {
    return "";
  }

  const auto & unit = units_[static_cast<std::size_t>(selected_index_)];
  if (selected_details_unit_ != unit.name) {
    return "";
  }

  const auto found = selected_unit_details_.properties.find("LogNamespace");
  if (found == selected_unit_details_.properties.end()) {
    return "";
  }
  return found->second;
}

std::vector<SystemdDetailRow> SystemdCommanderBackend::detail_rows_snapshot() const {
  std::vector<SystemdDetailRow> rows;

  std::lock_guard<std::mutex> lock(mutex_);
  if (units_.empty()) {
    rows.push_back({"No services available.", false});
    return rows;
  }

  const auto & unit = units_[static_cast<std::size_t>(selected_index_)];
  rows.push_back({"Service", true});
  rows.push_back({"Name: " + unit.name, false});
  rows.push_back({"Description: " + unit.description, false});
  rows.push_back({"Load: " + unit.load_state, false});
  rows.push_back({"Active: " + unit.active_state, false});
  rows.push_back({"Sub: " + unit.sub_state, false});
  if (!unit.unit_file_state.empty()) {
    rows.push_back({"UnitFileState: " + unit.unit_file_state, false});
  }

  if (selected_details_unit_ != unit.name || selected_unit_details_.properties.empty()) {
    rows.push_back({"Details", true});
    rows.push_back({"No detailed properties loaded.", false});
    return rows;
  }

  rows.push_back({"Properties", true});
  const char * property_keys[] = {
    "Id",
    "Description",
    "LoadState",
    "ActiveState",
    "SubState",
    "UnitFileState",
    "MainPID",
    "ExecMainPID",
    "FragmentPath",
    "ExecStart",
    "LogNamespace",
  };
  for (const char * key : property_keys) {
    const auto found = selected_unit_details_.properties.find(key);
    if (found != selected_unit_details_.properties.end() && !found->second.empty()) {
      rows.push_back({std::string(key) + ": " + found->second, false});
    }
  }

  rows.push_back({"Actions", true});
  rows.push_back(
    {
      "start=" + std::string(selected_unit_details_.can_start ? "yes" : "no") +
      "  stop=" + std::string(selected_unit_details_.can_stop ? "yes" : "no") +
      "  reload=" + std::string(selected_unit_details_.can_reload ? "yes" : "no"),
      false});
  return rows;
}

bool SystemdCommanderBackend::perform_action(const std::string & action) {
  const std::string unit_name = selected_unit_name();
  if (unit_name.empty()) {
    std::lock_guard<std::mutex> lock(mutex_);
    status_line_ = "No service selected.";
    return false;
  }

  std::string error;
  if (!client_.execute_unit_action(unit_name, action, &error)) {
    std::lock_guard<std::mutex> lock(mutex_);
    status_line_ = error.empty() ? ("Failed to " + action + " " + unit_name + ".") : error;
    return false;
  }

  refresh_units();
  std::lock_guard<std::mutex> lock(mutex_);
  status_line_ = "Ran `" + action + "` for " + unit_name + ".";
  return true;
}

}  // namespace systemd_commander
