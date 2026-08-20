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
)
target_include_directories(duck_storage PUBLIC include)
target_link_libraries(duck_storage PUBLIC Threads::Threads)

# duck_buffer
add_library(duck_buffer
    src/buffer/pool_manager.cpp
    src/buffer/page.cpp
    src/buffer/pinned_page.cpp
    src/buffer/replacer.cpp
)
target_include_directories(duck_buffer PUBLIC include)
target_link_libraries(duck_buffer PUBLIC Threads::Threads)

# duck_tuple
add_library(duck_tuple
    src/tuple/tuple.cpp
    src/tuple/schema.cpp
    src/tuple/value.cpp
)
target_include_directories(duck_tuple PUBLIC include)

# duck_table
add_library(duck_table
    src/table/slotted_page.cpp
    src/table/table_heap.cpp
)
target_include_directories(duck_table PUBLIC include)
target_link_libraries(duck_table PUBLIC duck_tuple duck_buffer)

# --- executable ---
add_executable(duck src/main.cpp)
target_link_libraries(duck PRIVATE 
    duck_storage
    # duck_buffer   # <- transitive
    # duck_tuple    # <- transitive
    duck_table
)

# --- tests ---
if(DUCK_BUILD_TESTS)
  include(FetchContent)
  FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.15.2.zip
  )
  set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
  FetchContent_MakeAvailable(googletest)

  enable_testing()
  add_executable(duck_tests
      test/storage/disk_manager_test.cpp
      test/buffer/buffer_pool_manager_test.cpp
      test/table/slotted_page_test.cpp
      test/table/table_heap_test.cpp
  )
  target_link_libraries(duck_tests PRIVATE duck_storage duck_table GTest::gtest_main)
  include(GoogleTest)
  gtest_discover_tests(duck_tests)
endif()