cmake_minimum_required(VERSION 3.16)
project(libfutex)

set(CMAKE_CXX_STANDARD 20)

include(FetchContent)
FetchContent_Declare(spdlog GIT_REPOSITORY https://github.com/gabime/spdlog.git GIT_TAG v1.x)
FetchContent_MakeAvailable(spdlog)

add_executable(libfutex robust_futex.c libcrtest/common.c)

add_executable(robust_futex robust_futex.cpp)
target_link_libraries(robust_futex spdlog::spdlog)

add_executable(robust_mutex robust_mutex.cpp)
target_link_libraries(robust_mutex pthread spdlog::spdlog)
