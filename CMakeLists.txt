cmake_minimum_required(VERSION 3.16)

project(systemd_commander VERSION 1.0.0 LANGUAGES CXX)

include(GNUInstallDirs)
include(CTest)

if(NOT CMAKE_CXX_STANDARD)
  set(CMAKE_CXX_STANDARD 17)
  set(CMAKE_CXX_STANDARD_REQUIRED ON)
endif()

set(CMAKE_CXX_EXTENSIONS OFF)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

set(CURSES_NEED_WIDE TRUE)
find_package(Curses REQUIRED)
find_library(NCURSESW_LIBRARY NAMES ncursesw REQUIRED)

add_library(systemd_commander_common
  src/process_runner.cpp
  src/systemd_client.cpp
  src/journal_client.cpp
)
target_include_directories(systemd_commander_common PUBLIC include)

add_library(systemd_commander_tui
  src/tui.cpp
)
target_include_directories(systemd_commander_tui PUBLIC include ${CURSES_INCLUDE_DIR})
target_link_libraries(systemd_commander_tui PUBLIC ${NCURSESW_LIBRARY})
target_compile_definitions(systemd_commander_tui PRIVATE
  SYSTEMD_COMMANDER_DATA_DIR="${CMAKE_INSTALL_FULL_DATADIR}/${PROJECT_NAME}"
)

add_library(journal_viewer_lib
  src/journal_viewer_backend.cpp
  src/journal_viewer_screen.cpp
)
target_include_directories(journal_viewer_lib PUBLIC include ${CURSES_INCLUDE_DIR})
target_link_libraries(journal_viewer_lib PUBLIC
  systemd_commander_common
  systemd_commander_tui
)

add_library(systemd_commander_lib
  src/systemd_commander_backend.cpp
  src/systemd_commander_screen.cpp
)
target_include_directories(systemd_commander_lib PUBLIC include ${CURSES_INCLUDE_DIR})
target_link_libraries(systemd_commander_lib PUBLIC
  journal_viewer_lib
  systemd_commander_common
  systemd_commander_tui
)

add_executable(systemd_commander src/systemd_commander.cpp)
target_link_libraries(systemd_commander PRIVATE systemd_commander_lib)

add_executable(journal_viewer src/journal_viewer.cpp)
target_link_libraries(journal_viewer PRIVATE journal_viewer_lib)

install(TARGETS
  systemd_commander_common
  systemd_commander_tui
  journal_viewer_lib
  systemd_commander_lib
  systemd_commander
  journal_viewer
)

install(DIRECTORY include/
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(FILES config/tui_theme.yaml
  DESTINATION ${CMAKE_INSTALL_DATADIR}/${PROJECT_NAME}/config
)

if(BUILD_TESTING)
  find_package(GTest QUIET)
  if(GTest_FOUND)
    add_executable(systemd_client_parse_test test/systemd_client_parse_test.cpp)
    target_link_libraries(systemd_client_parse_test PRIVATE
      GTest::gtest_main
      systemd_commander_common
    )
    add_test(NAME systemd_client_parse_test COMMAND systemd_client_parse_test)

    add_executable(journal_client_parse_test test/journal_client_parse_test.cpp)
    target_link_libraries(journal_client_parse_test PRIVATE
      GTest::gtest_main
      systemd_commander_common
    )
    add_test(NAME journal_client_parse_test COMMAND journal_client_parse_test)
  endif()
endif()
