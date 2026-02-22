@echo off
REM Build script for ASCII Encoder and Decoder

echo Building ASCII Encoder and Decoder...
echo.

REM Check if VCPKG_ROOT is set
if not defined VCPKG_ROOT (
    echo ERROR: VCPKG_ROOT environment variable is not set!
    echo Please set it to your vcpkg installation directory.
    echo.
    echo Example: 
    echo   set VCPKG_ROOT=C:\path\to\vcpkg
    echo   or permanently:
    echo   setx VCPKG_ROOT "C:\path\to\vcpkg"
    echo.
    pause
    exit /b 1
)

echo Using vcpkg from: %VCPKG_ROOT%
echo.

REM Create build directory
if not exist build mkdir build
cd build

REM Configure with CMake
echo Configuring project...
cmake .. -DCMAKE_TOOLCHAIN_FILE=%VCPKG_ROOT%/scripts/buildsystems/vcpkg.cmake
if %ERRORLEVEL% neq 0 (
    echo.
    echo ERROR: CMake configuration failed!
    cd ..
    pause
    exit /b 1
)

echo.
echo Building project...
cmake --build . --config Release
if %ERRORLEVEL% neq 0 (
    echo.
    echo ERROR: Build failed!
    cd ..
    pause
    exit /b 1
)

cd ..
echo.
echo ========================================
echo Build completed successfully!
echo ========================================
echo.
echo Output files are written to the out/ directory.
echo.
pause
