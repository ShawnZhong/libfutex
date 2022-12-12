cmake_minimum_required(VERSION 3.16)
project(libfutex)

set(CMAKE_CXX_STANDARD 20)

include(FetchContent)
FetchContent_Declare(spdlog GIT_REPOSITORY https://github.com/gabime/spdlog.git GIT_TAG v1.x)
FetchContent_Declare(Catch2 GIT_REPOSITORY https://github.com/catchorg/Catch2.git GIT_TAG v3.2.0)
FetchContent_MakeAvailable(spdlog Catch2)

add_executable(main main.cpp Futex.h RobustMutex.h RobustSpinlock.h)
target_compile_options(main PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Werror)
target_link_libraries(main spdlog::spdlog Catch2::Catch2)
