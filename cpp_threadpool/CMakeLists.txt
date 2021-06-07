cmake_minimum_required(VERSION 3.0.0)
project(threadpool_cpp VERSION 0.1.0)

include(CTest)
enable_testing()

add_executable(threadpool_cpp main.cpp threadpool.cpp threadpool.h)

find_package(Threads)
target_link_libraries(threadpool_cpp ${CMAKE_THREAD_LIBS_INIT})

set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})
include(CPack)
