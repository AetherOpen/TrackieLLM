#!/bin/bash

# ==============================================================================
# TrackieLLM OS Packaging Script (package_os.sh)
# ==============================================================================
#
# This script automates the process of building the TrackieLLM project and
# packaging all necessary artifacts into a distributable tarball for deployment
# on the target OS (e.g., Raspbian on a Raspberry Pi).
#
# Usage:
#   ./package_os.sh [version]
#
#   [version] - Optional. A version string (e.g., "1.0.1") to append to the
#               package name. If not provided, it defaults to the current date.
#

# --- Configuration ---
set -e # Exit immediately if a command exits with a non-zero status.
set -u # Treat unset variables as an error.
set -o pipefail # Causes a pipeline to return the exit status of the last command
                # that exited with a non-zero status.

# Get the directory of the script itself to reliably find the project root.
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )"
PROJECT_ROOT="$( cd "${SCRIPT_DIR}/../../" &> /dev/null && pwd )"

# --- Variables ---
VERSION=${1:-$(date +%Y%m%d)} # Use provided version or default to YYYYMMDD
BUILD_DIR="${PROJECT_ROOT}/build"
PACKAGE_NAME="trackiellm-os-${VERSION}"
PACKAGE_DIR="${PROJECT_ROOT}/${PACKAGE_NAME}"
FINAL_ARCHIVE="${PROJECT_ROOT}/${PACKAGE_NAME}.tar.gz"

# --- Main Logic ---

echo "--- Starting TrackieLLM OS Packaging ---"
echo "Project Root: ${PROJECT_ROOT}"
echo "Package Version: ${VERSION}"

# 1. Clean up previous builds and packages to ensure a fresh start.
echo "[1/5] Cleaning up previous artifacts..."
rm -rf "${BUILD_DIR}"
rm -rf "${PACKAGE_DIR}"
rm -f "${PROJECT_ROOT}/trackiellm-os-*.tar.gz"
echo "Cleanup complete."

# 2. Configure and build the project in Release mode.
# We are building the project from scratch to ensure it's clean.
echo "[2/5] Configuring and building the project in Release mode..."
cmake -S "${PROJECT_ROOT}" -B "${BUILD_DIR}" -DCMAKE_BUILD_TYPE=Release
cmake --build "${BUILD_DIR}" --parallel $(nproc)
echo "Build complete."

# 3. Create the packaging directory structure.
echo "[3/5] Creating package directory: ${PACKAGE_DIR}"
mkdir -p "${PACKAGE_DIR}"
echo "Package directory created."

# 4. Install the project artifacts into the clean package directory.
# The `cmake --install` command uses the rules defined in our CMakeLists.txt
# files. The `--prefix` option redirects the installation from a system-wide
# location (like /usr/local) to our local package directory.
echo "[4/5] Installing artifacts into package directory..."
cmake --install "${BUILD_DIR}" --prefix "${PACKAGE_DIR}"
echo "Installation complete."

# 5. Copy additional runtime scripts or service files.
# These are files needed for running the application on the target system
# but are not part of the CMake install rules (e.g., systemd service files,
# startup scripts).
echo "[5/5] Copying runtime scripts..."
# Example: Copying a startup script.
# cp "${PROJECT_ROOT}/scripts/TrackieOS/start.sh" "${PACKAGE_DIR}/bin/"

# Example: Copying a systemd service file for auto-starting the application.
# mkdir -p "${PACKAGE_DIR}/etc/systemd/system"
# cp "${PROJECT_ROOT}/scripts/deployment/trackiellm.service" "${PACKAGE_DIR}/etc/systemd/system/"

echo "Runtime scripts copied."


# --- Finalization ---
echo "--- Packaging complete. Creating final tarball... ---"
# Create the final .tar.gz archive.
# The -C flag tells tar to change to the specified directory before archiving,
# which prevents the archive from containing the full path from the root.
tar -C "${PROJECT_ROOT}" -czvf "${FINAL_ARCHIVE}" "${PACKAGE_NAME}"

echo ""
echo "SUCCESS!"
echo "Package created at: ${FINAL_ARCHIVE}"
echo ""
echo "To deploy, copy this file to the target device and run:"
echo "  tar -xzvf ${PACKAGE_NAME}.tar.gz"
echo "  cd ${PACKAGE_NAME}"
echo "  sudo ./install.sh  (if an installer script is provided)"
echo ""

# Clean up the temporary package directory after creating the archive.
rm -rf "${PACKAGE_DIR}"

exit 0
