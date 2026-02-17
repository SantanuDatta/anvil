cmake_minimum_required(VERSION 3.16)

# Guard Linux builds from selecting clang-cl (MSVC frontend), which does not
# work with the default Linux toolchain/runtime expectations.
if(UNIX AND NOT APPLE)
    set(_anvil_requested_cxx "")
    set(_anvil_requested_cxx_source "")

    # Respect explicit -DCMAKE_CXX_COMPILER selection over the CXX env var.
    if(DEFINED CMAKE_CXX_COMPILER AND NOT CMAKE_CXX_COMPILER STREQUAL "")
        set(_anvil_requested_cxx "${CMAKE_CXX_COMPILER}")
        set(_anvil_requested_cxx_source "CMAKE_CXX_COMPILER")
    elseif(DEFINED ENV{CXX} AND NOT "$ENV{CXX}" STREQUAL "")
        set(_anvil_requested_cxx "$ENV{CXX}")
        set(_anvil_requested_cxx_source "CXX environment variable")
    endif()

    if(NOT _anvil_requested_cxx STREQUAL "")
        separate_arguments(_anvil_requested_cxx_parts NATIVE_COMMAND "${_anvil_requested_cxx}")
        list(GET _anvil_requested_cxx_parts 0 _anvil_requested_cxx_bin)
        get_filename_component(_anvil_requested_cxx_name "${_anvil_requested_cxx_bin}" NAME)
        string(TOLOWER "${_anvil_requested_cxx_name}" _anvil_requested_cxx_name_lower)

        if(_anvil_requested_cxx_name_lower MATCHES "^clang-cl(\\.exe)?$")
            find_program(_anvil_linux_fallback_cxx NAMES clang++ g++)

            if(_anvil_linux_fallback_cxx)
                set(CMAKE_CXX_COMPILER "${_anvil_linux_fallback_cxx}" CACHE FILEPATH
                    "Linux fallback C++ compiler when clang-cl is selected" FORCE)
                message(WARNING
                    "clang-cl was selected via ${_anvil_requested_cxx_source} on Linux. "
                    "clang-cl targets MSVC-style environments and fails in standard Linux builds. "
                    "Using '${_anvil_linux_fallback_cxx}' instead."
                )
            else()
                message(FATAL_ERROR
                    "clang-cl was selected via ${_anvil_requested_cxx_source} on Linux, but no "
                    "fallback compiler was found. Install clang++ or g++, or set "
                    "-DCMAKE_CXX_COMPILER to a Linux-compatible compiler."
                )
            endif()
        endif()
    endif()
endif()

project(Anvil VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# ============================================================================
# Testing option (declared early so Qt Test dependency can be conditional)
# ============================================================================
option(ENABLE_TESTING "Enable testing" OFF)

set(ANVIL_QT_COMPONENTS
    Core
    Gui
    Widgets
    Network
    Concurrent
    DBus
)

if(ENABLE_TESTING)
    list(APPEND ANVIL_QT_COMPONENTS Test)
endif()

find_package(QT NAMES Qt6 Qt5 QUIET COMPONENTS ${ANVIL_QT_COMPONENTS})

if(NOT QT_FOUND)
    message(FATAL_ERROR
        "Qt development packages were not found. Install Qt6 (preferred) or Qt5, "
        "or set CMAKE_PREFIX_PATH/Qt6_DIR/Qt5_DIR to your Qt CMake package path."
    )
endif()

find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS ${ANVIL_QT_COMPONENTS})

message(STATUS "Using Qt${QT_VERSION_MAJOR}")

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# ============================================================================
# Backend library (NO UI) - safe to link in tests
# ============================================================================
add_library(anvil_backend STATIC
    # Utils
    src/utils/Logger.cpp
    src/utils/Process.cpp
    src/utils/FileSystem.cpp
    src/utils/Network.cpp
    src/utils/DistroDetector.cpp

    # Models
    src/models/Site.cpp
    src/models/PHPVersion.cpp
    src/models/Service.cpp

    # Core
    src/core/ConfigManager.cpp
    src/core/ServiceManager.cpp

    # Managers
    src/managers/SiteManager.cpp
    src/managers/VersionManager.cpp
    src/managers/ProcessManager.cpp

    # Services
    src/services/BaseService.cpp
    src/services/PHPService.cpp
    src/services/DnsService.cpp
    src/services/NginxService.cpp
    src/services/DatabaseService.cpp
    src/services/NodeService.cpp
)

target_link_libraries(anvil_backend PUBLIC
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Network
    Qt${QT_VERSION_MAJOR}::Concurrent
)

target_include_directories(anvil_backend PUBLIC
    ${CMAKE_SOURCE_DIR}/src
)

# ============================================================================
# UI library (Widgets UI)
# ============================================================================
add_library(anvil_ui STATIC
    src/ui/Theme.cpp
    src/ui/Theme.h
    src/ui/MainWindow.cpp
    src/ui/MainWindow.h
    src/ui/resources.qrc
)

target_link_libraries(anvil_ui PUBLIC
    anvil_backend
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Gui
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::Network
    Qt${QT_VERSION_MAJOR}::Concurrent
    Qt${QT_VERSION_MAJOR}::DBus
)

target_include_directories(anvil_ui PUBLIC
    ${CMAKE_SOURCE_DIR}/src
)

# ============================================================================
# Main executable (only main.cpp)
# ============================================================================
add_executable(Anvil
    src/main.cpp
)

target_link_libraries(Anvil PRIVATE
    anvil_ui
)

install(TARGETS Anvil
    RUNTIME DESTINATION bin
)

# ============================================================================
# Testing (optional)
# ============================================================================
if(ENABLE_TESTING)
    enable_testing()

    add_executable(test_php_version
        src/tests/unit/test_php_version.cpp
    )
    target_link_libraries(test_php_version PRIVATE anvil_backend Qt${QT_VERSION_MAJOR}::Test)
    add_test(NAME PHPVersionUnitTest COMMAND test_php_version)

    add_executable(test_site
        src/tests/unit/test_site.cpp
    )
    target_link_libraries(test_site PRIVATE anvil_backend Qt${QT_VERSION_MAJOR}::Test)
    add_test(NAME SiteUnitTest COMMAND test_site)

    add_executable(test_filesystem_integration
        src/tests/integration/test_filesystem_integration.cpp
    )
    target_link_libraries(test_filesystem_integration PRIVATE anvil_backend Qt${QT_VERSION_MAJOR}::Test)
    add_test(NAME FileSystemIntegrationTest COMMAND test_filesystem_integration)

    add_executable(test_process_executor_integration
        src/tests/integration/test_process_executor_integration.cpp
    )
    target_link_libraries(test_process_executor_integration PRIVATE anvil_backend Qt${QT_VERSION_MAJOR}::Test)
    add_test(NAME ProcessExecutorIntegrationTest COMMAND test_process_executor_integration)

    message(STATUS "")
    message(STATUS "Testing enabled. Run tests with: ctest --output-on-failure")
    message(STATUS "")
endif()
