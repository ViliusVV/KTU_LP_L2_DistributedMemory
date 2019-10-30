# only for cmake --version >= 3.5.1
cmake_minimum_required(VERSION 3.0.0)

# project name
project(l1-a-sharedmemory)

# set the C++14 standard
set(CMAKE_CXX_STANDARD 20)
set( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -Wall -Wextra -pedantic -fpermissive -fopenmp")

# include dirs
include_directories(../include/)
include_directories(../include/nlohmann)
include_directories(../include/sha512)

# puts all .cpp files inside src to the SOURCES variable
file(GLOB SOURCES ../src/*.cpp ../include/sha512/*.cpp)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ../bin)

# compiles the files defined by SOURCES to generante the executable defined by EXEC
add_executable(main ${SOURCES})