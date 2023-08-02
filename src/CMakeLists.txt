# Allow public access to the header files in the directory
target_include_directories(ablateFlameGenerator PUBLIC
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>"
        "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>")

# Tag the version file
configure_file (
        "version.h.in"
        "version.h"
)

# add the new version.h to the include directories
target_include_directories(ablateFlameGenerator PRIVATE ${CMAKE_CURRENT_BINARY_DIR})

target_sources(ablateFlameGenerator
        PRIVATE
        steadyStateStepper.cpp
        variableChange.cpp

        PUBLIC
        steadyStateStepper.hpp
        convergenceCriteria.hpp
        variableChange.hpp
)
