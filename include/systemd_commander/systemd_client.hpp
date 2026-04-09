#ifndef SYSTEMD_COMMANDER__SYSTEMD_CLIENT_HPP_
#define SYSTEMD_COMMANDER__SYSTEMD_CLIENT_HPP_

#include <map>
#include <string>
#include <vector>

namespace systemd_commander {

struct SystemdUnitSummary {
  std::string name;
  std::string load_state;
  std::string active_state;
  std::string sub_state;
  std::string description;
};

struct SystemdUnitDetails {
  std::map<std::string, std::string> properties;
  bool can_start{false};
  bool can_stop{false};
  bool can_reload{false};
};

std::vector<SystemdUnitSummary> parse_systemd_list_units_output(const std::string & text);
SystemdUnitDetails parse_systemd_show_output(const std::string & text);

class SystemdClient {
public:
  std::vector<SystemdUnitSummary> list_service_units(std::string * error = nullptr) const;
  bool show_unit_details(
    const std::string & unit_name,
    SystemdUnitDetails & details,
    std::string * error = nullptr) const;
  bool execute_unit_action(
    const std::string & unit_name,
    const std::string & action,
    std::string * error = nullptr) const;
};

}  // namespace systemd_commander

#endif  // SYSTEMD_COMMANDER__SYSTEMD_CLIENT_HPP_
