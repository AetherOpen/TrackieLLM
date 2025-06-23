# ==============================================================================
# CMakeLists.txt for the Perception Module
# ==============================================================================
#
# This file defines a static library `via_perception` containing all the
# computer vision and AI inference logic. It finds and links against the
# ONNX Runtime library and compiles C++ and optimized Assembly sources.
#

# --- Define the Library Target ---
add_library(via_perception STATIC)

# --- Enable C++, C, and Assembly Languages ---
# We need C for potential interop and ASM for optimizations.
set_target_properties(via_perception PROPERTIES
    LANGUAGES CXX C ASM
)

# --- Find External Dependencies ---
# Find the ONNX Runtime package. This assumes onnxruntime has been built
# and installed in a location CMake can find it (e.g., via CMAKE_PREFIX_PATH
# or by being a submodule built by the parent CMakeLists.txt).
find_package(onnxruntime REQUIRED)

# --- Collect Source Files ---
set(PERCEPTION_SOURCES
    # C++ implementation files
    src/OnnxRuntimeProcessor.cpp
    src/PerceptionEngine.cpp
    src/Preprocessor.cpp

    # Optimized Assembly files for ARM
    # A more robust check would be `if(CMAKE_SYSTEM_PROCESSOR MATCHES "arm|aarch64")`
    src/optim/preprocess_image_arm.s
    src/optim/postprocess_detect_arm.s
)

# Associate the collected source files with the library target.
target_sources(via_perception
    PRIVATE
        ${PERCEPTION_SOURCES}
)

# --- Specify Include Directories ---
target_include_directories(via_perception
    PUBLIC
        # Allows #include "via/perception/PerceptionEngine.h" from other modules.
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE
        # The source files within this module need access to the ONNX Runtime headers.
        # By making this PRIVATE, we don't expose ONNX headers to modules that
        # link against via_perception, which is good encapsulation.
        ${onnxruntime_INCLUDE_DIRS}
)

# --- Link Against Dependencies ---
target_link_libraries(via_perception
    PUBLIC
        # Any module linking to via_perception will also need to link to onnxruntime.
        # Making this PUBLIC propagates the dependency.
        onnxruntime

        # The perception module uses data structures from the shared module.
        # This dependency will also be propagated to the final executable.
        via_shared
)

# --- C++ Standard ---
set_target_properties(via_perception PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)

# --- Installation Rules ---
# Install the public headers for development against pre-compiled libraries.
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/
    DESTINATION include
)
