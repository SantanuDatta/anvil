cmake_minimum_required(VERSION 3.16)
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
    Svg
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
    Qt${QT_VERSION_MAJOR}::Svg
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
