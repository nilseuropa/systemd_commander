#include <gtest/gtest.h>

#include "systemd_commander/tui.hpp"

namespace systemd_commander::tui {
namespace {

TEST(TuiTest, TerminalSizeSupportIncludesCompactTerminals) {
  EXPECT_TRUE(terminal_size_supported(18, 40));
  EXPECT_TRUE(terminal_size_supported(24, 60));
  EXPECT_FALSE(terminal_size_supported(17, 80));
  EXPECT_FALSE(terminal_size_supported(24, 39));
}

TEST(TuiTest, ScrollOffsetKeepsSelectionInsideVisibleItemRows) {
  EXPECT_EQ(scroll_offset_for_selection(11, 0, 13), 0);
  EXPECT_EQ(scroll_offset_for_selection(12, 0, 13), 0);
  EXPECT_EQ(scroll_offset_for_selection(13, 0, 13), 1);
  EXPECT_EQ(scroll_offset_for_selection(20, 8, 13), 8);
  EXPECT_EQ(scroll_offset_for_selection(21, 8, 13), 9);
  EXPECT_EQ(scroll_offset_for_selection(4, 8, 13), 4);
}

}  // namespace
}  // namespace systemd_commander::tui
