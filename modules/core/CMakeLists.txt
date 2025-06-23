# ==============================================================================
# CMakeLists.txt for the Core Module
# ==============================================================================
#
# This file defines the final executable target `trackiellm` and links it
# against all the other module libraries (perception, reasoning, hal, etc.).
#

# --- Define the Executable Target ---
# This creates the main application executable.
add_executable(trackiellm)

# --- Specify Source Files ---
# List all the .cpp files that need to be compiled for this executable.
target_sources(trackiellm
    PRIVATE
        src/App.cpp
        src/main.cpp # The main entry point of the application
)

# --- Specify Include Directories ---
# The target needs to know where to find its own header files.
target_include_directories(trackiellm
    PUBLIC
        # Allows #include "via/core/App.h"
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# --- Link Against Module Libraries ---
# This is the most critical step. We link our executable against the library
# targets defined in the other modules' CMakeLists.txt files.
#
# Modern CMake handles transitive dependencies automatically. For example, since
# `via_perception` links to `onnxruntime`, and we link to `via_perception`,
# CMake ensures that `trackiellm` is also linked to `onnxruntime`.
target_link_libraries(trackiellm
    PRIVATE
        # Our internal module libraries
        via_perception
        via_reasoning
        via_shared
        via_hal
        via_config
)

# --- Platform-Specific Link Dependencies ---
# Some libraries are required only on specific operating systems.
if(UNIX AND NOT APPLE)
    # On Linux, we need to explicitly link against the pthreads library for std::thread
    # and the ALSA library for the audio HAL implementation.
    find_package(Threads REQUIRED)
    find_package(ALSA) # ALSA is used by the audio HAL

    target_link_libraries(trackiellm
        PRIVATE
            Threads::Threads
            # Only link against ALSA if it was found by CMake
            if(ALSA_FOUND)
                ALSA::ALSA
            endif()
    )
endif()

# --- Installation Rules ---
# These rules define what happens when `cmake --install .` is run.
# This is essential for creating a deployable package.

# Install the final executable to the `bin` directory of the install prefix.
install(TARGETS trackiellm
    RUNTIME DESTINATION bin
)

# Install the entire `assets` directory (models, configs, sounds) to the
# root of the install prefix, so the executable can find them at runtime.
# The path is relative to this CMakeLists.txt file.
install(DIRECTORY ${CMAKE_SOURCE_DIR}/assets/
    DESTINATION .
)

# Install the default configuration files into the `config` directory
# of the install prefix.
install(DIRECTORY ${CMAKE_SOURCE_DIR}/config/
    DESTINATION .
)

# --- C++ Standard ---
# Enforce C++17 for this target.
set_target_properties(trackiellm PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)
