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
)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# Source files
set(SOURCES
    src/main.cpp

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

# Header files
set(HEADERS
    # Utils
    src/utils/Logger.h
    src/utils/Process.h
    src/utils/FileSystem.h
    src/utils/Network.h
    src/utils/DistroDetector.h

    # Models
    src/models/Site.h
    src/models/PHPVersion.h
    src/models/Service.h

    # Core
    src/core/ConfigManager.h
    src/core/ServiceManager.h

    # Managers
    src/managers/SiteManager.h
    src/managers/VersionManager.h
    src/managers/ProcessManager.h

    # Services
    src/services/BaseService.h
    src/services/PHPService.h
    src/services/DnsService.h
    src/services/NginxService.h
    src/services/DatabaseService.h
    src/services/NodeService.h
)

add_executable(${PROJECT_NAME} ${SOURCES} ${HEADERS})

target_link_libraries(${PROJECT_NAME}
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Network
    Qt6::Concurrent
)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

install(TARGETS ${PROJECT_NAME}
    RUNTIME DESTINATION bin
)

# ============================================================================
# Testing
# ============================================================================

option(ENABLE_TESTING "Enable testing" OFF)

if(ENABLE_TESTING)
    enable_testing()

    # Test source files (reusable components)
    set(TEST_COMMON_SOURCES
        src/utils/Logger.cpp
        src/utils/Process.cpp
        src/utils/FileSystem.cpp
        src/utils/Network.cpp
        src/utils/DistroDetector.cpp
        src/models/Site.cpp
        src/models/PHPVersion.cpp
        src/models/Service.cpp
        src/core/ConfigManager.cpp
        src/core/ServiceManager.cpp
        src/managers/SiteManager.cpp
        src/managers/VersionManager.cpp
        src/managers/ProcessManager.cpp
        src/services/BaseService.cpp
        src/services/PHPService.cpp
        src/services/DnsService.cpp
        src/services/NginxService.cpp
        src/services/DatabaseService.cpp
        src/services/NodeService.cpp
    )

    # Integration Test 1: ConfigManager, ServiceManager, NodeService
    add_executable(test_integration
        src/tests/integration/test_integration.cpp
        ${TEST_COMMON_SOURCES}
    )

    target_link_libraries(test_integration
        Qt6::Core
        Qt6::Network
    )

    target_include_directories(test_integration PRIVATE
        ${CMAKE_SOURCE_DIR}/src
    )

    add_test(NAME IntegrationTest1 COMMAND test_integration)

    # Integration Test 2: PHP Bug Fix + SiteManager
    add_executable(test_integration_2
        src/tests/integration/test_integration_2.cpp
        ${TEST_COMMON_SOURCES}
    )

    target_link_libraries(test_integration_2
        Qt6::Core
        Qt6::Network
    )

    target_include_directories(test_integration_2 PRIVATE
        ${CMAKE_SOURCE_DIR}/src
    )

    add_test(NAME IntegrationTest2 COMMAND test_integration_2)

    # Integration Test 3: VersionManager
    add_executable(test_version_manager
        src/tests/integration/test_version_manager.cpp
        ${TEST_COMMON_SOURCES}
    )

    target_link_libraries(test_version_manager
        Qt6::Core
        Qt6::Network
    )

    target_include_directories(test_version_manager PRIVATE
        ${CMAKE_SOURCE_DIR}/src
    )

    add_test(NAME VersionManagerTest COMMAND test_version_manager)

    # Integration Test 4: ProcessManager
    add_executable(test_process_manager
        src/tests/integration/test_process_manager.cpp
        ${TEST_COMMON_SOURCES}
    )

    target_link_libraries(test_process_manager
        Qt6::Core
        Qt6::Network
    )

    target_include_directories(test_process_manager PRIVATE
        ${CMAKE_SOURCE_DIR}/src
    )

    add_test(NAME ProcessManagerTest COMMAND test_process_manager)

    # Display test info
    message(STATUS "")
    message(STATUS "Testing enabled. Available tests:")
    message(STATUS "  • test_integration       - ConfigManager, ServiceManager, NodeService")
    message(STATUS "  • test_integration_2     - PHP Bug Fix + SiteManager")
    message(STATUS "  • test_version_manager   - VersionManager")
    message(STATUS "  • test_process_manager   - ProcessManager")
    message(STATUS "")
    message(STATUS "Run tests with: ctest --output-on-failure")
    message(STATUS "")
endif()
