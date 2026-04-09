#ifndef SYSTEMD_COMMANDER__PROCESS_RUNNER_HPP_
#define SYSTEMD_COMMANDER__PROCESS_RUNNER_HPP_

#include <string>
#include <vector>

namespace systemd_commander {

struct ProcessResult {
  bool started{false};
  bool exited_normally{false};
  int exit_code{-1};
  std::string output;

  bool succeeded() const {
    return started && exited_normally && exit_code == 0;
  }
};

ProcessResult run_process(const std::vector<std::string> & arguments);
ProcessResult run_process_interactive(const std::vector<std::string> & arguments);

}  // namespace systemd_commander

#endif  // SYSTEMD_COMMANDER__PROCESS_RUNNER_HPP_
