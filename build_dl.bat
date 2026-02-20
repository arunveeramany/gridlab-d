@echo off
setlocal EnableExtensions

rem ===== Project & paths =====
set "PROJECT_NAME=gridlabd-cpp23-arm64"
set "SRC_DIR=C:\Arun\conda_projects\%PROJECT_NAME%"

rem ===== Target and toolset =====
rem Building x64 binaries on your Intel machine:
set "ARCH=x64"
set "VCPKG_TRIPLET=x64-windows"

rem Host toolset selection for the VS generator (use x64 host tools)
set "HOST_TOOLSET=host=x64"

rem Avoid '=' in folder names
set "HOST_TAG=host-x64"
set "BUILD_DIR=%SRC_DIR%\out\build\vs2022-%ARCH%-%HOST_TAG%"

rem Generator & VS Community vcpkg toolchain (manifest mode)
set "GENERATOR=Visual Studio 17 2022"
set "TOOLCHAIN=C:\Program Files\Microsoft Visual Studio\2022\Community\VC\vcpkg\scripts\buildsystems\vcpkg.cmake"
set "INSTALL_PREFIX=%USERPROFILE%\.temp\GridLAB-D"

rem ===== CLI switches =====
set "DO_CLEAN=0"
set "DO_REBUILD=0"
if /I "%~1"=="--clean"   set "DO_CLEAN=1"
if /I "%~1"=="--rebuild" set "DO_REBUILD=1" & set "DO_CLEAN=1"

echo === GridLAB-D build configuration ===
echo PROJECT_NAME   = %PROJECT_NAME%
echo SRC_DIR        = %SRC_DIR%
echo BUILD_DIR      = %BUILD_DIR%
echo GENERATOR      = %GENERATOR%
echo ARCH           = %ARCH%
echo HOST_TOOLSET   = %HOST_TOOLSET%
echo VCPKG_TRIPLET  = %VCPKG_TRIPLET%
echo TOOLCHAIN      = %TOOLCHAIN%
echo INSTALL_PREFIX = %INSTALL_PREFIX%
echo.

rem ===== Basic checks =====
if not exist "%SRC_DIR%\CMakeLists.txt" (
  echo [ERROR] CMakeLists.txt not found in "%SRC_DIR%".
  exit /b 1
)

if not exist "%TOOLCHAIN%" (
  echo [ERROR] VS Community vcpkg toolchain not found:
  echo         "%TOOLCHAIN%"
  echo         Ensure the "vcpkg package manager" component is installed in Visual Studio 2022.
  exit /b 1
)

rem ===== Optional clean =====
if "%DO_CLEAN%"=="1" (
  if exist "%BUILD_DIR%\CMakeCache.txt" del /f /q "%BUILD_DIR%\CMakeCache.txt"
  if exist "%BUILD_DIR%\CMakeFiles"     rmdir /s /q "%BUILD_DIR%\CMakeFiles"
  echo [CLEAN] Removed CMake cache in "%BUILD_DIR%".
)

rem ===== Configure =====
echo [CONFIGURE] CMake configure...
cmake -S "%SRC_DIR%" ^
  -B "%BUILD_DIR%" ^
  -G "%GENERATOR%" ^
  -A %ARCH% ^
  -T %HOST_TOOLSET% ^
  -DCMAKE_CONFIGURATION_TYPES=Debug ^
  -DVCPKG_TARGET_TRIPLET=%VCPKG_TRIPLET% ^
  -DCMAKE_TOOLCHAIN_FILE="%TOOLCHAIN%" ^
  -DCMAKE_INSTALL_PREFIX="%INSTALL_PREFIX%" ^
  -DGLD_CI_BUILD=ON

if errorlevel 1 (
  echo [ERROR] Configure failed. If it mentions a toolset mismatch, try:
  echo   .\build_dl.bat --clean
  echo or change BUILD_DIR to a different folder.
  exit /b %errorlevel%
)

rem ===== Build =====
if "%DO_REBUILD%"=="1" (
  echo [REBUILD] Rebuilding ALL_BUILD ^(Debug^)...
) else (
  echo [BUILD] Building ALL_BUILD ^(Debug^)...
)
cmake --build "%BUILD_DIR%" --config Debug --target ALL_BUILD

if errorlevel 1 (
  echo [ERROR] Build failed.
  exit /b %errorlevel%
)

echo [SUCCESS] Build completed.
