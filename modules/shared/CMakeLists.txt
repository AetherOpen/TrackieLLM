# ==============================================================================
# CMakeLists.txt for the Shared Utilities Module
# ==============================================================================
#
# This file defines an INTERFACE library `via_shared`.
#
# An INTERFACE library is a special type of target in CMake that doesn't
# compile any source files itself. Its purpose is to group properties,
# like include directories, that can be inherited by other targets that
# "link" to it.
#
# This is the perfect pattern for a header-only library like our `shared`
# module, which only contains data structures and templates.
#

# --- Define the Interface Library Target ---
# This creates a target that other modules can link against to get its
# public properties (like include directories).
add_library(via_shared INTERFACE)

# --- Specify Public Include Directories ---
# This is the core of an interface library. We specify that any target
# linking against `via_shared` should have this directory added to its
# list of include paths.
target_include_directories(via_shared
    INTERFACE
        # Allows #include "via/shared/DataStructures.h" from other modules.
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# --- Installation Rules ---
# We must install the headers so that other projects can be developed
# against a pre-compiled version of TrackieLLM.
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/
    DESTINATION include
)
