@echo off
rem ==============================================================================
rem TrackieLLM Windows Dependency Installer (install_deps.bat)
rem ==============================================================================
rem
rem This script helps set up the development environment for TrackieLLM on Windows.
rem It checks for required tools and uses vcpkg and pip to install dependencies.
rem
rem It is highly recommended to run this script in a "Developer Command Prompt
rem for Visual Studio" to ensure all C++ build tools are in the PATH.
rem
rem ==============================================================================

setlocal

rem --- Configuration ---
set SCRIPT_DIR=%~dp0
set PROJECT_ROOT=%SCRIPT_DIR%..\..
for %%i in ("%PROJECT_ROOT%") do set PROJECT_ROOT=%%~fi

set VCPKG_DIR=%PROJECT_ROOT%\third_party\vcpkg
set VCPKG_GIT_URL=https://github.com/microsoft/vcpkg.git

echo --- TrackieLLM Windows Dependency Setup ---
echo.
echo This script will guide you through installing the necessary dependencies.
echo.

rem ==============================================================================
rem 1. Prerequisite Tool Check
rem ==============================================================================

echo [1/5] Checking for prerequisite tools...

rem --- Check for Visual Studio ---
if not defined VSCMD_ARG_TGT_ARCH (
    echo WARNING: You do not appear to be in a Developer Command Prompt for VS.
    echo          C++ build tools might not be found.
    echo          Please install Visual Studio 2019 or later with the
    echo          "Desktop development with C++" workload.
    echo.
) else (
    echo   - Visual Studio Developer Environment: Found.
)

rem --- Check for Git ---
where /q git
if %errorlevel% neq 0 (
    echo ERROR: Git is not found in your PATH.
    echo Please install Git from https://git-scm.com/ and ensure it's added to the PATH.
    exit /b 1
)
echo   - Git: Found.

rem --- Check for CMake ---
where /q cmake
if %errorlevel% neq 0 (
    echo ERROR: CMake is not found in your PATH.
    echo Please install CMake from https://cmake.org/download/ and ensure it's added to the PATH.
    exit /b 1
)
echo   - CMake: Found.

rem --- Check for Python ---
where /q python
if %errorlevel% neq 0 (
    echo ERROR: Python is not found in your PATH.
    echo Please install Python 3.8+ from https://www.python.org/ and ensure it's added to the PATH.
    exit /b 1
)
echo   - Python: Found.

echo Prerequisite check complete.
echo.

rem ==============================================================================
rem 2. Setup vcpkg
rem ==============================================================================

echo [2/5] Setting up vcpkg for C++ dependencies...

if not exist "%VCPKG_DIR%" (
    echo   - vcpkg not found. Cloning repository...
    git clone "%VCPKG_GIT_URL%" "%VCPKG_DIR%"
) else (
    echo   - vcpkg directory already exists.
)

if not exist "%VCPKG_DIR%\vcpkg.exe" (
    echo   - Bootstrapping vcpkg...
    call "%VCPKG_DIR%\bootstrap-vcpkg.bat"
) else (
    echo   - vcpkg is already bootstrapped.
)
echo vcpkg setup complete.
echo.

rem ==============================================================================
rem 3. Install C++ Dependencies via vcpkg
rem ==============================================================================

echo [3/5] Installing C++ dependencies via vcpkg...
echo This may take a very long time as it will compile libraries from source.
echo.

rem The vcpkg triplet for 64-bit Windows.
set VCPKG_TRIPLET=x64-windows

rem Install ONNX Runtime. vcpkg handles all of its transitive dependencies.
rem We use `vcpkg integrate install` to make MSBuild/CMake find the packages automatically.
call "%VCPKG_DIR%\vcpkg" integrate install
call "%VCPKG_DIR%\vcpkg" install onnxruntime --triplet %VCPKG_TRIPLET%

rem Note: llama.cpp is handled as a git submodule, so we don't install it via vcpkg.
rem If other dependencies like ALSA were needed on Windows, they would be added here.

echo C++ dependencies installed.
echo.

rem ==============================================================================
rem 4. Install Python Dependencies
rem ==============================================================================

echo [4/5] Installing Python dependencies for scripts...

set REQUIREMENTS_FILE=%SCRIPT_DIR%\requirements.txt
if not exist "%REQUIREMENTS_FILE%" (
    echo WARNING: requirements.txt not found at %REQUIREMENTS_FILE%.
) else (
    python -m pip install -r "%REQUIREMENTS_FILE%"
)
echo Python dependencies installed.
echo.

rem ==============================================================================
rem 5. Download AI Models
rem ==============================================================================

echo [5/5] Downloading required AI models...
set DOWNLOAD_SCRIPT=%SCRIPT_DIR%\download_models.py
if not exist "%DOWNLOAD_SCRIPT%" (
    echo ERROR: Download script not found at %DOWNLOAD_SCRIPT%.
    exit /b 1
)
python "%DOWNLOAD_SCRIPT%"
echo Model download process finished.
echo.

echo ---
echo SUCCESS!
echo ---
echo The development environment has been set up.
echo.
echo To build the project, you can now run the following commands from the
echo project root directory in a Developer Command Prompt:
echo.
echo   mkdir build
echo   cd build
echo   cmake .. -DCMAKE_TOOLCHAIN_FILE=%VCPKG_DIR%/scripts/buildsystems/vcpkg.cmake
echo   cmake --build . --config Release
echo.

endlocal
exit /b 0
