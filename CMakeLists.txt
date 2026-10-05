cmake_minimum_required(VERSION 3.16)
project(thread_pool LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

include(CTest)

find_package(Threads REQUIRED)

# 自测目标
add_executable(test_pool
  test/test.cpp
  src/ThreadPool.cpp
)
target_include_directories(test_pool PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(test_pool PRIVATE Threads::Threads)
add_test(NAME thread_pool_self_test COMMAND $<TARGET_FILE:test_pool>)

# 场景演示目标
add_executable(thread_pool_demo
  test/demo.cpp
  src/ThreadPool.cpp
)
target_include_directories(thread_pool_demo PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(thread_pool_demo PRIVATE Threads::Threads)
