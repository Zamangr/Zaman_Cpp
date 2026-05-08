cmake_minimum_required(VERSION 3.16)
project(RoundRobinScheduling CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_BUILD_TYPE Release)
set(CMAKE_CXX_FLAGS_RELEASE "-O2 -Wall -Wextra")

include_directories(include)

set(SOURCES
    src/Schedule.cpp
    src/Validator.cpp
    src/Metrics.cpp
    src/BergerScheduler.cpp
    src/GreedyScheduler.cpp
    src/BacktrackScheduler.cpp
    src/GeneticScheduler.cpp
    src/main.cpp
)

add_executable(benchmark ${SOURCES})
