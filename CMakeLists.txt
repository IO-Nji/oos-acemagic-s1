cmake_minimum_required(VERSION 3.16)
project(oos-acemagic-s1 VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# OFF builds only the preview tool and the tests, which need no USB library
option(OOS_BUILD_SERVICE "Build the panel service (needs libusb-1.0)" ON)

include(GNUInstallDirs)
find_package(Threads REQUIRED)

add_compile_options(-Wall -Wextra)

# Everything that does not touch USB: drawing, fonts, settings, dirty-region tracking
add_library(oos_core STATIC
    src/core/config.cpp
    src/core/dirty_tiles.cpp
    src/core/font_orbitron.cpp
    src/core/framebuffer.cpp
)
target_include_directories(oos_core PUBLIC src)
target_link_libraries(oos_core PUBLIC Threads::Threads)

if(OOS_BUILD_SERVICE)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(LIBUSB REQUIRED IMPORTED_TARGET libusb-1.0)

    add_executable(oos-acemagic-s1
        src/main.cpp
        src/hw/panel.cpp
        src/hw/usb_device.cpp
    )
    target_compile_definitions(oos-acemagic-s1 PRIVATE OOS_VERSION="${PROJECT_VERSION}")
    target_link_libraries(oos-acemagic-s1 PRIVATE oos_core PkgConfig::LIBUSB)
    install(TARGETS oos-acemagic-s1 RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()

# Offline renderer: writes each page as a PNG. Its --check mode runs after every
# build and fails it if text overlaps, leaves the frame, or a theme colour drops
# below 4.5:1 contrast.
add_executable(oos-s1-preview tools/preview.cpp)
target_include_directories(oos-s1-preview PRIVATE tools)
target_link_libraries(oos-s1-preview PRIVATE oos_core)
add_custom_command(TARGET oos-s1-preview POST_BUILD COMMAND oos-s1-preview --check)

add_executable(oos-s1-tests tests/test_main.cpp)
target_link_libraries(oos-s1-tests PRIVATE oos_core)
add_custom_command(TARGET oos-s1-tests POST_BUILD COMMAND oos-s1-tests)

enable_testing()
add_test(NAME unit COMMAND oos-s1-tests)
add_test(NAME layout-and-contrast COMMAND oos-s1-preview --check)
