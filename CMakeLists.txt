#
cmake_minimum_required(VERSION 3.8)


#
project("r8brain-free-src")


#
##
include_directories("src")


#
add_subdirectory("src/pffft_double")
add_subdirectory("src/r8brain-free-src")
add_subdirectory("src/r8brain-free-src-example")
