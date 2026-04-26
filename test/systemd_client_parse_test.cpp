#include <gtest/gtest.h>

#include <string>

#include "systemd_commander/systemd_client.hpp"

namespace systemd_commander {
namespace {

TEST(SystemdClientParseTest, ParsesServiceListOutput) {
  const std::string output =
    "dbus.service loaded active running D-Bus System Message Bus\n"
    "ssh.service loaded inactive dead OpenBSD Secure Shell server\n"
    "broken.service loaded failed failed Broken Service\n";

  const auto units = parse_systemd_list_units_output(output);
  ASSERT_EQ(units.size(), 3u);

  EXPECT_EQ(units[0].name, "dbus.service");
  EXPECT_EQ(units[0].load_state, "loaded");
  EXPECT_EQ(units[0].active_state, "active");
  EXPECT_EQ(units[0].sub_state, "running");
  EXPECT_EQ(units[0].description, "D-Bus System Message Bus");

  EXPECT_EQ(units[2].name, "broken.service");
  EXPECT_EQ(units[2].active_state, "failed");
  EXPECT_EQ(units[2].sub_state, "failed");
}

TEST(SystemdClientParseTest, ParsesServiceUnitFileListOutput) {
  const std::string output =
    "dbus.service enabled enabled\n"
    "ssh.service disabled enabled\n"
    "dbus-org.freedesktop.resolve1.service alias -\n"
    "static-helper.service static -\n";

  const auto unit_files = parse_systemd_list_unit_files_output(output);
  ASSERT_EQ(unit_files.size(), 4u);

  EXPECT_EQ(unit_files[0].name, "dbus.service");
  EXPECT_EQ(unit_files[0].unit_file_state, "enabled");

  EXPECT_EQ(unit_files[1].name, "ssh.service");
  EXPECT_EQ(unit_files[1].unit_file_state, "disabled");

  EXPECT_EQ(unit_files[2].name, "dbus-org.freedesktop.resolve1.service");
  EXPECT_EQ(unit_files[2].unit_file_state, "alias");
}

TEST(SystemdClientParseTest, ParsesShowPropertiesAndActions) {
  const std::string output =
    "Id=ssh.service\n"
    "Description=OpenBSD Secure Shell server\n"
    "LoadState=loaded\n"
    "ActiveState=active\n"
    "SubState=running\n"
    "UnitFileState=enabled\n"
    "MainPID=1024\n"
    "ExecStart={ path=/usr/sbin/sshd ; argv[]=/usr/sbin/sshd -D }\n"
    "LogNamespace=robot\n"
    "CanStart=yes\n"
    "CanStop=yes\n"
    "CanReload=no\n";

  const auto details = parse_systemd_show_output(output);
  EXPECT_EQ(details.properties.at("Id"), "ssh.service");
  EXPECT_EQ(details.properties.at("Description"), "OpenBSD Secure Shell server");
  EXPECT_EQ(details.properties.at("ExecStart"), "{ path=/usr/sbin/sshd ; argv[]=/usr/sbin/sshd -D }");
  EXPECT_EQ(details.properties.at("LogNamespace"), "robot");
  EXPECT_TRUE(details.can_start);
  EXPECT_TRUE(details.can_stop);
  EXPECT_FALSE(details.can_reload);
}

}  // namespace
}  // namespace systemd_commander
