#!/bin/bash

# ==============================================================================
# TrackieLLM Linux/Debian Dependency Installer (install_deps.sh)
# ==============================================================================
#
# This script installs all necessary dependencies for building and running
# TrackieLLM on a Debian-based Linux system (like Ubuntu or Raspbian OS).
#
# It uses the `apt` package manager for system libraries and `pip` for
# Python script dependencies.
#
# It must be run with sudo privileges to install system packages.
#
# Usage:
#   sudo ./install_deps.sh
#

# --- Configuration ---
set -e # Exit immediately if a command exits with a non-zero status.
set -u # Treat unset variables as an error.
set -o pipefail # Ensure pipeline failures are detected.

# --- Check for Root/Sudo Privileges ---
if [ "$(id -u)" -ne 0 ]; then
  echo "ERROR: This script must be run with sudo privileges to install system packages." >&2
  echo "Please run again using: sudo $0"
  exit 1
fi

echo "--- TrackieLLM Linux Dependency Setup ---"
echo ""

# ==============================================================================
# 1. Update Package Lists
# ==============================================================================
echo "[1/4] Updating package lists with 'apt-get update'..."
apt-get update
echo "Package lists updated."
echo ""

# ==============================================================================
# 2. Install System-Level C/C++ Dependencies
# ==============================================================================
echo "[2/4] Installing essential build tools and libraries via apt..."

# A list of all required packages.
# - build-essential: A meta-package that includes gcc, g++, make, etc.
# - cmake: The build system generator.
# - git: For version control and submodules.
# - python3-pip: For installing Python dependencies.
# - libasound2-dev: ALSA development headers for the audio HAL.
# - libv4l-dev: Video4Linux development headers for the camera HAL.
# - libonnxruntime-dev: Pre-compiled ONNX Runtime library and headers.
#   (Note: This package might not be in default repos. If not, ONNX Runtime
#    must be built from source, which is a more complex process not
#    covered by this simple script).
# - pkg-config: Helps CMake find libraries.
DEPS=(
    build-essential
    cmake
    git
    python3
    python3-pip
    pkg-config
    libasound2-dev
    libv4l-dev
)

# Check for onnxruntime package availability, provide message if not found.
if ! apt-cache show libonnxruntime-dev > /dev/null 2>&1; then
    echo "WARNING: 'libonnxruntime-dev' not found in APT repositories."
    echo "         The CMake build will likely fail unless you have installed"
    echo "         ONNX Runtime manually to a location CMake can find."
    echo "         Continuing installation of other packages..."
else
    DEPS+=(libonnxruntime-dev)
fi

apt-get install -y "${DEPS[@]}"

echo "System dependencies installed."
echo ""

# ==============================================================================
# 3. Install Python Dependencies
# ==============================================================================
echo "[3/4] Installing Python dependencies for scripts via pip..."

# Find the project root directory relative to this script's location.
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )"
PROJECT_ROOT="$( cd "${SCRIPT_DIR}/../../" &> /dev/null && pwd )"
REQUIREMENTS_FILE="${PROJECT_ROOT}/scripts/setup/requirements.txt"

if [ ! -f "$REQUIREMENTS_FILE" ]; then
    echo "WARNING: requirements.txt not found at ${REQUIREMENTS_FILE}."
else
    # Use python3 explicitly.
    python3 -m pip install -r "$REQUIREMENTS_FILE"
fi
echo "Python dependencies installed."
echo ""

# ==============================================================================
# 4. Download AI Models
# ==============================================================================
echo "[4/4] Downloading required AI models..."
DOWNLOAD_SCRIPT="${PROJECT_ROOT}/scripts/setup/download_models.py"

if [ ! -f "$DOWNLOAD_SCRIPT" ]; then
    echo "ERROR: Download script not found at ${DOWNLOAD_SCRIPT}."
    exit 1
fi

# Run the download script as the original user, not as root.
# This prevents the models from being owned by the root user.
if [ -n "${SUDO_USER}" ]; then
    sudo -u "${SUDO_USER}" python3 "$DOWNLOAD_SCRIPT"
else
    python3 "$DOWNLOAD_SCRIPT"
fi
echo "Model download process finished."
echo ""


echo "---"
echo "SUCCESS!"
echo "---"
echo "The development environment has been set up."
echo "You can now build the project by running the following commands from the project root:"
echo ""
echo "  make"
echo ""

exit 0
