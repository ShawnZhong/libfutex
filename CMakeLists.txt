cmake_minimum_required(VERSION 3.16)
project(libfutex)

set(CMAKE_CXX_STANDARD 20)

include(FetchContent)
FetchContent_Declare(spdlog GIT_REPOSITORY https://github.com/gabime/spdlog.git GIT_TAG v1.x)
FetchContent_MakeAvailable(spdlog)

add_executable(main main.cpp Futex.h RobustMutex.h RobustSpinlock.h)
target_link_libraries(main spdlog::spdlog)
