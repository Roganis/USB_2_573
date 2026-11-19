# SPDX-License-Identifier: GPL-3.0-or-later
# The firmware's core on the PC:  cmake -S firmware/tests -B build/tests && cmake --build build/tests && ctest --test-dir build/tests
cmake_minimum_required(VERSION 3.13)
project(u573_tests C)
set(CMAKE_C_STANDARD 11)
enable_testing()
add_executable(test_core test_core.c ../core/u573_core.c)
target_include_directories(test_core PRIVATE ../core)
target_compile_options(test_core PRIVATE -Wall -Wextra -Werror -fsanitize=address,undefined)
target_link_options(test_core PRIVATE -fsanitize=address,undefined)
add_test(NAME core COMMAND test_core)
