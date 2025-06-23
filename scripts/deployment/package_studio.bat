@echo off
rem ==============================================================================
rem TrackieLLM Studio Packaging Script (package_studio.bat)
rem ==============================================================================
rem
rem This script automates the process of building the TrackieLLM project and
rem packaging all necessary artifacts into a distributable .zip file for use
rem on Windows machines.
rem
rem It assumes that Visual Studio and CMake are installed and available in the
rem system's PATH. It's best run from a "Developer Command Prompt for VS".
rem
rem Usage:
rem   package_studio.bat [version]
rem
rem   [version] - Optional. A version string (e.g., "1.0.1") to append to the
rem               package name. If not provided, it defaults to the current date.
rem

setlocal

rem --- Configuration ---
echo --- Setting up environment ---

rem Find the project root directory relative to this script's location.
set SCRIPT_DIR=%~dp0
set PROJECT_ROOT=%SCRIPT_DIR%..\..
rem Canonicalize the path
for %%i in ("%PROJECT_ROOT%") do set PROJECT_ROOT=%%~fi

rem --- Variables ---
rem Set version from command line argument or default to date YYYY-MM-DD
set VERSION=%1
if not defined VERSION (
    for /f "tokens=2 delims==" %%a in ('wmic OS Get localdatetime /value') do set "dt=%%a"
    set "YYYY=%dt:~0,4%"
    set "MM=%dt:~4,2%"
    set "DD=%dt:~6,2%"
    set VERSION=%YYYY%-%MM%-%DD%
)

set BUILD_DIR=%PROJECT_ROOT%\build
set PACKAGE_NAME=trackiellm-studio-%VERSION%
set PACKAGE_DIR=%PROJECT_ROOT%\%PACKAGE_NAME%
set FINAL_ARCHIVE=%PROJECT_ROOT%\%PACKAGE_NAME%.zip

rem --- Main Logic ---

echo --- Starting TrackieLLM Studio Packaging ---
echo Project Root: %PROJECT_ROOT%
echo Package Version: %VERSION%

rem 1. Clean up previous builds and packages.
echo [1/6] Cleaning up previous artifacts...
if exist "%BUILD_DIR%" (
    echo Deleting old build directory...
    rmdir /s /q "%BUILD_DIR%"
)
if exist "%PACKAGE_DIR%" (
    echo Deleting old package directory...
    rmdir /s /q "%PACKAGE_DIR%"
)
if exist "%PROJECT_ROOT%\trackiellm-studio-*.zip" (
    echo Deleting old zip archives...
    del "%PROJECT_ROOT%\trackiellm-studio-*.zip"
)
echo Cleanup complete.

rem 2. Configure and build the project in Release mode.
echo [2/6] Configuring and building the project in Release mode...
cmake -S "%PROJECT_ROOT%" -B "%BUILD_DIR%" -DCMAKE_BUILD_TYPE=Release
if %errorlevel% neq 0 (
    echo CMake configuration failed!
    exit /b 1
)

cmake --build "%BUILD_DIR%" --config Release --parallel
if %errorlevel% neq 0 (
    echo Build failed!
    exit /b 1
)
echo Build complete.

rem 3. Create the packaging directory structure.
echo [3/6] Creating package directory: %PACKAGE_DIR%
mkdir "%PACKAGE_DIR%"
echo Package directory created.

rem 4. Install the project artifacts into the clean package directory.
echo [4/6] Installing artifacts into package directory...
cmake --install "%BUILD_DIR%" --config Release --prefix "%PACKAGE_DIR%"
if %errorlevel% neq 0 (
    echo Installation failed!
    exit /b 1
)
echo Installation complete.

rem 5. Copy necessary runtime DLLs.
rem This is a CRITICAL step on Windows. The installed executable needs its
rem runtime dependencies (DLLs) to be in the same directory.
echo [5/6] Copying required runtime DLLs...

rem Find and copy the ONNX Runtime DLL.
rem This path needs to be adjusted based on where onnxruntime is installed.
rem This example assumes a vcpkg-style layout.
set ONNXRUNTIME_DLL_PATH=%PROJECT_ROOT%\third_party\onnxruntime\lib\onnxruntime.dll
if exist "%ONNXRUNTIME_DLL_PATH%" (
    echo Found onnxruntime.dll, copying...
    copy "%ONNXRUNTIME_DLL_PATH%" "%PACKAGE_DIR%\bin\"
) else (
    echo WARNING: onnxruntime.dll not found at expected path. The package may not run.
)

rem Find and copy llama.cpp DLLs if it was built as a dynamic library.
rem If built statically, this step is not needed.
rem set LLAMA_DLL_PATH=...
rem if exist "%LLAMA_DLL_PATH%" (
rem     copy "%LLAMA_DLL_PATH%" "%PACKAGE_DIR%\bin\"
rem )

echo DLLs copied.

rem 6. Create the final .zip archive.
echo [6/6] Creating final zip archive...
rem Windows does not have a native `zip` command. We use `tar` which is
rem available with modern Windows 10/11 or Git for Windows.
tar -a -c -f "%FINAL_ARCHIVE%" -C "%PROJECT_ROOT%" "%PACKAGE_NAME%"
if %errorlevel% neq 0 (
    echo Failed to create zip archive. Make sure 'tar' is in your PATH.
    exit /b 1
)

echo.
echo SUCCESS!
echo Package created at: %FINAL_ARCHIVE%
echo.
echo To run, extract the zip file and execute the .bat script inside.
echo.

rem Clean up the temporary package directory.
rmdir /s /q "%PACKAGE_DIR%"

endlocal
exit /b 0
