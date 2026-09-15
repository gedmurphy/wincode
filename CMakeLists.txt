#
# WinCode
#
# Copyright (c) 2026 Ged Murphy. GPLv3 or later; see LICENSE.
#
cmake_minimum_required(VERSION 3.25)
project(WinCode VERSION 2.0.0 LANGUAGES CXX RC)

if(NOT MSVC)
    message(FATAL_ERROR "WinCode builds with MSVC only.")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

#
# Static CRT, so every binary runs on a bare Windows install, a test VM
# included, with nothing deployed beside it.
#
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

add_compile_definitions(UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX _WIN32_WINNT=0x0A00)
add_compile_options("$<$<COMPILE_LANGUAGE:CXX>:/W4;/WX;/permissive-;/utf-8;/Zc:__cplusplus;/sdl>")

#
# The core: parsing, lookup and decoding, with no UI. Every front end is a
# thin layer over it.
#
add_library(WinCodeCore STATIC
    Core/Describe.cpp
    Core/Lookup.cpp
    Core/Parse.cpp
    Core/Search.cpp
    Core/Version.cpp
    Core/Tables/BugChecks.g.cpp
    Core/Tables/HResultFacilities.g.cpp
    Core/Tables/HResults.g.cpp
    Core/Tables/NtFacilities.g.cpp
    Core/Tables/NtStatus.g.cpp
    Core/Tables/TablesInfo.g.cpp
    Core/Tables/WindowMessages.g.cpp
    Core/Tables/WinErrors.g.cpp)
target_include_directories(WinCodeCore PUBLIC Core)
target_link_libraries(WinCodeCore PUBLIC user32)
target_compile_definitions(WinCodeCore PRIVATE
    WINCODE_VERSION_MAJOR=${PROJECT_VERSION_MAJOR}
    WINCODE_VERSION_MINOR=${PROJECT_VERSION_MINOR}
    WINCODE_VERSION_PATCH=${PROJECT_VERSION_PATCH})

add_executable(wincode
    Cli/WinCodeCli.cpp)
target_link_libraries(wincode PRIVATE WinCodeCore)

#
# The GUI. WinCodeUI rather than WinCode: file names ignore case, so it would
# clash with wincode.exe in the same directory.
#
add_executable(WinCodeUI WIN32
    Gui/DetailsPane.cpp
    Gui/Draw.cpp
    Gui/Hotkey.cpp
    Gui/Json.cpp
    Gui/MainWindow.cpp
    Gui/Popup.cpp
    Gui/Results.cpp
    Gui/Settings.cpp
    Gui/SettingsDialog.cpp
    Gui/Startup.cpp
    Gui/Tray.cpp
    Gui/Theme.cpp
    Gui/WinCodeUI.cpp
    Gui/WinCodeUI.rc
    Gui/WinCodeUI.manifest)
target_include_directories(WinCodeUI PRIVATE Gui)
target_link_libraries(WinCodeUI PRIVATE WinCodeCore comctl32 uxtheme dwmapi shell32 shcore ole32 oleacc)
target_compile_definitions(WinCodeUI PRIVATE
    WINCODE_VERSION_MAJOR=${PROJECT_VERSION_MAJOR}
    WINCODE_VERSION_MINOR=${PROJECT_VERSION_MINOR}
    WINCODE_VERSION_PATCH=${PROJECT_VERSION_PATCH}
    WINCODE_VERSION_STRING="${PROJECT_VERSION}")

#
# The installer and the zip, for people downloading WinCode rather than building
# it:
#
#   cmake --build --preset x64-release --target installer
#
# Not part of the default build: it needs Inno Setup, and only a Release build
# is worth packaging. x64 only for now, as the plan decided, since the ARM64
# build hasn't been checked on an ARM64 machine.
#
if(CMAKE_CXX_COMPILER_ARCHITECTURE_ID STREQUAL "x64")
    add_custom_target(installer
        COMMAND powershell -NoProfile -ExecutionPolicy Bypass
                -File "${CMAKE_CURRENT_SOURCE_DIR}/tools/build_installer.ps1"
                -BuildDir "$<TARGET_FILE_DIR:WinCodeUI>"
                -OutputDir "${CMAKE_CURRENT_SOURCE_DIR}/build"
                -Version "${PROJECT_VERSION}"
                -Arch x64
                -Configuration "$<CONFIG>"
        COMMENT "Building the installer and the zip"
        VERBATIM)

    add_dependencies(installer WinCodeUI wincode)
endif()

enable_testing()
add_executable(WinCodeTests
    Tests/WinCodeTests.cpp)
target_link_libraries(WinCodeTests PRIVATE WinCodeCore)
add_test(NAME WinCodeTests COMMAND WinCodeTests)

#
# End to end checks of the CLI: each runs wincode and looks for what it should
# print, or checks it fails when it should. ctest captures the output, so it is
# redirected and unwrapped, as a script would see it.
#
add_test(NAME wincode.value COMMAND wincode 0xC0000005)
set_tests_properties(wincode.value PROPERTIES PASS_REGULAR_EXPRESSION "NTSTATUS +STATUS_ACCESS_VIOLATION")

add_test(NAME wincode.log_text COMMAND wincode "CreateFile failed (HRESULT: 0x80070005)")
set_tests_properties(wincode.log_text PROPERTIES PASS_REGULAR_EXPRESSION "HRESULT +E_ACCESSDENIED")

add_test(NAME wincode.related COMMAND wincode 0xC0000022)
set_tests_properties(wincode.related PROPERTIES
    PASS_REGULAR_EXPRESSION "maps to +Win32 error 5 ERROR_ACCESS_DENIED +\\(RtlNtStatusToDosError\\)")

add_test(NAME wincode.name COMMAND wincode wm_close)
set_tests_properties(wincode.name PROPERTIES PASS_REGULAR_EXPRESSION "WM_CLOSE = 0x0010")

add_test(NAME wincode.no_match COMMAND wincode 0x12345678)
set_tests_properties(wincode.no_match PROPERTIES PASS_REGULAR_EXPRESSION "0x12345678  no match")

add_test(NAME wincode.no_match_exit COMMAND wincode 0x12345678)
set_tests_properties(wincode.no_match_exit PROPERTIES WILL_FAIL TRUE)

add_test(NAME wincode.usage_exit COMMAND wincode)
set_tests_properties(wincode.usage_exit PROPERTIES WILL_FAIL TRUE)

add_test(NAME wincode.help COMMAND wincode --help)
set_tests_properties(wincode.help PROPERTIES PASS_REGULAR_EXPRESSION "usage: wincode")

add_test(NAME wincode.mapped_from COMMAND wincode ERROR_ACCESS_DENIED)
set_tests_properties(wincode.mapped_from PROPERTIES
    PASS_REGULAR_EXPRESSION "mapped from +[0-9]+ NTSTATUS codes: [A-Z_, ]*STATUS_ACCESS_DENIED")
