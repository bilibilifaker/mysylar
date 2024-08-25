cmake_minimum_required(VERSION 2.8)
project(sylar)

set(CMAKE_VERBOSE_MAKEFILE ON)
set(CMAKE_CXX_FLAGS "$ENV{CXXFLAGS} -rdynamic -O0 -g -std=c++11 -Wall -Wno-deprecated -Werror -Wno-unused-function")

include_directories(.)

find_library(YAMLCPP libyaml-cpp.a)

set(LIB_SRC
    src/log.cpp
    )


add_library(sylar SHARED ${LIB_SRC})

add_executable(test_log test/log_test.cpp)
add_executable(test_config test/config_test.cpp)
add_executable(test_thread test/thread_test.cpp)
add_executable(test_macro test/macro_test.cpp)
add_executable(test_fiber test/fiber_test.cpp)
add_executable(test_scheduler test/scheduler_test.cpp)
add_executable(test_IOManager test/IOManager_test.cpp)
add_dependencies(test_log sylar)
add_dependencies(test_config sylar)
add_dependencies(test_thread sylar)
add_dependencies(test_macro sylar)
add_dependencies(test_fiber sylar)
add_dependencies(test_scheduler sylar)
add_dependencies(test_IOManager sylar)
target_link_libraries(test_log sylar yaml-cpp dl)
target_link_libraries(test_config sylar yaml-cpp dl)
target_link_libraries(test_thread sylar yaml-cpp dl)
target_link_libraries(test_macro sylar yaml-cpp dl)
target_link_libraries(test_fiber sylar yaml-cpp dl)
target_link_libraries(test_scheduler sylar yaml-cpp dl)
target_link_libraries(test_IOManager sylar yaml-cpp dl)

add_executable(test_timer test/timer_test.cpp)
add_dependencies(test_timer sylar)
target_link_libraries(test_timer sylar yaml-cpp dl)

add_executable(test_hook test/hook_test.cpp)
add_dependencies(test_hook sylar)
target_link_libraries(test_hook sylar yaml-cpp dl)

add_executable(test_address test/address_test.cpp)
add_dependencies(test_address sylar)
target_link_libraries(test_address sylar yaml-cpp dl)

SET(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin)
SET(LIBRARY_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/lib)