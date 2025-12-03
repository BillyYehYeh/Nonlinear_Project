cmake_minimum_required(VERSION 3.10)
project(CNN CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# SystemC configuration
# Try to find SystemC, but if not found, manually set paths
find_package(SystemC QUIET)

if(NOT SystemC_FOUND)
    # Manually configure SystemC paths
    set(SystemC_INCLUDE_DIRS /usr/include)
    set(SystemC_LIBRARY_DIRS /usr/lib /usr/lib/x86_64-linux-gnu /usr/local/lib)
    set(SystemC_LIBRARIES systemc m)
    
    message(STATUS "Using manual SystemC configuration:")
    message(STATUS "  Include: ${SystemC_INCLUDE_DIRS}")
    message(STATUS "  Library dirs: ${SystemC_LIBRARY_DIRS}")
else()
    message(STATUS "Found SystemC via find_package")
endif()

# Include directories
include_directories(${CMAKE_SOURCE_DIR}/include)
include_directories(${SystemC_INCLUDE_DIRS})

# Source files for SOLE executable
set(SOLE_SOURCES
    src/SOLE.cpp
    src/E2Softmax_LayerNorm.cpp
)

# Source files for MaxUnit test executable
set(MAXUNIT_TEST_SOURCES
    test/MaxUnit_test.cpp
)

# Create SOLE executable
add_executable(SOLE ${SOLE_SOURCES})
target_link_directories(SOLE PRIVATE ${SystemC_LIBRARY_DIRS})
target_link_libraries(SOLE ${SystemC_LIBRARIES})

# Create MaxUnit test executable
add_executable(MaxUnit_test ${MAXUNIT_TEST_SOURCES})
target_link_directories(MaxUnit_test PRIVATE ${SystemC_LIBRARY_DIRS})
target_link_libraries(MaxUnit_test ${SystemC_LIBRARIES})

# Optional: Add a custom target to run tests
enable_testing()
add_test(NAME MaxUnit COMMAND MaxUnit_test)
