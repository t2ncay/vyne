cmake_minimum_required(VERSION 3.10)
project(vlang-lsp-backend)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Include directories
include_directories(${CMAKE_CURRENT_SOURCE_DIR})
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/lexer)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/parser)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/ast)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/utils)

# Download JSON library
include(FetchContent)
FetchContent_Declare(
    json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.2
)
FetchContent_MakeAvailable(json)

# Find all source files
file(GLOB_RECURSE SOURCES "*.cpp")

# Create executable
add_executable(vlang-lsp-backend ${SOURCES})
target_link_libraries(vlang-lsp-backend PRIVATE nlohmann_json::nlohmann_json)

# Installation
install(TARGETS vlang-lsp-backend DESTINATION ../bin)