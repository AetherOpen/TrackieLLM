# ==============================================================================
# TrackieLLM - Root CMakeLists.txt
# ==============================================================================
#
# This is the main entry point for the CMake build system. It configures the
# project, finds/builds third-party dependencies, and includes all the
# internal modules.
#

# --- Project Definition ---
# Set the minimum required version of CMake.
cmake_minimum_required(VERSION 3.16)

# Define the project name, version, and languages.
# Enabling C, CXX, and ASM at the top level makes them available to all sub-targets.
project(TrackieLLM
    VERSION 1.0.0
    LANGUAGES CXX C ASM
)

# --- Global Build Configuration ---
# Set a default build type if none was specified.
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type (Release, Debug, RelWithDebInfo, MinSizeRel)" FORCE)
endif()

# Use modern C++ features and disable vendor-specific extensions for portability.
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# --- Third-Party Dependency Management (via Git Submodules) ---
# This is the most robust way to handle dependencies. We add the submodules'
# source trees to our build, ensuring they are compiled with compatible settings.

# Add the llama.cpp submodule to the build.
# Its CMakeLists.txt will create a `llama` library target.
# We turn off options we don't need to speed up the build.
add_subdirectory(third_party/llama.cpp)
set_target_properties(llama PROPERTIES FOLDER "third_party")

# Add the onnxruntime submodule to the build.
# This is more complex as we need to run their build script.
# A simpler approach is to require it to be pre-installed.
# For this example, we'll assume it's pre-installed and findable by CMake.
# The robust submodule approach would involve `ExternalProject_Add`.
#
# find_package(onnxruntime REQUIRED)
#
# For a self-contained build, you would typically do:
# include(ExternalProject)
# ExternalProject_Add(onnxruntime_project
#     SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/third_party/onnxruntime
#     ... build commands ...
# )
# For simplicity, we will rely on find_package. Ensure CMAKE_PREFIX_PATH is set
# if onnxruntime is installed in a non-standard location.
find_package(onnxruntime REQUIRED)


# --- Internal Module Processing ---
# Use add_subdirectory() to descend into each module's directory and
# process its CMakeLists.txt file. The order matters if modules depend
# on each other. We build from the most independent to the most dependent.

# 1. Shared Module (no dependencies)
add_subdirectory(modules/shared)

# 2. Config Loader Module (no internal dependencies)
add_subdirectory(modules/config_loader-rs)

# 3. HAL Module (depends on system libraries, but no internal modules)
add_subdirectory(modules/hal-c)

# 4. Perception Module (depends on onnxruntime and shared)
add_subdirectory(modules/perception)

# 5. Reasoning Module (depends on llama.cpp and shared)
add_subdirectory(modules/reasoning)

# 6. Core Module (depends on all other modules)
# This is the last module as it creates the final executable and links
# everything together.
add_subdirectory(modules/core)

# --- CTest Configuration (Optional but Recommended) ---
# Enable testing for the entire project.
enable_testing()

# You can add test subdirectories here if you have them.
# add_subdirectory(tests)

# --- Final Output ---
message(STATUS "TrackieLLM project configured.")
message(STATUS "Build Type: ${CMAKE_BUILD_TYPE}")
message(STATUS "Build Directory: ${CMAKE_BINARY_DIR}")
message(STATUS "Run 'cmake --build ${CMAKE_BINARY_DIR}' to compile.")
