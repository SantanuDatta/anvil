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

    # Services
    src/services/BaseService.cpp
    src/services/PHPService.cpp
    src/services/DnsService.cpp
    src/services/NginxService.cpp
    src/services/DatabaseService.cpp
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

    # Services
    src/services/BaseService.h
    src/services/PHPService.h
    src/services/DnsService.h
    src/services/NginxService.h
    src/services/DatabaseService.h
)

add_executable(${PROJECT_NAME} ${SOURCES} ${HEADERS})

target_link_libraries(${PROJECT_NAME}
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Network
)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

install(TARGETS ${PROJECT_NAME}
    RUNTIME DESTINATION bin
)
