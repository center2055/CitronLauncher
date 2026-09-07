cmake_minimum_required(VERSION 3.28)

project(CitronLauncher VERSION 0.1.0 LANGUAGES CXX RC)

if(NOT WIN32 OR NOT MSVC)
    message(FATAL_ERROR "Citron Launcher targets Windows x64 with MSVC")
endif()

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

if(NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

set(CITRON_COMPILER "${CMAKE_CXX_COMPILER_ID}")
string(REGEX MATCH "^([0-9]+)\\.([0-9]+)" _compiler_version "${CMAKE_CXX_COMPILER_VERSION}")
if(_compiler_version)
    set(CITRON_COMPILER "MSVC ${CMAKE_MATCH_1}.${CMAKE_MATCH_2}")
endif()
set(CITRON_BUILDINFO_HEADER ${CMAKE_BINARY_DIR}/generated/BuildInfo.h)
set(CITRON_BUILDINFO_COMMAND ${CMAKE_COMMAND}
    -DSOURCE_DIR=${CMAKE_SOURCE_DIR}
    -DOUTPUT=${CITRON_BUILDINFO_HEADER}
    -DVERSION=${PROJECT_VERSION}
    -DVERSION_MAJOR=${PROJECT_VERSION_MAJOR}
    -DVERSION_MINOR=${PROJECT_VERSION_MINOR}
    -DVERSION_PATCH=${PROJECT_VERSION_PATCH}
    "-DCOMPILER=${CITRON_COMPILER}"
    -P ${CMAKE_SOURCE_DIR}/cmake/BuildInfo.cmake)
execute_process(COMMAND ${CITRON_BUILDINFO_COMMAND})
add_custom_target(citron_buildinfo COMMAND ${CITRON_BUILDINFO_COMMAND} BYPRODUCTS ${CITRON_BUILDINFO_HEADER})

option(CITRON_BUILD_APP "build the launcher executable" ON)

set(CITRON_WARNINGS /W4 /permissive- /w14640 /w14826 /w14905 /w14906 /w14928)
set(CITRON_DEFINES
    UNICODE _UNICODE
    WIN32_LEAN_AND_MEAN NOMINMAX STRICT
    _WIN32_WINNT=0x0A00 WINVER=0x0A00
    WINRT_LEAN_AND_MEAN)

add_library(citron_core STATIC
    src/core/Error.cpp
    src/core/Format.cpp
    src/core/Json.cpp
    src/core/Logger.cpp
    src/core/Paths.cpp
    src/core/PathSafety.cpp
    src/core/Settings.cpp
    src/core/TaskScheduler.cpp
    src/core/Text.cpp
    src/minecraft/Version.cpp
    src/minecraft/VersionCatalog.cpp
    src/minecraft/InstallState.cpp
    src/download/DownloadMeta.cpp)
target_include_directories(citron_core PUBLIC src ${CMAKE_BINARY_DIR}/generated)
target_compile_options(citron_core PUBLIC ${CITRON_WARNINGS} /utf-8 /EHsc /Zc:__cplusplus /Zc:preprocessor /bigobj)
target_compile_definitions(citron_core PUBLIC ${CITRON_DEFINES})
target_link_libraries(citron_core PUBLIC shlwapi shell32 ole32 advapi32)

if(CITRON_BUILD_APP)
add_executable(CitronLauncher WIN32
    src/main.cpp
    src/app/Application.cpp
    src/app/AppState.cpp
    src/app/Strings.cpp
    src/download/Download.cpp
    src/download/Http.cpp
    src/download/DownloadManager.cpp
    src/minecraft/MinecraftService.cpp
    src/minecraft/VersionManager.cpp
    src/minecraft/InstallationManager.cpp
    src/minecraft/LaunchManager.cpp
    src/minecraft/PrerequisiteManager.cpp
    src/platform/windows/PackageManager.cpp
    src/platform/windows/Activation.cpp
    src/platform/windows/Registry.cpp
    src/platform/windows/Process.cpp
    src/platform/windows/Shell.cpp
    src/platform/windows/Dpi.cpp
    src/platform/windows/FileOps.cpp
    src/platform/windows/Hash.cpp
    src/platform/windows/Gdk.cpp
    src/ui/Window.cpp
    src/ui/Renderer.cpp
    src/ui/Theme.cpp
    src/ui/Animation.cpp
    src/ui/Icons.cpp
    src/ui/Wordmark.cpp
    src/ui/controls/Element.cpp
    src/ui/controls/Text.cpp
    src/ui/controls/Button.cpp
    src/ui/controls/IconButton.cpp
    src/ui/controls/StackPanel.cpp
    src/ui/controls/ScrollView.cpp
    src/ui/controls/ListView.cpp
    src/ui/controls/TextInput.cpp
    src/ui/controls/Toggle.cpp
    src/ui/controls/ProgressBar.cpp
    src/ui/controls/Dialog.cpp
    src/ui/controls/Tabs.cpp
    src/ui/controls/Toast.cpp
    src/ui/controls/Spinner.cpp
    src/ui/pages/PlayPage.cpp
    src/ui/pages/VersionsPage.cpp
    src/ui/pages/SettingsPage.cpp
    src/ui/Shell.cpp
    resources/citron.rc
    resources/app.manifest)
target_link_libraries(CitronLauncher PRIVATE citron_core
    d3d11 dxgi d2d1 dwrite dcomp dwmapi winhttp oleaut32 uuid bcrypt
    user32 gdi32 version imm32 windowscodecs windowsapp)
set_target_properties(CitronLauncher PROPERTIES OUTPUT_NAME "Citron")
add_dependencies(CitronLauncher citron_buildinfo)
target_include_directories(CitronLauncher PRIVATE resources)

target_compile_options(citron_core PUBLIC $<$<CONFIG:Release>:/O2 /GL /Gy /Gw /Oi>)
target_link_options(CitronLauncher PRIVATE
    $<$<CONFIG:Release>:/LTCG /OPT:REF /OPT:ICF /INCREMENTAL:NO /DEBUG:FULL>
    /SUBSYSTEM:WINDOWS /DYNAMICBASE /NXCOMPAT)
endif()

enable_testing()
add_executable(citron_tests
    tests/main.cpp
    tests/test_version.cpp
    tests/test_json.cpp
    tests/test_settings.cpp
    tests/test_paths.cpp
    tests/test_format.cpp
    tests/test_catalog.cpp
    tests/test_install_state.cpp
    tests/test_download_meta.cpp
    tests/test_error.cpp)
target_link_libraries(citron_tests PRIVATE citron_core)
add_test(NAME citron_tests COMMAND citron_tests)
