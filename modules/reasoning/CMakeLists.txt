# ==============================================================================
# CMakeLists.txt for the Reasoning Module
# ==============================================================================
#
# This file defines a static library `via_reasoning` containing the logic for
# interacting with the Large Language Model. It finds and links against the
# llama.cpp library.
#

# --- Define the Library Target ---
add_library(via_reasoning STATIC)

# --- Find External Dependencies ---
# Find the llama.cpp package. This assumes llama.cpp has been built as a
# library and installed, or is being built as part of this project via a
# submodule and `add_subdirectory`. The target it exports is typically `llama`.
#
# We mark it as REQUIRED because the reasoning module is useless without it.
find_package(llama REQUIRED)

# --- Collect Source Files ---
set(REASONING_SOURCES
    src/LlmInterpreter.cpp
)

# Associate the collected source files with the library target.
target_sources(via_reasoning
    PRIVATE
        ${REASONING_SOURCES}
)

# --- Specify Include Directories ---
target_include_directories(via_reasoning
    PUBLIC
        # Allows #include "via/reasoning/LlmInterpreter.h" from other modules.
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE
        # The source files within this module need access to the llama.cpp headers.
        # We get the include directory from the `llama` target's properties.
        # This is a modern and robust way to get dependency paths.
        $<TARGET_PROPERTY:llama,INTERFACE_INCLUDE_DIRECTORIES>
)

# --- Link Against Dependencies ---
target_link_libraries(via_reasoning
    PUBLIC
        # Any module linking to via_reasoning will also need to link to llama.
        # Making this PUBLIC propagates the dependency to the final executable.
        llama

        # The reasoning module uses the SafeQueue from the shared module.
        via_shared
)

# --- C++ Standard ---
set_target_properties(via_reasoning PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)

# --- Installation Rules ---
# Install the public headers for development against pre-compiled libraries.
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/
    DESTINATION include
)
