cmake_minimum_required(VERSION 3.20)

project(simple-duck-dbms LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(DUCK_BUILD_TESTS "Build unit tests" ON)
option(DUCK_ENABLE_ASAN "Enable AddressSanitizer" OFF)
option(DUCK_ENABLE_TSAN "Enable ThreadSanitizer" OFF)

add_compile_options(-Wall -Wextra -Wpedantic)

if(DUCK_ENABLE_ASAN)
    add_compile_options(-fsanitize=address,undefined -g)
    add_link_options(-fsanitize=address,undefined)
elseif(DUCK_ENABLE_TSAN)
    add_compile_options(-fsanitize=thread -g)
    add_link_options(-fsanitize=thread)
endif()

find_package(Threads REQUIRED)

# --- libraries ---

# duck_storage
add_library(duck_storage
    src/storage/disk_manager.cpp
    src/buffer/pool_manager.cpp
)
target_include_directories(duck_storage PUBLIC include)
target_link_libraries(duck_storage PUBLIC Threads::Threads)

# --- executable ---
add_executable(duck src/main.cpp)
target_link_libraries(duck PRIVATE duck_storage)