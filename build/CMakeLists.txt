cmake_minimum_required(VERSION 3.0.0)

# project name
project(l2-distributedMemory)

set(CMAKE_CXX_STANDARD 20)
find_package(MPI REQUIRED)
include_directories(${MPI_INCLUDE_PATH})

# set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -Wall -Wextra -pedantic -fpermissive -fopenmp")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -fpermissive -fopenmp")
# include dirs
include_directories(../include/)
include_directories(../include/nlohmann)
include_directories(../include/sha512)

# puts all .cpp files inside src to the SOURCES variable
file(GLOB SOURCES ../src/*.cpp ../include/sha512/*.cpp)
# outputs executable binary file to bin folder
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ../bin)

# specify OpenMPI compiler
SET(CMAKE_C_COMPILER mpicc)
SET(CMAKE_CXX_COMPILER mpicxx)

# compiles the files defined by SOURCES to generante the executable defined by EXEC
add_executable(main ${SOURCES})