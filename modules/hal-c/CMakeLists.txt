# ==============================================================================
# CMakeLists.txt for the Hardware Abstraction Layer (HAL) C Module
# ==============================================================================
#
# This file defines a static library `via_hal` containing the hardware
# abstraction code. It handles platform-specific source file selection
# and the compilation of optimized Assembly routines.
#

# --- Define the Library Target ---
# We create a static library that will be linked into the main executable.
add_library(via_hal STATIC)

# --- Enable C and Assembly Languages ---
# We need to explicitly enable ASM for this target.
set_target_properties(via_hal PROPERTIES
    LANGUAGES C ASM
)

# --- Collect Source Files ---
# Start with a list of common source files.
set(HAL_SOURCES
    # Currently no common C files, but this is where they would go.
)

# --- Platform-Specific Source Selection ---
# Include the correct implementation file based on the operating system.
if(WIN32)
    # On Windows, use the Media Foundation and Waveform API implementations.
    list(APPEND HAL_SOURCES
        src/camera_windows.c
        src/audio_windows.c
    )
elseif(UNIX)
    # On Unix-like systems (Linux), use V4L2 and ALSA implementations.
    list(APPEND HAL_SOURCES
        src/camera_linux.c
        src/audio_linux.c
    )
else()
    # For any other unsupported OS, provide stub implementations.
    message(WARNING "HAL: Unsupported OS. Using stub implementations.")
    list(APPEND HAL_SOURCES
        src/camera_stub.c
        src/audio_stub.c
    )
endif()

# --- Add Optimized Assembly Files ---
# These files are platform-specific (ARM), but we can use CMake variables
# set at the top level to conditionally include them.
# For simplicity here, we assume they are always included for ARM targets.
# A more robust check would be `if(CMAKE_SYSTEM_PROCESSOR MATCHES "arm|aarch64")`
list(APPEND HAL_SOURCES
    src/optim/audio_filter_arm.s
)

# Associate the collected source files with the library target.
target_sources(via_hal
    PRIVATE
        ${HAL_SOURCES}
)

# --- Specify Include Directories ---
# This makes the public headers of this module available to other modules
# that link against it (like `core`).
target_include_directories(via_hal
    PUBLIC
        # Allows #include "via/hal/hal.h" from other modules.
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# --- Installation Rules ---
# Since this is a static library, we typically don't need to install it
# directly, as it will be part of the final executable. However, we must
# install its public headers so that if someone wants to develop against
# a pre-compiled version of TrackieLLM, they can.
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/
    DESTINATION include
)
