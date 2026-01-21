cmake_minimum_required(VERSION 3.16)
project(Anvil VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS
    Core
    Gui
    Widgets
    Network
    Concurrent
    DBus
)

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
    Qt6::Core
    Qt6::Network
    Qt6::Concurrent
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
)

target_link_libraries(anvil_ui PUBLIC
    anvil_backend
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Network
    Qt6::Concurrent
    Qt6::DBus
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
option(ENABLE_TESTING "Enable testing" OFF)

if(ENABLE_TESTING)
    enable_testing()

    add_executable(test_integration
        src/tests/integration/test_integration.cpp
    )
    target_link_libraries(test_integration PRIVATE anvil_backend)
    add_test(NAME IntegrationTest1 COMMAND test_integration)

    add_executable(test_integration_2
        src/tests/integration/test_integration_2.cpp
    )
    target_link_libraries(test_integration_2 PRIVATE anvil_backend)
    add_test(NAME IntegrationTest2 COMMAND test_integration_2)

    add_executable(test_version_manager
        src/tests/integration/test_version_manager.cpp
    )
    target_link_libraries(test_version_manager PRIVATE anvil_backend)
    add_test(NAME VersionManagerTest COMMAND test_version_manager)

    add_executable(test_process_manager
        src/tests/integration/test_process_manager.cpp
    )
    target_link_libraries(test_process_manager PRIVATE anvil_backend)
    add_test(NAME ProcessManagerTest COMMAND test_process_manager)

    message(STATUS "")
    message(STATUS "Testing enabled. Run tests with: ctest --output-on-failure")
    message(STATUS "")
endif()
